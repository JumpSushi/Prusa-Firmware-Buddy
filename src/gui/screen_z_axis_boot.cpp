#include "screen_z_axis_boot.hpp"

#include <cstdio>
#include <img_resources.hpp>
#include <marlin_client.hpp>
#include <sound.hpp>
#include <utils/string_builder.hpp>
#include <gui_time.hpp>

ScreenZAxisBoot::ScreenZAxisBoot()
    : screen_t()
    , phase(Phase::STANDING)
    , target_z(static_cast<float>(round(marlin_vars().logical_pos[2])))
    , standing_height(0.0f)
    , base_height(0.0f)
    , direction(0)
    , countdown_start(0)
    , status_buf {}
    , header    (this, _("DESK CONTROLLER"))
    , lbl_step  (this, rc_step,   is_multiline::no,  is_closed_on_click_t::no,
                 string_view_utf8::MakeCPUFLASH(""))
    , lbl_title (this, rc_title,  is_multiline::no,  is_closed_on_click_t::no,
                 string_view_utf8::MakeCPUFLASH(""))
    , lbl_instr (this, rc_instr,  is_multiline::yes, is_closed_on_click_t::no,
                 string_view_utf8::MakeCPUFLASH(""))
    , icon      (this, rc_icon,   &img::turn_knob_81x55)
    , lbl_dn    (this, rc_lbl_dn, is_multiline::no,  is_closed_on_click_t::no,
                 string_view_utf8::MakeCPUFLASH("DOWN"))
    , lbl_up    (this, rc_lbl_up, is_multiline::no,  is_closed_on_click_t::no,
                 string_view_utf8::MakeCPUFLASH("UP"))
    , arrows    (this, pt_arrows, { 0, 6, 0, 6 })
    , lbl_status(this, rc_status, is_multiline::no,  is_closed_on_click_t::no,
                 string_view_utf8::MakeCPUFLASH(""))
    , lbl_hint  (this, rc_hint,   is_multiline::yes, is_closed_on_click_t::no,
                 string_view_utf8::MakeCPUFLASH(""))
{
    header.SetIcon(&img::z_axis_16x16);

    lbl_step.set_font(Font::small);
    lbl_step.SetAlignment(Align_t::Center());
    lbl_step.SetTextColor(COLOR_GRAY);

    lbl_title.set_font(Font::big);
    lbl_title.SetAlignment(Align_t::Center());
    lbl_title.SetTextColor(COLOR_ORANGE);

    lbl_instr.set_font(Font::normal);
    lbl_instr.SetAlignment(Align_t::Center());

    lbl_dn.set_font(GuiDefaults::FontBig);
    lbl_dn.SetAlignment(Align_t::RightCenter());

    lbl_up.set_font(GuiDefaults::FontBig);
    lbl_up.SetAlignment(Align_t::LeftCenter());

    arrows.SetState(WindowArrows::State_t::undef);

    lbl_status.set_font(Font::normal);
    lbl_status.SetAlignment(Align_t::Center());
    lbl_status.SetTextColor(COLOR_ORANGE);

    lbl_hint.set_font(Font::small);
    lbl_hint.SetAlignment(Align_t::Center());
    lbl_hint.SetTextColor(COLOR_GRAY);

    refresh_display();
}

void ScreenZAxisBoot::issue_jog(float z) {
    ArrayStringBuilder<40> sb;
    sb.append_printf("G123 Z%.2f F3000", static_cast<double>(z));
    marlin_client::gcode_try(sb.str());
}

void ScreenZAxisBoot::update_arrows() {
    if (direction > 0) {
        arrows.SetState(WindowArrows::State_t::up);
    } else if (direction < 0) {
        arrows.SetState(WindowArrows::State_t::down);
    } else {
        arrows.SetState(WindowArrows::State_t::undef);
    }
}

void ScreenZAxisBoot::update_countdown_label() {
    const uint32_t elapsed   = gui::GetTick() - countdown_start;
    const int      secs_left = (elapsed < COUNTDOWN_MS)
        ? static_cast<int>((COUNTDOWN_MS - elapsed + 999u) / 1000u)
        : 0;
    snprintf(status_buf, sizeof(status_buf), "Rising in %ds", secs_left);
    lbl_status.SetText(string_view_utf8::MakeRAM(status_buf));
    lbl_status.Invalidate();
}

