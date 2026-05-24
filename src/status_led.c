#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"

#include "ws2812.pio.h"
#include "status_led.h"

#define WS2812_PIN 16
#define WS2812_UPDATE_MS 100
#define BLINK_PERIOD 5
#define ERROR_BLINK

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(pio0, 0, pixel_grb << 8u);
}

static inline uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t) (r) << 8) | ((uint32_t) (g) << 16) | (uint32_t) (b);
}

static inline void parse_urgb_u32(uint32_t urgb, uint8_t *r, uint8_t *g, uint8_t *b) {
    *r = (urgb >> 8) & 0xff;
    *g = (urgb >> 16) & 0xff;
    *b = urgb & 0xff;
}

static uint32_t status_led_colour = 0;
static uint32_t error_colour = 0;
static bool status_led_blink = false;
static bool status_led_error = false;
static uint8_t blink_counter = 0;
static struct repeating_timer status_led_update_timer;


bool status_led_update_timer_callback(struct repeating_timer *t) {
    if (status_led_error) {
#ifdef ERROR_BLINK
        if (blink_counter >= BLINK_PERIOD) put_pixel(0);
        else put_pixel(error_colour);

        blink_counter++;
        if (blink_counter >= (BLINK_PERIOD*2)) blink_counter = 0;
#else
        put_pixel(error_colour);
#endif
        return true;
    }
    
    if (status_led_blink && blink_counter >= BLINK_PERIOD) put_pixel(0);
    else put_pixel(status_led_colour);

    if (status_led_blink) {
        blink_counter++;
        if (blink_counter >= (BLINK_PERIOD*2)) blink_counter = 0;
    } else blink_counter = 0;

    return true;
}

void init_status_led(PIO pio) {
    int sm = pio_claim_unused_sm(pio, true); // panic if none available
    uint offset = pio_add_program(pio, &ws2812_program);

    ws2812_program_init(pio, sm, offset, WS2812_PIN, 800000, false);

    status_led_colour = urgb_u32(0x10, 0x00, 0x10);
    put_pixel(status_led_colour);

    error_colour = urgb_u32(0xff, 0x00, 0x00);

    add_repeating_timer_ms(WS2812_UPDATE_MS, status_led_update_timer_callback, NULL, &status_led_update_timer);
}

void set_status_led(uint8_t r, uint8_t g, uint8_t b, bool blink) {
    status_led_colour = urgb_u32(r, g, b);
    status_led_blink = blink;
}

void get_status_led_state(uint8_t *r, uint8_t *g, uint8_t *b, bool *blink) {
    parse_urgb_u32(status_led_colour, r, g, b);
    *blink = status_led_blink;
}

void set_status_led_error(bool has_error) {
    status_led_error = has_error;
}