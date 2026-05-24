#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "kvstore.h"

#include "settings.h"

static const settings_t default_settings = {
    .ver = 1, // Settings version, increment when changing settings struct
    .disp_contrast = 0xff,
    .graph_en = 0,
    .led_brightness = 0x80,
    .measure_interval_ms = 100,
};

settings_t settings = default_settings;
bool settings_invalid = false;

void load_settings(void) {
    printf("Loading settings...\n");
    kvs_init();

    size_t len = 0;
    int rc = kvs_get("S", &settings, sizeof settings, &len);
    if (rc != KVSTORE_SUCCESS || len != sizeof settings || settings.ver != default_settings.ver) {
        printf("Failed to load settings\n");
        memcpy(&settings, &default_settings, sizeof settings);
        settings_invalid = true;
        return;
    }
    printf("Settings loaded\n");
}

void save_settings(void) {
    printf("Saving settings...\n");
    int rc = kvs_set("S", &settings, sizeof settings);
    if (rc != KVSTORE_SUCCESS) {
        printf("Failed to save settings\n");
        return;
    }
    printf("Settings saved\n");
    settings_invalid = false;
}

void load_default_settings(void) {
    memcpy(&settings, &default_settings, sizeof settings);
}
