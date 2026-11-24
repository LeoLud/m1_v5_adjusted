// Copyright 2024 yangzheng20003 (@yangzheng20003)
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "wls/wls.h"
#include "rgb_record/rgb_record.h"

#ifdef WIRELESS_ENABLE
#    include "wireless.h"
#    include "usb_main.h"
#    include "lowpower.h"
#endif

typedef union {
    uint32_t raw;
    struct {
        uint8_t flag : 1;
        uint8_t devs : 3;
        uint8_t record_channel : 4;
        uint8_t record_last_mode;
        uint8_t last_btdevs : 3;
        uint8_t dir_flag : 1;
        uint8_t ctrl_app_flag : 1;
        uint8_t bat_ind_off : 1; // Fn + G: hide the always-on battery LEDs
        uint8_t wake_ind_off : 1; // ISO-Port: Fn + I: Akku-/Verbindungsanzeige nach dem Aufwachen aus
    };
} confinfo_t;
confinfo_t confinfo;

typedef struct {
    bool active;
    uint32_t timer;
    uint32_t interval;
    uint32_t times;
    uint8_t index;
    RGB rgb;
    void (*blink_cb)(uint8_t);
} hs_rgb_indicator_t;

enum layers {
    _BL = 0,
    _FL,
    _MBL,
    _MFL,
    _FBL,
};

hs_rgb_indicator_t hs_rgb_indicators[HS_RGB_INDICATOR_COUNT];
hs_rgb_indicator_t hs_rgb_bat[HS_RGB_BAT_COUNT];

void hs_reset_settings(void);
void rgb_matrix_hs_indicator(void);
void rgb_matrix_hs_indicator_set(uint8_t index, RGB rgb, uint32_t interval, uint8_t times);
void rgb_matrix_hs_set_remain_time(uint8_t index, uint8_t remain_time);

#define keymap_is_mac_system() ((get_highest_layer(default_layer_state) == _MBL) || (get_highest_layer(default_layer_state) == _MFL))
#define keymap_is_base_layer() ((get_highest_layer(default_layer_state) == _BL) || (get_highest_layer(default_layer_state) == _FL))

uint32_t post_init_timer     = 0x00;
bool charging_state          = false;
bool bat_full_flag           = false;
bool enable_bat_indicators   = true;
uint32_t bat_indicator_cnt   = true;
static uint32_t ee_clr_timer = 0;
bool no_record_fg, im_test_rate_flag;
bool lower_sleep = false;
uint8_t buff[]   = {14, 8, 2, 1, 1, 1, 1, 1, 1, 1, 0};

void eeconfig_confinfo_update(uint32_t raw) {

    eeconfig_update_kb(raw);
}

uint32_t eeconfig_confinfo_read(void) {

    return eeconfig_read_kb();
}

void eeconfig_confinfo_default(void) {

    confinfo.flag             = true;
    confinfo.record_channel   = 0;
    confinfo.record_last_mode = 0xff;
    confinfo.last_btdevs      = 1;
    confinfo.dir_flag         = 0;
    confinfo.ctrl_app_flag    = 0;
    confinfo.bat_ind_off      = 0;
    confinfo.wake_ind_off     = 0;
    // #ifdef WIRELESS_ENABLE
    //     confinfo.devs = DEVS_USB;
    // #endif

    eeconfig_init_user_datablock();
    eeconfig_confinfo_update(confinfo.raw);

#ifdef RGBLIGHT_ENABLE
    rgblight_mode(buff[0]);
#endif
}

void eeconfig_confinfo_init(void) {

    confinfo.raw = eeconfig_confinfo_read();
    if (!confinfo.raw) {
        eeconfig_confinfo_default();
    }
}

void keyboard_post_init_kb(void) {

#ifdef CONSOLE_ENABLE
    debug_enable = true;
#endif

    eeconfig_confinfo_init();

#ifdef LED_POWER_EN_PIN
    gpio_set_pin_output(LED_POWER_EN_PIN);
    if (rgb_matrix_get_val() != 0) gpio_write_pin_high(LED_POWER_EN_PIN);

    gpio_set_pin_output(HS_LED_BOOSTING_PIN);
    gpio_write_pin_high(HS_LED_BOOSTING_PIN);
#endif

#ifdef MM_BT_DEF_PIN
    setPinInputHigh(MM_BT_DEF_PIN);
#endif

#ifdef MM_2G4_DEF_PIN
    setPinInputHigh(MM_2G4_DEF_PIN);
#endif

#ifdef USB_POWER_EN_PIN
    gpio_write_pin_low(USB_POWER_EN_PIN);
    gpio_set_pin_output(USB_POWER_EN_PIN);
#endif

#ifdef HS_BAT_CABLE_PIN
    setPinInput(HS_BAT_CABLE_PIN);
#endif

#ifdef BAT_FULL_PIN
    setPinInputHigh(BAT_FULL_PIN);
#endif

    setPinInputHigh(SYSTEM_WIN_PIN);
    setPinInputHigh(SYSTEM_MAC_PIN);

#ifdef WIRELESS_ENABLE
    wireless_init();
#    if (!(defined(HS_BT_DEF_PIN) && defined(HS_2G4_DEF_PIN)))
    wireless_devs_change(!confinfo.devs, confinfo.devs, false);
#    endif
    post_init_timer = timer_read32();
#endif

    keyboard_post_init_user();

    rgbrec_init(confinfo.record_channel);
}

#ifdef WIRELESS_ENABLE

void usb_power_connect(void) {

#    ifdef USB_POWER_EN_PIN
    gpio_write_pin_low(USB_POWER_EN_PIN);
#    endif
}

void usb_power_disconnect(void) {

#    ifdef USB_POWER_EN_PIN
    gpio_write_pin_high(USB_POWER_EN_PIN);
#    endif
}

void suspend_power_down_kb(void) {

#    ifdef LED_POWER_EN_PIN
    gpio_write_pin_low(LED_POWER_EN_PIN);
#    endif

    suspend_power_down_user();
}

void suspend_wakeup_init_kb(void) {

#    ifdef LED_POWER_EN_PIN
    if (rgb_matrix_get_val() != 0) gpio_write_pin_high(LED_POWER_EN_PIN);
#    endif

    wireless_devs_change(wireless_get_current_devs(), wireless_get_current_devs(), false);
    suspend_wakeup_init_user();
    hs_rgb_blink_set_timer(timer_read32());
}

bool lpwr_is_allow_timeout_hook(void) {

    if (wireless_get_current_devs() == DEVS_USB) {
        // ISO-Port: Im Kabelmodus darf die Tastatur schlafen, wenn kein Kabel
        // steckt (sie läuft dann auf Akku und kann ohnehin nichts senden).
        return !charging_state;
    }

    return true;
}

void wireless_post_task(void) {

    // auto switching devs
    if (post_init_timer && timer_elapsed32(post_init_timer) >= 100) {

        md_send_devctrl(MD_SND_CMD_DEVCTRL_FW_VERSION);   // get the module fw version.
        md_send_devctrl(MD_SND_CMD_DEVCTRL_SLEEP_BT_EN);  // timeout 30min to sleep in bt mode, enable
        md_send_devctrl(MD_SND_CMD_DEVCTRL_SLEEP_2G4_EN); // timeout 30min to sleep in 2.4g mode, enable
        wireless_devs_change(!confinfo.devs, confinfo.devs, false);
        post_init_timer = 0x00;
    }

    hs_mode_scan(false, confinfo.devs, confinfo.last_btdevs);

    // ISO-Port: Kabelmodus ohne eingestecktes Kabel. Die Verbindungsanzeige
    // (Taste 5) blinkt wie bei Funk ohne Verbindung; nach HS_LBACK_TIMEOUT ohne
    // Kabel und ohne Tastendruck geht die Tastatur schlafen. Aufwachen per
    // Taste oder durch Einstecken des Kabels.
    {
        static uint32_t iso_nocable_timer = 0;
        if (wireless_get_current_devs() == DEVS_USB && !charging_state && lpwr_get_state() == LPWR_NORMAL) {
            if (!iso_nocable_timer) iso_nocable_timer = timer_read32() | 1;
            if (timer_elapsed32(iso_nocable_timer) >= HS_LBACK_TIMEOUT && last_input_activity_elapsed() >= HS_LBACK_TIMEOUT) {
                iso_nocable_timer = 0;
                lpwr_set_timeout_manual(true);
            }
        } else {
            iso_nocable_timer = 0;
        }
    }
}

