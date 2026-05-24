#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct __packed__ {
    const uint8_t ver;              // Settings version, increment when changing settings struct
    uint8_t disp_contrast;          // The display contrast
    uint8_t graph_en;               // Enable graph (0: off, 1: on)
    uint8_t led_brightness;         // Status led brightbess
    uint16_t measure_interval_ms;   // Measure interval in ms
} settings_t;

extern settings_t settings;
extern bool settings_invalid;

void load_settings(void);
void save_settings(void);
void load_default_settings(void);