void ScreenZAxisBoot::refresh_display() {
    switch (phase) {
    case Phase::STANDING:
        lbl_step.SetText(string_view_utf8::MakeCPUFLASH("-- Step 1 of 2 --"));
        lbl_title.SetText(string_view_utf8::MakeCPUFLASH("STANDING HEIGHT"));
        lbl_instr.SetText(string_view_utf8::MakeCPUFLASH(
            "Move Z to where the desk\nshould be when STANDING.\nPress knob to confirm."));
        lbl_status.SetText(string_view_utf8::MakeCPUFLASH(""));
        lbl_hint.SetText(string_view_utf8::MakeCPUFLASH(
            "Spin to latch direction.\nSpin opposite to stop."));
        break;
    case Phase::BASE:
        lbl_step.SetText(string_view_utf8::MakeCPUFLASH("-- Step 2 of 2 --"));
        lbl_title.SetText(string_view_utf8::MakeCPUFLASH("SITTING HEIGHT"));
        lbl_instr.SetText(string_view_utf8::MakeCPUFLASH(
            "Lower Z to where the desk\nshould be when SITTING.\nPress knob to confirm."));
        snprintf(status_buf, sizeof(status_buf), "Standing height saved");
        lbl_status.SetText(string_view_utf8::MakeRAM(status_buf));
        lbl_hint.SetText(string_view_utf8::MakeCPUFLASH(
            "Spin to latch direction.\nSpin opposite to stop."));
        break;
    case Phase::COUNTDOWN:
        lbl_step.SetText(string_view_utf8::MakeCPUFLASH(""));
        lbl_title.SetText(string_view_utf8::MakeCPUFLASH("SETUP COMPLETE"));
        lbl_instr.SetText(string_view_utf8::MakeCPUFLASH(
            "Desk will rise automatically.\nSpin to adjust target.\nPress knob to restart."));
        lbl_hint.SetText(string_view_utf8::MakeCPUFLASH(""));
        update_countdown_label();
        break;
    case Phase::MOVING:
        lbl_step.SetText(string_view_utf8::MakeCPUFLASH(""));
        lbl_title.SetText(string_view_utf8::MakeCPUFLASH("RISING"));
        lbl_instr.SetText(string_view_utf8::MakeCPUFLASH("Moving to standing height..."));
        lbl_status.SetText(string_view_utf8::MakeCPUFLASH(""));
        lbl_hint.SetText(string_view_utf8::MakeCPUFLASH(""));
        break;
    }
    lbl_step.Invalidate();
    lbl_title.Invalidate();
    lbl_instr.Invalidate();
    lbl_hint.Invalidate();
    update_arrows();
}

void ScreenZAxisBoot::windowEvent(window_t *sender, GUI_event_t event, void *param) {
    switch (event) {

    case GUI_event_t::CLICK:
        switch (phase) {
        case Phase::STANDING:
            standing_height = target_z;
            direction       = 0;
            phase           = Phase::BASE;
            refresh_display();
            Sound_Play(eSOUND_TYPE::ButtonEcho);
            break;
        case Phase::BASE:
            base_height     = target_z;
            direction       = 0;
            countdown_start = gui::GetTick_ForceActualization();
            phase           = Phase::COUNTDOWN;
            refresh_display();
            Sound_Play(eSOUND_TYPE::ButtonEcho);
            break;
        case Phase::COUNTDOWN:
            direction = 0;
            phase     = Phase::STANDING;
            refresh_display();
            Sound_Play(eSOUND_TYPE::ButtonEcho);
            break;
        case Phase::MOVING:
            break;
        }
        break;

    // Latched direction: spinning UP latches up; spinning DOWN stops if going up,
    // then latches down on the next input.
    case GUI_event_t::ENC_UP:
        if (direction >= 0) {
            direction = 1; // already stopped or going up: latch up
        } else {
            direction = 0; // was going down: first input stops it
        }
        update_arrows();
        break;

    case GUI_event_t::ENC_DN:
        if (direction <= 0) {
            direction = -1; // already stopped or going down: latch down
        } else {
            direction = 0; // was going up: first input stops it
        }
        update_arrows();
        break;

    case GUI_event_t::LOOP: {
        // Continuously issue G123 in the latched direction every tick
        if (direction != 0 && phase != Phase::MOVING) {
            target_z += static_cast<float>(direction) * STEP_MM;
            issue_jog(target_z);

            if (phase == Phase::COUNTDOWN) {
                standing_height = target_z;
                countdown_start = gui::GetTick();
            }
        }

        if (phase == Phase::COUNTDOWN) {
            const uint32_t elapsed = gui::GetTick() - countdown_start;
            if (elapsed >= COUNTDOWN_MS) {
                direction = 0;
                issue_jog(standing_height);
                phase = Phase::MOVING;
                refresh_display();
            } else {
                update_countdown_label();
            }
        }
        break;
    }

    default:
        screen_t::windowEvent(sender, event, param);
        break;
    }
}