uint32_t wls_process_long_press(uint32_t trigger_time, void *cb_arg) {
    uint16_t keycode = *((uint16_t *)cb_arg);

    switch (keycode) {
        case KC_BT1: {
            uint8_t mode = confinfo.devs;
            hs_modeio_detection(true, &mode, confinfo.last_btdevs);
            if ((mode == hs_bt) || (mode == hs_wireless) || (mode == hs_none)) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_BT1, true);
            }

        } break;
        case KC_BT2: {
            uint8_t mode = confinfo.devs;
            hs_modeio_detection(true, &mode, confinfo.last_btdevs);
            if ((mode == hs_bt) || (mode == hs_wireless) || (mode == hs_none)) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_BT2, true);
            }
        } break;
        case KC_BT3: {
            uint8_t mode = confinfo.devs;
            hs_modeio_detection(true, &mode, confinfo.last_btdevs);
            if ((mode == hs_bt) || (mode == hs_wireless) || (mode == hs_none)) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_BT3, true);
            }
        } break;
        case KC_2G4: {
            uint8_t mode = confinfo.devs;
            hs_modeio_detection(true, &mode, confinfo.last_btdevs);
            if ((mode == hs_2g4) || (mode == hs_wireless) || (mode == hs_none)) {
                wireless_devs_change(wireless_get_current_devs(), DEVS_2G4, true);
            }
        } break;
        case EE_CLR: {

        } break;
        default:
            break;
    }

    return 0;
}

bool process_record_wls(uint16_t keycode, keyrecord_t *record) {
    static uint16_t keycode_shadow                     = 0x00;
    static deferred_token wls_process_long_press_token = INVALID_DEFERRED_TOKEN;

    keycode_shadow = keycode;

#    ifndef WLS_KEYCODE_PAIR_TIME
#        define WLS_KEYCODE_PAIR_TIME 3000
#    endif

#    define WLS_KEYCODE_EXEC(wls_dev)                                                                                          \
        do {                                                                                                                   \
            if (record->event.pressed) {                                                                                       \
                if (wireless_get_current_devs() != wls_dev)                                                                    \
                    wireless_devs_change(wireless_get_current_devs(), wls_dev, false);                                         \
                if (wls_process_long_press_token == INVALID_DEFERRED_TOKEN) {                                                  \
                    wls_process_long_press_token = defer_exec(WLS_KEYCODE_PAIR_TIME, wls_process_long_press, &keycode_shadow); \
                }                                                                                                              \
            } else {                                                                                                           \
                cancel_deferred_exec(wls_process_long_press_token);                                                            \
                wls_process_long_press_token = INVALID_DEFERRED_TOKEN;                                                         \
            }                                                                                                                  \
        } while (false)

    switch (keycode) {
        case KC_BT1: {
            uint8_t mode = confinfo.devs;
            hs_modeio_detection(true, &mode, confinfo.last_btdevs);
            if ((mode == hs_bt) || (mode == hs_wireless) || (mode == hs_none)) {
                WLS_KEYCODE_EXEC(DEVS_BT1);
                hs_rgb_blink_set_timer(timer_read32());
            }

        } break;
        case KC_BT2: {
            uint8_t mode = confinfo.devs;
            hs_modeio_detection(true, &mode, confinfo.last_btdevs);
            if ((mode == hs_bt) || (mode == hs_wireless) || (mode == hs_none)) {
                WLS_KEYCODE_EXEC(DEVS_BT2);
                hs_rgb_blink_set_timer(timer_read32());
            }
        } break;
        case KC_BT3: {
            uint8_t mode = confinfo.devs;
            hs_modeio_detection(true, &mode, confinfo.last_btdevs);
            if ((mode == hs_bt) || (mode == hs_wireless) || (mode == hs_none)) {
                WLS_KEYCODE_EXEC(DEVS_BT3);
                hs_rgb_blink_set_timer(timer_read32());
            }
        } break;
        case KC_2G4: {
            uint8_t mode = confinfo.devs;
            hs_modeio_detection(true, &mode, confinfo.last_btdevs);
            if ((mode == hs_2g4) || (mode == hs_wireless) || (mode == hs_none)) {
                WLS_KEYCODE_EXEC(DEVS_2G4);
                hs_rgb_blink_set_timer(timer_read32());
            }
        } break;

        case KC_USB: {
            uint8_t mode = confinfo.devs;
            hs_modeio_detection(true, &mode, confinfo.last_btdevs);
            if ((mode == hs_2g4) || (mode == hs_wireless) || (mode == hs_none)) {
                WLS_KEYCODE_EXEC(DEVS_USB);
                hs_rgb_blink_set_timer(timer_read32());
            }
        } break;
        default:
            return true;
    }

    return false;
}
#endif

// While an RGB recording is running, keys paint LEDs instead of typing.
static bool process_record_rgbrec(uint16_t keycode, keyrecord_t *record) {

    if (*md_getp_state() == MD_STATE_CONNECTED) {
        hs_rgb_blink_set_timer(timer_read32());
    }

    switch (keycode) {
        case MO(_FL):
        case MO(_MFL): {
            if (!record->event.pressed && rgbrec_is_started()) {
                if (no_record_fg == true) {
                    no_record_fg = false;
                    rgbrec_register_record(keycode, record);
                }
                no_record_fg = true;
            }
            break;
        }
        case RP_END:
        case RP_P0:
        case RP_P1:
        case RP_P2:
        case RGB_MOD:
            break;
        default: {
            if (rgbrec_is_started()) {
                if (!IS_QK_MOMENTARY(keycode) && record->event.pressed) {
                    rgbrec_register_record(keycode, record);

                    return false;
                }
            }
        } break;
    }

    if (rgbrec_is_started() && (!(keycode == RP_P0 || keycode == RP_P1 || keycode == RP_P2 || keycode == RP_END || keycode == RGB_MOD || keycode == MO(_FL) || keycode == MO(_MFL)))) {

        return false;
    }

    return true;
}

void im_rgblight_increase(void) {
    HSV rgb;
    uint8_t moude;
    static uint8_t mode = 0;

    moude = rgblight_get_mode();
    if (moude == 1) {
        rgb = rgblight_get_hsv();
        if (rgb.h == 0 && rgb.s != 0)
            mode = 3;
        else
            mode = 9;
        switch (rgb.h) {
            case 40: {
                mode = 4;
            } break;
            case 80: {
                mode = 5;
            } break;
            case 120: {
                mode = 6;
            } break;
            case 160: {
                mode = 7;
            } break;
            case 200: {
                mode = 8;
            } break;
            default:
                break;
        }
    }

    mode++;
    if (mode == 11) mode = 0;
    if (mode == 10) {
        rgb = rgblight_get_hsv();
        rgblight_sethsv(0, 255, rgb.v);
        rgblight_disable();
    } else {
        rgblight_enable();
        rgblight_mode(buff[mode]);
    }

    rgb = rgblight_get_hsv();
    switch (mode) {
        case 3: {
            rgblight_sethsv(0, 255, rgb.v);
        } break;
        case 4: {
            rgblight_sethsv(40, 255, rgb.v);
        } break;
        case 5: {
            rgblight_sethsv(80, 255, rgb.v);
        } break;
        case 6: {
            rgblight_sethsv(120, 255, rgb.v);
        } break;
        case 7: {
            rgblight_sethsv(160, 255, rgb.v);
        } break;
        case 8: {
            rgblight_sethsv(200, 255, rgb.v);
        } break;
        case 9: {
            rgblight_sethsv(0, 0, rgb.v);
        } break;
        case 0: {
            rgblight_set_speed(255);
        } break;
        default: {
            rgblight_set_speed(200);
        } break;
    }
}

