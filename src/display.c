#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "ssd1306.h"

#include "display.h"

#include "Pix32_Font.h"
#include "Pix32_Inv_Font.h"

#define POPUP_SHOW 2
#define POPUP_SHOWN 1

static ssd1306_t disp;

static bool dirty = true;
static struct {
    int temperature_10mC;
    struct {
        char msg[32];
        uint8_t x;
        uint8_t y;
        uint8_t scale;
        absolute_time_t hide_time;
        uint8_t state;
    } popup;
} st = {0};

static void draw_string(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, const uint8_t *font, const char *s);
static void draw_string_with_inverts(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, const char *s);

static void draw_temp();

void update_temp(float temp_c) {
    int temp_10mC = 100 * temp_c;
    dirty |= st.temperature_10mC != temp_10mC;
    st.temperature_10mC = temp_10mC;
}

void init_display(void) {
    disp.external_vcc=false;
#ifdef SH1106
    disp.is_sh1106=true;
#endif
    ssd1306_init(&disp, 128, 64, 0x3C, i2c0);
    /* Possible changes to init cmds
    
    SET_CONTRAST,
    0xCF, // From https://github.com/adafruit/Adafruit_SSD1306/blob/master/Adafruit_SSD1306.cpp#L598
    ...
    SET_VCOM_DESEL,
    0x40, // From https://github.com/adafruit/Adafruit_SSD1306/blob/master/Adafruit_SSD1306.cpp#L618
    */

    dirty = true;
    ssd1306_clear(&disp);
    // ssd1306_draw_square(&disp, 0, 0, 128, 64);
    ssd1306_show(&disp);
}

void show_popup(uint8_t x, uint8_t y, uint8_t scale, uint16_t show_time_ms, const char *msg) {
    st.popup.x = x;
    st.popup.y = y;
    st.popup.scale = scale;
    strncpy(st.popup.msg, msg, sizeof(st.popup.msg));
    st.popup.hide_time = make_timeout_time_ms(show_time_ms);
    st.popup.state = POPUP_SHOW;
    dirty = true;
}

void show_popup_centered(uint8_t x, uint8_t y, uint8_t scale, uint16_t show_time_ms, uint8_t w, uint8_t h, const char *msg) {
    int16_t x2 = x*2; // Center using half pixels, then floor to nearest pixel
    int16_t y2 = y*2;

    x2 -= scale * w * Pix32_Font[1];
    y2 -= scale * h * Pix32_Font[0];

    show_popup(x2/2, y2/2, scale, show_time_ms, msg);
}

void refresh_display(void) {
    if (!dirty) return;
    dirty = false;

    if (st.popup.state) {
        if (absolute_time_diff_us(get_absolute_time(), st.popup.hide_time) < 0) {
            st.popup.state = 0;
        } else {
            if (st.popup.state == POPUP_SHOW) {
                st.popup.state = POPUP_SHOWN;
                ssd1306_clear(&disp);
                draw_string_with_inverts(&disp, st.popup.x, st.popup.y, st.popup.scale, st.popup.msg);
                ssd1306_show(&disp);
            }
            dirty = true;
            return;
        }
    }

    ssd1306_clear(&disp);

    draw_temp();
    
    ssd1306_show(&disp);
}

static void draw_string(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, const uint8_t *font, const char *s) {
    uint32_t sx = x;
    uint32_t dx = (font[1]+font[2]) * scale;
    uint32_t dy = (font[0]+font[2]) * scale;

    while (*s != 0) {
        if (*s == '\n') {
            x = sx;
            y += dy;
        } else {
            ssd1306_draw_char_with_font(p, x, y, scale, font, *s);
            x += dx;
        }
        s++;
    }
}

static void draw_string_with_inverts(ssd1306_t *p, uint32_t x, uint32_t y, uint32_t scale, const char *s) {
    uint32_t sx = x;
    uint32_t dx = (Pix32_Font[1]+Pix32_Font[2]) * scale;
    uint32_t dy = (Pix32_Font[0]+Pix32_Font[2]) * scale;
    bool inverted = false;

    while (*s != 0) {
        if (*s == '\n') {
            x = sx;
            y += dy;
        } else if (*s =='\a') { // Bell character
            inverted = !inverted;
        } else {
            ssd1306_draw_char_with_font(p, x, y, scale, inverted ? Pix32_Inv_Font : Pix32_Font, *s);
            x += dx;
        }
        s++;
    }
}

static void draw_temp() {
    int temp_C = st.temperature_10mC;
    uint8_t temp_10mC = abs(temp_C) % 100;
    temp_C /= 100;

    char s[20];
    uint8_t n = sizeof(s);

    // ` is actually the degrees symbol
    // snprintf(s, sizeof(s), "%2d.%02d`C", temp_C, temp_10mC);
    snprintf(s, sizeof(s), "%2d.%01d`C", temp_C, temp_10mC / 10);

    uint8_t len = strlen(s);

    draw_string(&disp, 64-(strlen(s)*Pix32_Font[1]), 32-Pix32_Font[0], 2, Pix32_Font, s);
}
