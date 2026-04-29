#pragma once

#include <stdint.h>
#include <stdbool.h>

// Use SH1106 instead of SSD1306
#define SH1106

void init_display(void);
void refresh_display(void);
void update_contrast(void);

void show_popup(uint8_t x, uint8_t y, uint8_t scale, uint16_t show_time_ms, const char *msg);
void show_popup_centered(uint8_t x, uint8_t y, uint8_t scale, uint16_t show_time_ms, uint8_t w, uint8_t h, const char *msg);

void update_temp(float temp_c);