uint32_t hs_ct_time;
RGB rgb_test_open;
bool process_record_kb(uint16_t keycode, keyrecord_t *record) {

    if (process_record_user(keycode, record) != true) {
        return false;
    }

    if (process_record_rgbrec(keycode, record) != true) {
        return false;
    }

#ifdef WIRELESS_ENABLE
    if (process_record_wls(keycode, record) != true) {
        return false;
    }
#endif
    switch (keycode) {
        case QK_BOOT: {
            if (record->event.pressed) {
                dprintf("into boot!!!\r\n");
                // Keep settings across a flash. QMK re-initialises EEPROM on its own when
                // the layout changes (EECONFIG_MAGIC_NUMBER, user datablock version), and
                // VIA resets keymap/macros when the build date differs.
                bootloader_jump();
            }
        } break;

        case BT_TEST: {
            if (record->event.pressed) {
                md_send_devctrl(0x62);
            }
            return false;
        } break;
        case NK_TOGG: {
            if (rgbrec_is_started()) {

                return false;
            }
            if (record->event.pressed) {
                rgb_matrix_hs_indicator_set(0xFF, (RGB){0x00, 0x6E, 0x00}, 250, 1);
            }
        } break;
        case RL_MOD: {
            if (rgbrec_is_started()) {

                return false;
            }
            if (record->event.pressed) {
                im_rgblight_increase();
            }

            return false;
        } break;
        case EE_CLR: {
            if (record->event.pressed) {
                ee_clr_timer = timer_read32();
            } else {
                ee_clr_timer = 0;
            }

            return false;
        } break;
        case RGB_SPD: {
            if (record->event.pressed) {
                if (rgb_matrix_get_speed() <= RGB_MATRIX_SPD_STEP * 2) {
                    rgb_matrix_set_speed(RGB_MATRIX_SPD_STEP);

                    return false;
                }
            }
        } break;
        case RGB_VAI: {
            if (record->event.pressed) {
                rgb_matrix_enable();
                gpio_write_pin_high(LED_POWER_EN_PIN);
            }
        } break;
        case RGB_VAD: {
            if (record->event.pressed) {
                if (rgb_matrix_get_val() <= RGB_MATRIX_VAL_STEP) {
                    gpio_write_pin_low(LED_POWER_EN_PIN);
                    for (uint8_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
                        rgb_matrix_set_color(i, 0, 0, 0);
                    }
                }
            }
        } break;
        case TO(_BL): {
            if (record->event.pressed) {
                rgb_matrix_hs_set_remain_time(HS_RGB_BLINK_INDEX_MAC, 0);
                rgb_matrix_hs_indicator_set(HS_RGB_BLINK_INDEX_WIN, (RGB){RGB_WHITE}, 250, 3);
                if (keymap_is_mac_system()) {
                    set_single_persistent_default_layer(_BL);
                    layer_move(0);
                }
            }

            return false;
        } break;
        case TO(_MBL): {
            if (record->event.pressed) {
                rgb_matrix_hs_set_remain_time(HS_RGB_BLINK_INDEX_WIN, 0);
                rgb_matrix_hs_indicator_set(HS_RGB_BLINK_INDEX_MAC, (RGB){RGB_WHITE}, 250, 3);
                if (!keymap_is_mac_system()) {
                    set_single_persistent_default_layer(_MBL);
                    layer_move(0);
                }
            }

            return false;
        } break;
        case RP_P0: {
            if (record->event.pressed) {
                confinfo.record_channel = 0;
                rgbrec_read_current_channel(confinfo.record_channel);
                rgbrec_end(confinfo.record_channel);
                eeconfig_confinfo_update(confinfo.raw);
                rgbrec_show(confinfo.record_channel);
                dprintf("confinfo.record_last_mode = %d\r\n", confinfo.record_last_mode);
            }

            return false;
        } break;
        case RP_P1: {
            if (record->event.pressed) {
                confinfo.record_channel = 1;
                rgbrec_read_current_channel(confinfo.record_channel);
                rgbrec_end(confinfo.record_channel);
                eeconfig_confinfo_update(confinfo.raw);
                rgbrec_show(confinfo.record_channel);
            }

            return false;
        } break;
        case RP_P2: {
            if (record->event.pressed) {
                confinfo.record_channel = 2;
                rgbrec_read_current_channel(confinfo.record_channel);
                rgbrec_end(confinfo.record_channel);
                eeconfig_confinfo_update(confinfo.raw);
                rgbrec_show(confinfo.record_channel);
            }

            return false;
        } break;
        case RP_END: {
            if (record->event.pressed) {
                if (rgb_matrix_get_mode() != RGB_MATRIX_CUSTOM_RGBR_PLAY) {

                    return false;
                }
                if (!rgbrec_is_started()) {
                    rgbrec_start(confinfo.record_channel);
                    no_record_fg = false;
                    rgbrec_set_close_all(HSV_BLACK);
                } else {
                    rgbrec_end(confinfo.record_channel);
                }
                dprintf("confinfo.record_last_mode = %d\r\n", confinfo.record_last_mode);
            }

            return false;
        } break;
        case RGB_MOD: {
            if (record->event.pressed) {
                if (rgb_matrix_get_mode() == RGB_MATRIX_CUSTOM_RGBR_PLAY) {
                    if (rgbrec_is_started()) {
                        rgbrec_read_current_channel(confinfo.record_channel);
                        rgbrec_end(confinfo.record_channel);
                        no_record_fg = false;
                    }
                    if (confinfo.record_last_mode != 0xFF)
                        rgb_matrix_mode(confinfo.record_last_mode);
                    else
                        rgb_matrix_mode(RGB_MATRIX_DEFAULT_MODE);
                    eeconfig_confinfo_update(confinfo.raw);
                    dprintf("confinfo.record_last_mode = %d\r\n", confinfo.record_last_mode);
                    return false;
                }
                record_rgbmatrix_increase(&(confinfo.record_last_mode));
                eeconfig_confinfo_update(confinfo.raw);
            }

            return false;
        } break;
        case KC_LCMD: {
            if (keymap_is_mac_system()) {
                if (keymap_config.no_gui && !rgbrec_is_started()) {
                    if (record->event.pressed) {
                        register_code16(KC_LCMD);
                    } else {
                        unregister_code16(KC_LCMD);
                    }
                }
            }

            return true;
        } break;
        case KC_RCMD: {
            if (keymap_is_mac_system()) {
                if (keymap_config.no_gui && !rgbrec_is_started()) {
                    if (record->event.pressed) {
                        register_code16(KC_RCMD);
                    } else {
                        unregister_code16(KC_RCMD);
                    }
                }
            }

            return true;
        } break;
        case HS_BATQ: {
            extern bool rk_bat_req_flag;
            rk_bat_req_flag = (confinfo.devs != DEVS_USB) && record->event.pressed;
            return false;
        } break;
        case HS_BAT_IND: {
            if (record->event.pressed) {
                confinfo.bat_ind_off = !confinfo.bat_ind_off;
                // green flash = battery LEDs on, red flash = off
                if (confinfo.bat_ind_off) {
                    rgb_matrix_hs_indicator_set(0xFF, (RGB){.r = 0x40, .g = 0x00, .b = 0x00}, 250, 1);
                } else {
                    rgb_matrix_hs_indicator_set(0xFF, (RGB){.r = 0x00, .g = 0x40, .b = 0x00}, 250, 1);
                }
                eeconfig_confinfo_update(confinfo.raw);
            }
            return false;
        } break;
        case HS_WAKE_IND: {
            // ISO-Port: Aufwach-Anzeige (Akku + Verbindung) ein-/ausschalten.
            // Bestätigung wie bei den Akku-LEDs: grün blinkt = an, rot = aus.
            if (record->event.pressed) {
                confinfo.wake_ind_off = !confinfo.wake_ind_off;
                if (confinfo.wake_ind_off) {
                    rgb_matrix_hs_indicator_set(0xFF, (RGB){.r = 0x40, .g = 0x00, .b = 0x00}, 250, 1);
                } else {
                    rgb_matrix_hs_indicator_set(0xFF, (RGB){.r = 0x00, .g = 0x40, .b = 0x00}, 250, 1);
                }
                eeconfig_confinfo_update(confinfo.raw);
            }
            return false;
        } break;
        case HS_DIR: {
            if (record->event.pressed) {
                confinfo.dir_flag = !confinfo.dir_flag;
                rgb_test_open     = hsv_to_rgb((HSV){.h = 0, .s = 0, .v = RGB_MATRIX_VAL_STEP * 5});
                rgb_matrix_hs_indicator_set(0xFF, (RGB){rgb_test_open.r, rgb_test_open.g, rgb_test_open.b}, 250, 1);
                eeconfig_confinfo_update(confinfo.raw);
            }
            return false;
        } break;

        case KC_A: {
            if (confinfo.dir_flag) {
                if (record->event.pressed) {
                    register_code16(KC_LEFT);
                } else {
                    unregister_code16(KC_LEFT);
                }
                return false;
            } else {
                return true;
            }
        } break;

        case KC_S: {
            if (confinfo.dir_flag) {
                if (record->event.pressed) {
                    register_code16(KC_DOWN);
                } else {
                    unregister_code16(KC_DOWN);
                }
                return false;
            } else {
                return true;
            }
        } break;

        case KC_D: {
            if (confinfo.dir_flag) {
                if (record->event.pressed) {
                    register_code16(KC_RGHT);
                } else {
                    unregister_code16(KC_RGHT);
                }
                return false;
            } else {
                return true;
            }
        } break;

        case KC_W: {
            if (confinfo.dir_flag) {
                if (record->event.pressed) {
                    register_code16(KC_UP);
                } else {
                    unregister_code16(KC_UP);
                }
                return false;
            } else {
                return true;
            }
        } break;

        case KC_LEFT: {
            if (confinfo.dir_flag) {
                if (record->event.pressed) {
                    register_code16(KC_A);
                } else {
                    unregister_code16(KC_A);
                }
                return false;
            } else {
                return true;
            }
        } break;

        case KC_DOWN: {
            if (confinfo.dir_flag) {
                if (record->event.pressed) {
                    register_code16(KC_S);
                } else {
                    unregister_code16(KC_S);
                }
                return false;
            } else {
                return true;
            }
        } break;

        case KC_RGHT: {
            if (confinfo.dir_flag) {
                if (record->event.pressed) {
                    register_code16(KC_D);
                } else {
                    unregister_code16(KC_D);
                }
                return false;
            } else {
                return true;
            }
        } break;

        case KC_UP: {
            if (confinfo.dir_flag) {
                if (record->event.pressed) {
                    register_code16(KC_W);
                } else {
                    unregister_code16(KC_W);
                }
                return false;
            } else {
                return true;
            }
        } break;
        case HS_CT_A: {
            if (record->event.pressed) {
                hs_ct_time = timer_read32();
            } else {
                hs_ct_time = 0;
            }
            return false;
        } break;
        case KC_RCTL: {
            if (confinfo.ctrl_app_flag) {
                if (record->event.pressed) {
                    register_code16(KC_APP);
                } else {
                    unregister_code16(KC_APP);
                }
                return false;
            } else {
                return true;
            }
        } break;
        case HS_SIRI: {
            if (record->event.pressed) {
                register_code(KC_LCMD);
                register_code(KC_SPC);
            } else {
                unregister_code(KC_SPC);
                unregister_code(KC_LCMD);
            }
            return false;
        } break;
        case KC_MCTL: {
            if (record->event.pressed) {
                register_code(KC_LCTL);
                register_code(KC_UP);

            } else {
                unregister_code(KC_LCTL);
                unregister_code(KC_UP);
            }
            return false;
        } break;
        default:
            break;
    }

    return true;
}

