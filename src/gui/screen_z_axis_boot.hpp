#pragma once
#include "screen.hpp"
#include "window_header.hpp"
#include "window_text.hpp"
#include "window_arrows.hpp"
#include "window_icon.hpp"
#include <guiconfig/guiconfig.h>

/**
 * Z-axis desk controller boot screen.
 *
 * Movement model (latched direction):
 *   - Spin UP   -> latches direction UP;   keeps moving up every LOOP tick.
 *   - Spin DOWN -> if going UP, stops first; next DOWN input starts moving down.
 *   - Direction is held until reversed; no movement when direction == 0.
 *
 * Flow:
 *   STANDING -> BASE -> COUNTDOWN (30 s) -> MOVING
 */
class ScreenZAxisBoot : public screen_t {
public:
    ScreenZAxisBoot();

private:
    static constexpr uint32_t COUNTDOWN_MS = 30'000;
    static constexpr float    STEP_MM      = 0.5f;   ///< mm per tick when latched

    // Layout (MINI 240x320)
    static constexpr Rect16 rc_step    {   0,  27, 240,  18 }; ///< "Step X of 2"
    static constexpr Rect16 rc_title   {   0,  47, 240,  28 }; ///< Phase title
    static constexpr Rect16 rc_instr   {   5,  78, 230,  78 }; ///< Instructions
    // Arrow diagram centred: knob icon 81x55, arrows flanking it
    static constexpr Rect16 rc_icon    {  80, 160,  81,  55 }; ///< turn_knob image
    static constexpr Rect16 rc_lbl_dn  {   4, 177,  70,  21 }; ///< "DOWN" label
    static constexpr Rect16 rc_lbl_up  { 166, 177,  70,  21 }; ///< "UP" label
    static constexpr point_i16_t pt_arrows { 113, 177 }; ///< animated arrows origin
    static constexpr Rect16 rc_status  {   0, 222, 240,  22 }; ///< countdown / saved
    static constexpr Rect16 rc_hint    {   0, 248, 240,  44 }; ///< secondary hint

    enum class Phase : uint8_t { STANDING, BASE, COUNTDOWN, MOVING };

    Phase    phase;
    float    target_z;
    float    standing_height;
    float    base_height;
    int      direction;       ///< -1=down, 0=stopped, +1=up (latched)
    uint32_t countdown_start;
    char     status_buf[48];

    window_header_t header;
    window_text_t   lbl_step;
    window_text_t   lbl_title;
    window_text_t   lbl_instr;
    window_icon_t   icon;
    window_text_t   lbl_dn;
    window_text_t   lbl_up;
    WindowArrows    arrows;
    window_text_t   lbl_status;
    window_text_t   lbl_hint;

    void refresh_display();
    void update_arrows();
    void update_countdown_label();
    void issue_jog(float z);

    virtual void windowEvent(window_t *sender, GUI_event_t event, void *param) override;
};
