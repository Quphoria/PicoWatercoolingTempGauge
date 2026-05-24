#pragma once 

#include <stdint.h>
#if !PICO_NO_HARDWARE
#include "hardware/pio.h"
#endif

void init_status_led(PIO pio);
void set_status_led(uint8_t r, uint8_t g, uint8_t b, bool blink);
void get_status_led_state(uint8_t *r, uint8_t *g, uint8_t *b, bool *blink);
void set_status_led_error(bool has_error);