// Runs every main-loop pass. This is a _user hook only because the wireless
// driver (keyboards/linker/wireless/wireless.c) already owns housekeeping_task_kb.
void housekeeping_task_user(void) {
    uint8_t hs_now_mode;
    static uint32_t hs_current_time;

    charging_state = readPin(HS_BAT_CABLE_PIN);

    bat_full_flag = readPin(BAT_FULL_PIN);

    if (charging_state && (bat_full_flag)) {
        hs_now_mode = MD_SND_CMD_DEVCTRL_CHARGING_DONE;
    } else if (charging_state) {
        hs_now_mode = MD_SND_CMD_DEVCTRL_CHARGING;
    } else {
        hs_now_mode = MD_SND_CMD_DEVCTRL_CHARGING_STOP;
    }

    if (!hs_current_time || timer_elapsed32(hs_current_time) > 1000) {

        hs_current_time = timer_read32();
        md_send_devctrl(hs_now_mode);
        md_send_devctrl(MD_SND_CMD_DEVCTRL_INQVOL);
    }

    if (charging_state) {
        writePin(HS_LED_BOOSTING_PIN, 0);

    } else {
        writePin(HS_LED_BOOSTING_PIN, 1);
    }

    if (timer_elapsed32(hs_ct_time) > 3000 && hs_ct_time) {
        confinfo.ctrl_app_flag = !confinfo.ctrl_app_flag;
        rgb_test_open          = hsv_to_rgb((HSV){.h = 0, .s = 0, .v = RGB_MATRIX_VAL_STEP * 5});
        rgb_matrix_hs_indicator_set(0xFF, (RGB){rgb_test_open.r, rgb_test_open.g, rgb_test_open.b}, 250, 1);
        eeconfig_confinfo_update(confinfo.raw);
        hs_ct_time = 0;
    }

    if ((readPin(SYSTEM_WIN_PIN) != 0) && (readPin(SYSTEM_MAC_PIN) == 0)) { // mac system
        if (!keymap_is_mac_system()) {
            set_single_persistent_default_layer(_MBL);
            layer_move(0);
        }
    } else { // win system
        if (keymap_is_mac_system()) {
            set_single_persistent_default_layer(_BL);
            layer_move(0);
        }
    }
}

#ifdef RGB_MATRIX_ENABLE

#    ifdef WIRELESS_ENABLE
bool wls_rgb_indicator_reset        = false;
uint32_t wls_rgb_indicator_timer    = 0x00;
uint32_t wls_rgb_indicator_interval = 0;
uint32_t wls_rgb_indicator_times    = 0;
uint32_t wls_rgb_indicator_index    = 0;
RGB wls_rgb_indicator_rgb           = {0};

void rgb_matrix_wls_indicator_set(uint8_t index, RGB rgb, uint32_t interval, uint8_t times) {

    wls_rgb_indicator_timer = timer_read32();

    wls_rgb_indicator_index    = index;
    wls_rgb_indicator_interval = interval;
    wls_rgb_indicator_times    = times * 2;
    wls_rgb_indicator_rgb      = rgb;
}

