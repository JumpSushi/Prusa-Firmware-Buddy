#include "screen_splash.hpp"
#include "ScreenHandler.hpp"

#include "config.h"
#include "config_features.h"
#include <version/version.hpp>
#include "img_resources.hpp"
#include "marlin_client.hpp"
#include <config_store/store_instance.hpp>

#include "i18n.h"
#include "../lang/translator.hpp"
#include "language_eeprom.hpp"
#include "screen_menu_languages.hpp"
#include <pseudo_screen_callback.hpp>
#include "bsod.h"
#include <guiconfig/guiconfig.h>
#include <feature/factory_reset/factory_reset.hpp>
#include <window_msgbox_happy_printing.hpp>

#include <option/bootloader.h>
#include <option/developer_mode.h>
#include <option/has_translations.h>
#include <gui/screen_printer_setup.hpp>

#include <option/has_selftest.h>
#if HAS_SELFTEST()
    #include "printer_selftest.hpp"
    #include "screen_menu_selftest_snake.hpp"
#endif // HAS_SELFTEST

#include <option/has_touch.h>
#if HAS_TOUCH()
    #include <hw/touchscreen/touchscreen.hpp>
#endif // HAS_TOUCH

#if ENABLED(POWER_PANIC)
    #include "power_panic.hpp"
#endif

#include <option/has_toolchanger.h>
#if HAS_TOOLCHANGER()
    #include <module/prusa/toolchanger.h>
#endif

#include "display.hpp"
#include <option/has_switched_fan_test.h>

#if HAS_MINI_DISPLAY()
    #define SPLASHSCREEN_PROGRESSBAR_X 16
    #define SPLASHSCREEN_PROGRESSBAR_Y 148
    #define SPLASHSCREEN_PROGRESSBAR_W 206
    #define SPLASHSCREEN_PROGRESSBAR_H 12
    #define SPLASHSCREEN_VERSION_Y     165

#elif HAS_LARGE_DISPLAY()
    #define SPLASHSCREEN_PROGRESSBAR_X 100
    #define SPLASHSCREEN_PROGRESSBAR_Y 165
    #define SPLASHSCREEN_PROGRESSBAR_W 280
    #define SPLASHSCREEN_PROGRESSBAR_H 12
    #define SPLASHSCREEN_VERSION_Y     185
#endif

screen_splash_data_t::screen_splash_data_t()
    : screen_t()
    , text_progress(this, Rect16(0, SPLASHSCREEN_VERSION_Y, GuiDefaults::ScreenWidth, 18), is_multiline::no)
    , progress(this, Rect16(SPLASHSCREEN_PROGRESSBAR_X, SPLASHSCREEN_PROGRESSBAR_Y, SPLASHSCREEN_PROGRESSBAR_W, SPLASHSCREEN_PROGRESSBAR_H), COLOR_ORANGE, COLOR_GRAY, 6)
    , version_displayed(false) {
    ClrMenuTimeoutClose();

    text_progress.set_font(Font::small);
    text_progress.SetAlignment(Align_t::Center());
    text_progress.SetTextColor(COLOR_GRAY);

    snprintf(text_progress_buffer, sizeof(text_progress_buffer), "Firmware %s", version::project_version_full);
    text_progress.SetText(string_view_utf8::MakeRAM(text_progress_buffer));
    progress.SetProgressPercent(0);

#if ENABLED(POWER_PANIC)
    // don't present any screen or wizard if there is a powerpanic pending
    if (power_panic::state_stored()) {
        return;
    }
#endif

#if DEVELOPER_MODE()
    // don't present any screen or wizard
    return;
#endif

    // Custom Z-axis boot firmware: skip happy-printing, setup wizard, network
    // wizard, and hw-config wizard entirely. Mark them done so they don't
    // re-appear on subsequent boots.
    config_store().printer_network_setup_done.set(true);
    config_store().printer_hw_config_done.set(true);

#if HAS_TOUCH()
    constexpr auto touch_error_callback = +[] {
        touchscreen.set_enabled(false);
        MsgBoxWarning(_("Touch driver failed to initialize, touch functionality disabled"), Responses_Ok);
    };
#endif

    // Check for FW type change
    {
        auto &model_var = config_store().last_boot_base_printer_model;
        const auto model = model_var.get();
        const auto current_base_model = PrinterModelInfo::firmware_base().model;
        if (model == model_var.default_val) {
            // Not initialized - assume correct printer
            model_var.set(current_base_model);

        } else if (model != current_base_model) {
            constexpr auto callback = +[] {
                StringViewUtf8Parameters<16> params;
                MsgBoxError(
                    _("Printer type changed from %s to %s.\nFactory reset will be performed.\nSome configuration (network, filament profiles, ...) will be preserved.")
                        .formatted(params, PrinterModelInfo::get(config_store().last_boot_base_printer_model.get()).id_str, PrinterModelInfo::firmware_base().id_str),
                    { Response::Continue });

                FactoryReset::perform(false, FactoryReset::item_bitset({ FactoryReset::Item::network, FactoryReset::Item::stats, FactoryReset::Item::user_interface, FactoryReset::Item::user_profiles }));
            };
            Screens::Access()->PushBeforeCurrent(ScreenFactory::Screen<PseudoScreenCallback, callback>);
        }
    }

#if HAS_TOUCH()
    if (touchscreen.is_enabled() && !touchscreen.is_hw_ok()) {
        Screens::Access()->PushBeforeCurrent(ScreenFactory::Screen<PseudoScreenCallback, touch_error_callback>);
    }
#endif // HAS_TOUCH
#if HAS_TRANSLATIONS()
    if (!LangEEPROM::getInstance().IsValid()) {
        Screens::Access()->PushBeforeCurrent(ScreenFactory::Screen<ScreenMenuLanguages, ScreenMenuLanguages::Context::initial_language_selection>);
    }
#endif
}

