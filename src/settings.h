#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct __packed__ {
    const uint8_t ver;      // Settings version, increment when changing settings struct
    uint8_t disp_contrast;  // The display contrast
} settings_t;

extern settings_t settings;

void load_settings(void);
void save_settings(void);
void load_default_settings(void);