void wireless_devs_change_kb(uint8_t old_devs, uint8_t new_devs, bool reset) {

    wls_rgb_indicator_reset = reset;

    if (confinfo.devs != wireless_get_current_devs()) {
        confinfo.devs = wireless_get_current_devs();
        if (confinfo.devs > 0 && confinfo.devs < 4) confinfo.last_btdevs = confinfo.devs;
        eeconfig_confinfo_update(confinfo.raw);
    }

    switch (new_devs) {
        case DEVS_USB: {
            if (reset) {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_USB, (RGB){HS_LBACK_COLOR_USB}, 200, 1);
            } else {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_USB, (RGB){HS_PAIR_COLOR_USB}, 500, 1);
            }
        } break;
        case DEVS_BT1: {
            if (reset) {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_BT1, (RGB){HS_LBACK_COLOR_BT1}, 200, 1);
            } else {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_BT1, (RGB){HS_PAIR_COLOR_BT1}, 500, 1);
            }
        } break;
        case DEVS_BT2: {
            if (reset) {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_BT2, (RGB){HS_LBACK_COLOR_BT2}, 200, 1);
            } else {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_BT2, (RGB){HS_PAIR_COLOR_BT2}, 500, 1);
            }
        } break;
        case DEVS_BT3: {
            if (reset) {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_BT3, (RGB){HS_LBACK_COLOR_BT3}, 200, 1);
            } else {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_BT3, (RGB){HS_PAIR_COLOR_BT3}, 500, 1);
            }
        } break;
        case DEVS_BT4: {
            if (reset) {
                rgb_matrix_wls_indicator_set(41, (RGB){RGB_BLUE}, 200, 1);
            } else {
                rgb_matrix_wls_indicator_set(41, (RGB){RGB_BLUE}, 500, 1);
            }
        } break;
        case DEVS_BT5: {
            if (reset) {
                rgb_matrix_wls_indicator_set(42, (RGB){RGB_BLUE}, 200, 1);
            } else {
                rgb_matrix_wls_indicator_set(42, (RGB){RGB_BLUE}, 500, 1);
            }
        } break;
        case DEVS_2G4: {
            if (reset) {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_2G4, (RGB){HS_LBACK_COLOR_2G4}, 200, 1);
            } else {
                rgb_matrix_wls_indicator_set(HS_RGB_BLINK_INDEX_2G4, (RGB){HS_LBACK_COLOR_2G4}, 500, 1);
            }
        } break;
        default:
            break;
    }
}

bool rgb_matrix_wls_indicator_cb(void) {

    if (*md_getp_state() != MD_STATE_CONNECTED) {
        if (!(wireless_get_current_devs() == DEVS_USB && USB_DRIVER.state == USB_ACTIVE)) wireless_devs_change_kb(wireless_get_current_devs(), wireless_get_current_devs(), wls_rgb_indicator_reset);
        return true;
    }

    // refresh led
    led_wakeup();

    return false;
}

void rgb_matrix_wls_indicator(void) {

    if (wls_rgb_indicator_timer) {

        if (timer_elapsed32(wls_rgb_indicator_timer) >= wls_rgb_indicator_interval) {
            wls_rgb_indicator_timer = timer_read32();

            if (wls_rgb_indicator_times) {
                wls_rgb_indicator_times--;
            }

            if (wls_rgb_indicator_times <= 0) {
                wls_rgb_indicator_timer = 0x00;
                if (rgb_matrix_wls_indicator_cb() != true) {
                    return;
                }
            }
        }

        if (wls_rgb_indicator_times % 2) {
            rgb_matrix_set_color(wls_rgb_indicator_index, wls_rgb_indicator_rgb.r, wls_rgb_indicator_rgb.g, wls_rgb_indicator_rgb.b);
        } else {
            rgb_matrix_set_color(wls_rgb_indicator_index, 0x00, 0x00, 0x00);
        }
    }
}

void rgb_matrix_hs_bat_set(uint8_t index, RGB rgb, uint32_t interval, uint8_t times) {
    for (int i = 0; i < HS_RGB_BAT_COUNT; i++) {
        if (!hs_rgb_bat[i].active) {
            hs_rgb_bat[i].active   = true;
            hs_rgb_bat[i].timer    = timer_read32();
            hs_rgb_bat[i].interval = interval;
            hs_rgb_bat[i].times    = times * 2;
            hs_rgb_bat[i].index    = index;
            hs_rgb_bat[i].rgb      = rgb;
            break;
        }
    }
}

void rgb_matrix_hs_bat(void) {
    for (int i = 0; i < HS_RGB_BAT_COUNT; i++) {
        if (hs_rgb_bat[i].active) {
            if (timer_elapsed32(hs_rgb_bat[i].timer) >= hs_rgb_bat[i].interval) {
                hs_rgb_bat[i].timer = timer_read32();

                if (hs_rgb_bat[i].times) {
                    hs_rgb_bat[i].times--;
                }

                if (hs_rgb_bat[i].times <= 0) {
                    hs_rgb_bat[i].active = false;
                    hs_rgb_bat[i].timer  = 0x00;
                }
            }

            if (hs_rgb_bat[i].times % 2) {
                rgb_matrix_set_color(hs_rgb_bat[i].index, hs_rgb_bat[i].rgb.r, hs_rgb_bat[i].rgb.g, hs_rgb_bat[i].rgb.b);
            } else {
                rgb_matrix_set_color(hs_rgb_bat[i].index, 0x00, 0x00, 0x00);
            }
        }
    }
}

void bat_indicators(void) {
    static uint32_t battery_process_time = 0;

    if (charging_state && (bat_full_flag)) {
        battery_process_time = 0;
    } else if (charging_state) {

        battery_process_time = 0;
        if (!confinfo.bat_ind_off) rgb_matrix_set_color(HS_MATRIX_BLINK_INDEX_BAT, 0x00, 0xFF, 0x00);
    } else if (*md_getp_bat() <= BATTERY_CAPACITY_LOW) {

        if (!confinfo.bat_ind_off) rgb_matrix_hs_bat_set(HS_MATRIX_BLINK_INDEX_BAT, (RGB){.r = 0xFF, .g = 0x00, .b = 0x00}, 250, 1);

        if (*md_getp_bat() <= BATTERY_CAPACITY_STOP) {
            if (!battery_process_time) {
                battery_process_time = timer_read32();
            }

            if (battery_process_time && timer_elapsed32(battery_process_time) > 60000) {
                battery_process_time = 0;
                lower_sleep          = true;
                lpwr_set_timeout_manual(true);
            }
        } else {
            // ISO-Port: Timer zurücksetzen, sobald der Akku wieder über 0 % meldet.
            // Vorher lief er zwischen 1 und 15 % weiter, sodass ein einzelner
            // kurzer 0-%-Messwert sofort den Tiefschlaf auslösen konnte, aus dem
            // die Tastatur nur per Kabel wieder aufwacht.
            battery_process_time = 0;
        }
    } else {
        battery_process_time = 0;
    }
}

#    endif

#endif

static bool hs_reset_settings_user(void) {

    rgb_matrix_hs_indicator_set(0xFF, (RGB){0x10, 0x10, 0x10}, 250, 3);

    return true;
}

void nkr_indicators_hook(uint8_t index) {

    if ((hs_rgb_indicators[index].rgb.r == 0x6E) && (hs_rgb_indicators[index].rgb.g == 0x00) && (hs_rgb_indicators[index].rgb.b == 0x00)) {

        rgb_matrix_hs_indicator_set(0xFF, (RGB){0x6E, 0x00, 0x00}, 250, 1);

    } else if ((hs_rgb_indicators[index].rgb.r == 0x00) && (hs_rgb_indicators[index].rgb.g == 0x6E) && (hs_rgb_indicators[index].rgb.b == 0x00)) {

        rgb_matrix_hs_indicator_set(0xFF, (RGB){0x00, 0x00, 0x6F}, 250, 1);
    }
}

void rgb_matrix_hs_indicator_set(uint8_t index, RGB rgb, uint32_t interval, uint8_t times) {

    for (int i = 0; i < HS_RGB_INDICATOR_COUNT; i++) {
        if (!hs_rgb_indicators[i].active) {
            hs_rgb_indicators[i].active   = true;
            hs_rgb_indicators[i].timer    = timer_read32();
            hs_rgb_indicators[i].interval = interval;
            hs_rgb_indicators[i].times    = times * 2;
            hs_rgb_indicators[i].index    = index;
            hs_rgb_indicators[i].rgb      = rgb;
            if (index != 0xFF)
                hs_rgb_indicators[i].blink_cb = NULL;
            else {
                hs_rgb_indicators[i].blink_cb = nkr_indicators_hook;
            }
            break;
        }
    }
}