screen_splash_data_t::~screen_splash_data_t() {
    display::enable_resource_file(); // now it is safe to use resources from xFlash
}

void screen_splash_data_t::draw() {
    Validate();
    progress.Invalidate();
    text_progress.Invalidate();
    screen_t::draw(); // We want to draw over bootloader's screen without flickering/redrawing
#ifdef _DEBUG
    #if HAS_MINI_DISPLAY()
    display::draw_text(Rect16(180, 91, 60, 16), string_view_utf8::MakeCPUFLASH("DEBUG"), Font::small, COLOR_BLACK, COLOR_RED);
    #endif
    #if HAS_LARGE_DISPLAY()
    display::draw_text(Rect16(340, 130, 60, 16), string_view_utf8::MakeCPUFLASH("DEBUG"), Font::small, COLOR_BLACK, COLOR_RED);
    #endif
#endif //_DEBUG
}

/**
 * @brief this callback must be called in GUI thread
 * also it must be called manually before main gui loop
 * no events can be fired during that period and gui_redraw() must be called manually
 *
 * @param percent value for progressbar
 * @param str string to show instead loading
 */
void screen_splash_data_t::bootstrap_cb(unsigned percent, std::optional<const char *> str) {
    GUIStartupProgress progr = { percent, str };
    event_conversion_union un;
    un.pGUIStartupProgress = &progr;
    Screens::Access()->WindowEvent(GUI_event_t::GUI_STARTUP, un.pvoid);
}

void screen_splash_data_t::windowEvent([[maybe_unused]] window_t *sender, GUI_event_t event, void *param) {
    if (event == GUI_event_t::GUI_STARTUP) { // without clear it could run multiple times before screen is closed
        if (!param) {
            return;
        }

        event_conversion_union un;
        un.pvoid = param;
        if (!un.pGUIStartupProgress) {
            return;
        }
        int percent = un.pGUIStartupProgress->percent_done;

        // Bootstrap & FW version are displayed in the same space - we want to display what process is happening during bootstrap
        // If such a process description is not available (e.g: fw_gui_splash_progress()) - draw FW version (only once to avoid flickering)
        if (un.pGUIStartupProgress->bootstrap_description.has_value()) {
            strlcpy(text_progress_buffer, un.pGUIStartupProgress->bootstrap_description.value(), sizeof(text_progress_buffer));
            text_progress.SetText(string_view_utf8::MakeRAM(text_progress_buffer));
            text_progress.Invalidate();
            version_displayed = false;
        } else {
            if (!version_displayed) {
                snprintf(text_progress_buffer, sizeof(text_progress_buffer), "Firmware %s", version::project_version_full);
                text_progress.SetText(string_view_utf8::MakeRAM(text_progress_buffer));
                text_progress.Invalidate();
                version_displayed = true;
            }
        }

        progress.SetProgressPercent(std::clamp(percent, 0, 100));
    }
}
