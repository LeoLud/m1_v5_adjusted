// Copyright 2024 yangzheng20003 (@yangzheng20003)
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef ENABLE_RGB_MATRIX_RGBR_PLAY
RGB_MATRIX_EFFECT(RGBR_PLAY)
#    ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

bool RGBR_PLAY(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    extern void rgbrec_play(uint8_t led_min, uint8_t led_max);
    rgbrec_play(led_min, led_max);

    return rgb_matrix_check_finished_leds(led_max);
}

#    endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#endif

// OpenRGB "Direct" mode: per-LED colours sent over VialRGB (vialrgb/vialrgb.c).
RGB_MATRIX_EFFECT(OPENRGB_DIRECT)
#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

HSV g_openrgb_direct[RGB_MATRIX_LED_COUNT];

bool OPENRGB_DIRECT(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        // ISO-Port: Gesamthelligkeit (Fn-Tasten, VIA) wirkt auch im Direct-Modus.
        // Bei maximaler Helligkeit erscheinen die Farben genau wie in OpenRGB gesetzt.
        // Absicherung: Steht die Gesamthelligkeit auf 0 (z. B. von OpenRGB gesetzt),
        // Farben voll anzeigen statt alles dunkel.
        HSV     hsv = g_openrgb_direct[i];
        uint8_t gv  = rgb_matrix_get_val();
        if (gv == 0) gv = RGB_MATRIX_MAXIMUM_BRIGHTNESS;
        hsv.v = (uint16_t)hsv.v * gv / RGB_MATRIX_MAXIMUM_BRIGHTNESS;
        RGB rgb = hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }

    return rgb_matrix_check_finished_leds(led_max);
}

#endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