void rgb_matrix_hs_set_remain_time(uint8_t index, uint8_t remain_time) {

    for (int i = 0; i < HS_RGB_INDICATOR_COUNT; i++) {
        if (hs_rgb_indicators[i].active && hs_rgb_indicators[i].index == index) {
            hs_rgb_indicators[i].times  = 0;
            hs_rgb_indicators[i].active = false;
            break;
        }
    }
}

void rgb_matrix_hs_indicator(void) {
    for (int i = 0; i < HS_RGB_INDICATOR_COUNT; i++) {
        if (hs_rgb_indicators[i].active) {
            if (timer_elapsed32(hs_rgb_indicators[i].timer) >= hs_rgb_indicators[i].interval) {
                hs_rgb_indicators[i].timer = timer_read32();

                if (hs_rgb_indicators[i].times) {
                    hs_rgb_indicators[i].times--;
                }

                if (hs_rgb_indicators[i].times <= 0) {
                    hs_rgb_indicators[i].active = false;
                    hs_rgb_indicators[i].timer  = 0x00;
                    if (hs_rgb_indicators[i].blink_cb != NULL)
                        hs_rgb_indicators[i].blink_cb(i);
                    continue;
                }
            }

            if ((hs_rgb_indicators[i].times % 2)) {
                if (hs_rgb_indicators[i].index == 0xFF) {
                    rgb_matrix_set_color_all(hs_rgb_indicators[i].rgb.r, hs_rgb_indicators[i].rgb.g, hs_rgb_indicators[i].rgb.b);
                } else {
                    rgb_matrix_set_color(hs_rgb_indicators[i].index, hs_rgb_indicators[i].rgb.r, hs_rgb_indicators[i].rgb.g, hs_rgb_indicators[i].rgb.b);
                }
            } else {
                if (hs_rgb_indicators[i].index == 0xFF) {
                    rgb_matrix_set_color_all(0x00, 0x00, 0x00);
                } else {
                    rgb_matrix_set_color(hs_rgb_indicators[i].index, 0x00, 0x00, 0x00);
                }
            }
        }
    }
}

/* ---------------------------------------------------------------------------
 * Zusätzliche Anzeigen (ISO-Port)
 *
 *  - Ebene: Solange eine Ebene über der Grundebene aktiv ist, leuchtet F1..F4
 *    für Ebene 1..4 (Nummern wie in VIA) grün, die anderen drei F-Tasten sind
 *    dunkel. Auf der Grundebene bleibt alles normal.
 *  - Verbindung: Solange Fn gehalten wird, leuchtet die Taste des aktuellen
 *    Modus dauerhaft: 1/2/3 = Bluetooth (blau), 4 = 2.4 GHz (grün), 5 = Kabel (weiß).
 *    Zusätzlich zeigt die Leertaste den Akkustand (rot = leer ... grün = voll).
 *  - Aufwachen: Nach dem Aufwachen aus dem Energiesparmodus zeigt die Leertaste
 *    1 Sekunde lang den Akkustand (rot = leer ... grün = voll) und die Taste
 *    des aktuellen Modus leuchtet mit.
 *
 * Läuft vor den Statusanzeigen der Firmware; deren Blinken (z. B. Kopplung)
 * liegt also weiterhin obenauf.
 * ------------------------------------------------------------------------- */
#define ISO_IND_WAKE_MS      1000
#define ISO_IND_LED_SPACE    82
static const uint8_t iso_ind_fkey_led[4] = {22, 21, 20, 19}; // F1, F2, F3, F4
static uint32_t iso_ind_wake_timer = 0;

static void iso_ind_set(uint8_t led, uint8_t led_min, uint8_t led_max, uint8_t r, uint8_t g, uint8_t b) {
    if (led >= led_min && led < led_max) rgb_matrix_set_color(led, r, g, b);
}

static void iso_ind_connection(uint8_t led_min, uint8_t led_max) {
#ifdef WIRELESS_ENABLE
    switch (wireless_get_current_devs()) {
        case DEVS_BT1: iso_ind_set(HS_RGB_BLINK_INDEX_BT1, led_min, led_max, 0x00, 0x00, 0xFF); break;
        case DEVS_BT2: iso_ind_set(HS_RGB_BLINK_INDEX_BT2, led_min, led_max, 0x00, 0x00, 0xFF); break;
        case DEVS_BT3: iso_ind_set(HS_RGB_BLINK_INDEX_BT3, led_min, led_max, 0x00, 0x00, 0xFF); break;
        case DEVS_2G4: iso_ind_set(HS_RGB_BLINK_INDEX_2G4, led_min, led_max, 0x00, 0xFF, 0x00); break;
        case DEVS_USB: iso_ind_set(HS_RGB_BLINK_INDEX_USB, led_min, led_max, 0xFF, 0xFF, 0xFF); break;
        default: break;
    }
#endif
}

// ISO-Port: Kabelmodus, Kabel steckt, aber der PC ist nicht aktiv (heruntergefahren
// oder Standby): Nach ISO_USB_IDLE_OFF_MS geht die Tastatur in den Schlafzustand,
// alle LEDs und die LED-Stromversorgung aus, kein Blinken. Sobald der PC sich
// wieder meldet, ist sofort alles wie vorher.
#define ISO_USB_IDLE_OFF_MS  5000
static uint32_t iso_usb_idle_timer = 0;

static bool iso_usb_idle(void) {
#ifdef WIRELESS_ENABLE
    if (wireless_get_current_devs() != DEVS_USB) {
        iso_usb_idle_timer = 0;
        return false;
    }
#endif
    // Ohne Kabel ist das kein "PC aus", sondern "keine Verbindung": dann soll die
    // Verbindungsanzeige blinken (siehe wireless_post_task).
    if (!charging_state) {
        iso_usb_idle_timer = 0;
        return false;
    }
    if (USB_DRIVER.state == USB_ACTIVE) {
        iso_usb_idle_timer = 0;
        return false;
    }
    if (!iso_usb_idle_timer) iso_usb_idle_timer = timer_read32() | 1;
    return timer_elapsed32(iso_usb_idle_timer) >= ISO_USB_IDLE_OFF_MS;
}

static void iso_ind_draw(uint8_t led_min, uint8_t led_max) {
    if (iso_usb_idle()) {
        for (uint8_t i = led_min; i < led_max; i++) rgb_matrix_set_color(i, 0x00, 0x00, 0x00);
        return;
    }
    if (rgbrec_is_started()) return;

    // Ebene auf F1..F4
    uint8_t def = get_highest_layer(default_layer_state);
    uint8_t top = get_highest_layer(layer_state | default_layer_state);
    if (top > def) {
        for (uint8_t i = 0; i < 4; i++) {
            if (top == i + 1)
                iso_ind_set(iso_ind_fkey_led[i], led_min, led_max, 0x00, 0xFF, 0x00); // grün
            else
                iso_ind_set(iso_ind_fkey_led[i], led_min, led_max, 0x00, 0x00, 0x00);
        }
    }

    // Verbindung, solange Fn gehalten wird
    bool fn_held = layer_state & (((layer_state_t)1 << _FL) | ((layer_state_t)1 << _MFL));

    // Nach dem Aufwachen: Akku auf der Leertaste + Verbindung
    bool wake_show = iso_ind_wake_timer && timer_elapsed32(iso_ind_wake_timer) < ISO_IND_WAKE_MS;
    if (iso_ind_wake_timer && !wake_show) iso_ind_wake_timer = 0;

    if (fn_held || wake_show) iso_ind_connection(led_min, led_max);

#ifdef WIRELESS_ENABLE
    if (wake_show || fn_held) {
        uint8_t bat = *md_getp_bat();
        if (bat > 100) bat = 100;
        RGB rgb = hsv_to_rgb((HSV){.h = (uint8_t)((uint16_t)bat * 85 / 100), .s = 255, .v = RGB_MATRIX_MAXIMUM_BRIGHTNESS});
        iso_ind_set(ISO_IND_LED_SPACE, led_min, led_max, rgb.r, rgb.g, rgb.b);
    }
#endif
}

static bool iso_indicators_advanced_impl(uint8_t led_min, uint8_t led_max) {

    if (rgb_matrix_indicators_advanced_user(led_min, led_max) != true) {

        return false;
    }

    iso_ind_draw(led_min, led_max);

    if (ee_clr_timer && timer_elapsed32(ee_clr_timer) > 3000) {
        hs_reset_settings();
        ee_clr_timer = 0;
    }

    bool iso_idle = iso_usb_idle();

#ifdef LED_POWER_EN_PIN
    // ISO-Port: Kabel steckt, PC aus -> LED-Stromversorgung aus (Schlafzustand).
    if (iso_idle && gpio_read_pin(LED_POWER_EN_PIN)) {
        gpio_write_pin_low(LED_POWER_EN_PIN);
    }
    // ISO-Port: LED-Stromversorgung mit dem tatsächlichen Zustand abgleichen.
    // MonsGeek schaltet sie nur über Fn + Pfeil hoch oder beim Aufwachen (falls
    // Helligkeit > 0) wieder ein. Hat OpenRGB zwischendurch Helligkeit 0 gesetzt
    // und die Tastatur ist eingeschlafen, blieb die Beleuchtung danach dunkel,
    // obwohl OpenRGB wieder eine Helligkeit > 0 geschickt hat. Diese Funktion
    // läuft nur bei aktiver Beleuchtung; Helligkeit 0, Schlaf und Standby
    // bleiben dunkel wie bisher.
    if (!iso_idle && rgb_matrix_get_val() > 0
#    ifdef WIRELESS_ENABLE
        && lpwr_get_state() == LPWR_NORMAL
#    endif
        && !gpio_read_pin(LED_POWER_EN_PIN)) {
        gpio_write_pin_high(LED_POWER_EN_PIN);
    }
#endif

    // ISO-Port: PC im Kabelmodus nicht aktiv (Kabel steckt): alles dunkel, auch
    // Caps Lock, Win-Sperre und das Blinken der Kabel-Verbindungsanzeige (Taste 5).
    if (!iso_idle && host_keyboard_led_state().caps_lock)
        rgb_matrix_set_color(HS_RGB_INDEX_CAPS, 0xFF, 0xFF, 0xFF); // ISO-Port: Caps Lock weiß, volle Helligkeit

    if (!iso_idle && !keymap_is_mac_system() && keymap_config.no_gui)
        rgb_matrix_set_color(HS_RGB_INDEX_WIN_LOCK, 0x20, 0x20, 0x20);

#ifdef WIRELESS_ENABLE
    if (!iso_idle) rgb_matrix_wls_indicator();

    if (enable_bat_indicators && !rgbrec_is_started()) {
        if (!confinfo.bat_ind_off) rgb_matrix_hs_bat();
        bat_indicators(); // always runs: also handles auto-sleep at 0 %
        bat_indicator_cnt = timer_read32();
    }

    if (!enable_bat_indicators) {
        if (timer_elapsed32(bat_indicator_cnt) > 2000) {
            enable_bat_indicators = true;
            bat_indicator_cnt     = timer_read32();
        }
    }

#endif

    rgb_matrix_hs_indicator();

    query();
    return true;
}

void hs_reset_settings(void) {
    enable_bat_indicators = false;
    eeconfig_init();
    eeconfig_update_rgb_matrix_default();

#ifdef RGBLIGHT_ENABLE
    extern void rgblight_init(void);
    is_rgblight_initialized = false;
    rgblight_init();
    eeconfig_update_rgblight_default();
    rgblight_enable();
#endif

    keymap_config.raw = eeconfig_read_keymap();

#if defined(NKRO_ENABLE) && defined(FORCE_NKRO)
    // eeconfig_init() cleared NKRO; turn it back on like boot does, otherwise
    // the board stays in 6KRO until it is power-cycled.
    keymap_config.nkro = 1;
    eeconfig_update_keymap(keymap_config.raw);
#endif

    // #if defined(WIRELESS_ENABLE)
    //     wireless_devs_change(wireless_get_current_devs(), DEVS_USB, false);
    // #endif

    if (hs_reset_settings_user() != true) {

        return;
    }
    hs_rgb_blink_set_timer(timer_read32());
    keyboard_post_init_kb();
}

void lpwr_wakeup_hook(void) {
    hs_mode_scan(false, confinfo.devs, confinfo.last_btdevs);

    if (!confinfo.wake_ind_off) iso_ind_wake_timer = timer_read32() | 1; // Aufwach-Anzeige starten (nie 0), abschaltbar mit Fn + I

    if (rgb_matrix_get_val() != 0)
        gpio_write_pin_high(LED_POWER_EN_PIN);
    else
        gpio_write_pin_low(LED_POWER_EN_PIN);

    gpio_write_pin_high(HS_LED_BOOSTING_PIN);
}


/* ---------------------------------------------------------------------------
 * Farbkorrektur (ISO-Port)
 *
 * Die LEDs bekommen ihre Werte sonst linear; Mischfarben wirken dann blass und
 * Farbtöne verschieben sich beim Dimmen. Deshalb werden Farbe und Helligkeit
 * hier getrennt behandelt, kurz vor der Ausgabe an die LEDs:
 *
 *  1. Farbe: Die Farbe wird auf volle Helligkeit hochgerechnet (stärkster Kanal
 *     = 255) und dort korrigiert: Farbabgleich (ISO_BALANCE_R/G/B), Lila-
 *     Korrekturen (ISO_PURPLE_RED_CUT, ISO_PURPLE_RICH) und Gamma 1.8.
 *     Ergebnis: wie die Farbe bei voller Helligkeit aussehen soll.
 *  2. Helligkeit: Die ursprüngliche Helligkeit (stärkster Kanal) läuft über eine
 *     eigene Kurve (Gamma 1.8, unterhalb 64 linear wie bei sRGB), damit Dimmen
 *     und Animationen gleichmäßig wirken.
 *  3. Ausgabe = korrigierte Farbe x Helligkeit. Der Farbton bleibt dadurch auf
 *     allen Helligkeitsstufen gleich.
 *  4. Ganz unten (8-Bit-Grenze): Würde ein wichtiger Farbanteil (mind. 1/6 des
 *     stärksten) auf 0 fallen, geht die LED ganz aus statt kurz in eine falsche
 *     Farbe zu kippen (z. B. Lila -> Blau am Ende von Breathing). Eingangswerte
 *     unter ISO_MIN_INPUT gelten als aus, weil QMK dort selbst keinen sauberen
 *     Farbton mehr liefert.
 *
 * Gilt für alles: Effekte, Statusanzeigen und OpenRGB.
 * Abschalten: in config.h die Zeile "#define ISO_GAMMA_ENABLE" auskommentieren.
 * ------------------------------------------------------------------------- */
#ifdef ISO_GAMMA_ENABLE
extern rgb_led_t rgb_matrix_ws2812_array[];

#    ifndef ISO_MIN_INPUT
#        define ISO_MIN_INPUT 6
#    endif

// Farbkurve: reines Gamma 1.8 (Farbanteile bei voller Helligkeit)
static const uint8_t PROGMEM iso_color_table[256] = {
      0,   0,   0,   0,   0,   0,   0,   0,   1,   1,   1,   1,   1,   1,   1,   2,
      2,   2,   2,   2,   3,   3,   3,   3,   4,   4,   4,   4,   5,   5,   5,   6,
      6,   6,   7,   7,   8,   8,   8,   9,   9,  10,  10,  10,  11,  11,  12,  12,
     13,  13,  14,  14,  15,  15,  16,  16,  17,  17,  18,  18,  19,  19,  20,  21,
     21,  22,  22,  23,  24,  24,  25,  26,  26,  27,  28,  28,  29,  30,  30,  31,
     32,  32,  33,  34,  35,  35,  36,  37,  38,  38,  39,  40,  41,  41,  42,  43,
     44,  45,  46,  46,  47,  48,  49,  50,  51,  52,  53,  53,  54,  55,  56,  57,
     58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,
     74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  86,  87,  88,  89,  90,
     91,  92,  93,  95,  96,  97,  98,  99, 100, 102, 103, 104, 105, 107, 108, 109,
    110, 111, 113, 114, 115, 116, 118, 119, 120, 122, 123, 124, 126, 127, 128, 129,
    131, 132, 134, 135, 136, 138, 139, 140, 142, 143, 145, 146, 147, 149, 150, 152,
    153, 154, 156, 157, 159, 160, 162, 163, 165, 166, 168, 169, 171, 172, 174, 175,
    177, 178, 180, 181, 183, 184, 186, 188, 189, 191, 192, 194, 195, 197, 199, 200,
    202, 204, 205, 207, 208, 210, 212, 213, 215, 217, 218, 220, 222, 224, 225, 227,
    229, 230, 232, 234, 236, 237, 239, 241, 243, 244, 246, 248, 250, 251, 253, 255
};

// Helligkeitskurve: Gamma 1.8, unterhalb 64 linear (gleichmäßiges Dimmen)
static const uint8_t PROGMEM iso_level_table[256] = {
      0,   0,   1,   1,   1,   2,   2,   2,   3,   3,   3,   4,   4,   4,   5,   5,
      5,   6,   6,   6,   7,   7,   7,   8,   8,   8,   9,   9,   9,  10,  10,  10,
     11,  11,  11,  12,  12,  12,  13,  13,  13,  14,  14,  14,  15,  15,  15,  16,
     16,  16,  17,  17,  17,  18,  18,  18,  19,  19,  19,  20,  20,  20,  21,  21,
     21,  22,  22,  23,  24,  24,  25,  26,  26,  27,  28,  28,  29,  30,  30,  31,
     32,  32,  33,  34,  35,  35,  36,  37,  38,  38,  39,  40,  41,  41,  42,  43,
     44,  45,  46,  46,  47,  48,  49,  50,  51,  52,  53,  53,  54,  55,  56,  57,
     58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,
     74,  75,  76,  77,  78,  79,  80,  81,  82,  83,  84,  86,  87,  88,  89,  90,
     91,  92,  93,  95,  96,  97,  98,  99, 100, 102, 103, 104, 105, 107, 108, 109,
    110, 111, 113, 114, 115, 116, 118, 119, 120, 122, 123, 124, 126, 127, 128, 129,
    131, 132, 134, 135, 136, 138, 139, 140, 142, 143, 145, 146, 147, 149, 150, 152,
    153, 154, 156, 157, 159, 160, 162, 163, 165, 166, 168, 169, 171, 172, 174, 175,
    177, 178, 180, 181, 183, 184, 186, 188, 189, 191, 192, 194, 195, 197, 199, 200,
    202, 204, 205, 207, 208, 210, 212, 213, 215, 217, 218, 220, 222, 224, 225, 227,
    229, 230, 232, 234, 236, 237, 239, 241, 243, 244, 246, 248, 250, 251, 253, 255
};

// Farbe bei voller Helligkeit korrigieren (Eingang: stärkster Kanal = 255).
static void iso_color_full(uint8_t r, uint8_t g, uint8_t b, uint8_t *R, uint8_t *G, uint8_t *B) {
    // Farbabgleich
    r = (uint16_t)r * ISO_BALANCE_R / 255;
    g = (uint16_t)g * ISO_BALANCE_G / 255;
    b = (uint16_t)b * ISO_BALANCE_B / 255;
#    if ISO_PURPLE_RICH > 0
    // Lila-Gewicht: 0 bei reinem Blau und bei Magenta (Rot = Blau), Maximum bei
    // Rot = halbes Blau, nur mit wenig Grün.
    uint8_t  r0 = r;
    uint16_t w  = 0;
    if (b > r && r > g) {
        uint16_t d = (uint16_t)(((r - g) < (b - r)) ? (r - g) : (b - r)) * 2;
        if (d > b) d = b;
        w = d * ISO_PURPLE_RICH / b;
    }
#    endif
#    if ISO_PURPLE_RED_CUT > 0
    // Rotanteil bei Rot-Blau-Mischungen senken (Magenta/Pink/Lila nicht rosa-blass).
    {
        uint8_t rb  = (r < b) ? r : b;
        uint8_t mix = (rb > g) ? (uint8_t)(rb - g) : 0;
        r           = (uint8_t)(r - (uint16_t)mix * ISO_PURPLE_RED_CUT / 255);
    }
#    endif
    *R = pgm_read_byte(&iso_color_table[r]);
    *G = pgm_read_byte(&iso_color_table[g]);
    *B = pgm_read_byte(&iso_color_table[b]);
#    if ISO_PURPLE_RICH > 0
    // Lila satt halten: Rot im Lila-Bereich ins eingestellte Verhältnis zurückführen.
    if (w) {
        int32_t rr = (int32_t)r0 * (*B) / b;
        *R         = (uint8_t)(*R + ((rr - (int32_t)(*R)) * (int32_t)w) / 255);
    }
#    endif
}

// Letzter von uns ausgegebener Wert je LED. Unverändert = in diesem Durchlauf
// nicht neu gezeichnet, dann nicht nochmal korrigieren.
static rgb_led_t iso_gamma_shadow[RGB_MATRIX_LED_COUNT];

static void iso_gamma_apply(uint8_t led_min, uint8_t led_max) {
    for (uint8_t i = led_min; i < led_max && i < RGB_MATRIX_LED_COUNT; i++) {
        rgb_led_t *led = &rgb_matrix_ws2812_array[i];
        if (led->r == iso_gamma_shadow[i].r && led->g == iso_gamma_shadow[i].g && led->b == iso_gamma_shadow[i].b) continue;

        uint8_t r = led->r, g = led->g, b = led->b;
        uint8_t m = r > g ? (r > b ? r : b) : (g > b ? g : b);
        uint8_t R = 0, G = 0, B = 0;

        if (m >= ISO_MIN_INPUT) {
            // 1. Farbe auf volle Helligkeit normieren und korrigieren
            uint8_t cr, cg, cb;
            iso_color_full((uint8_t)(((uint16_t)r * 255 + m / 2) / m), (uint8_t)(((uint16_t)g * 255 + m / 2) / m), (uint8_t)(((uint16_t)b * 255 + m / 2) / m), &cr, &cg, &cb);
            // 2. Helligkeit über eigene Kurve
            uint8_t M = pgm_read_byte(&iso_level_table[m]);
            // 3. Farbe x Helligkeit
            R = ((uint16_t)cr * M + 127) / 255;
            G = ((uint16_t)cg * M + 127) / 255;
            B = ((uint16_t)cb * M + 127) / 255;
            // 4. Wichtiger Farbanteil fällt weg -> ganz aus statt falscher Farbe
            uint8_t cm = cr > cg ? (cr > cb ? cr : cb) : (cg > cb ? cg : cb);
            if ((cr && (uint16_t)cr * 6 >= cm && !R) || (cg && (uint16_t)cg * 6 >= cm && !G) || (cb && (uint16_t)cb * 6 >= cm && !B)) {
                R = G = B = 0;
            }
        }
        led->r              = R;
        led->g              = G;
        led->b              = B;
        iso_gamma_shadow[i] = *led;
    }
}
#endif

bool rgb_matrix_indicators_advanced_kb(uint8_t led_min, uint8_t led_max) {
    bool ret = iso_indicators_advanced_impl(led_min, led_max);
#ifdef ISO_GAMMA_ENABLE
    iso_gamma_apply(led_min, led_max);
#endif
    return ret;
}
