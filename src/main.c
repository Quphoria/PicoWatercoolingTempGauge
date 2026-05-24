#include <stdio.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/unique_id.h"
// To allow reset to bootloader
#include "pico/bootrom.h"
#include "pico/time.h"
#include "hardware/i2c.h"
#include "hardware/watchdog.h"
#include "hardware/adc.h"

#include "status_led.h"
#include "display.h"
#include "settings.h"

#define FIRMWARE_VERSION "1.0.1"
#define WATCHDOG_TIMEOUT_MS 2000
#define WATCHDOG_UPDATE_TIMER_MS 500

#define BOARD_ID_STR_LEN (2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1)

static bool initialized = false;
static bool alive = true;

static void process_command(const char *cmd);

bool watchdog_update_timer_callback(struct repeating_timer *t) {
    if (!initialized) return true;
    if (!alive) return true;
    alive = false;

    watchdog_update();

    return true;
}

static struct repeating_timer watchdog_update_timer;

void setup_i2c(void) {
    i2c_init(i2c0, 400 * 1000); // 400kHz I2C Fast Mode
    gpio_set_function(12, GPIO_FUNC_I2C);
    gpio_set_function(13, GPIO_FUNC_I2C);
    gpio_pull_up(12);
    gpio_pull_up(13);
}

static void enter_bootloader(void) {
    // Enter the bootloader
    reset_usb_boot(0,0); // No activity led
}

static const uint LED_PIN = 25;
static const uint TEMP_SENSOR_PIN = 26;
static const uint ADC_CHAN = 0;


static const double TK = 273.15;
static const double B = 3435; // B25_80
static const double T0 = TK + 25;
static const double R0 = 10e3;
static const double Rdiv = 10e3;
static const double VREF = 3.3;

static float voltage_to_temp(float voltage) {
    // V = VREF * R/(R+Rdiv)
    // (R+Rdiv)*(V/VREF) = R
    // Rdiv*V/VREF = R*(1 - V/VREF)
    // R = Rdiv*V/VREF/(1-V/VREF)
    const double Vratio = voltage/VREF;
    const double R = Rdiv * Vratio / (1 - Vratio);
    // (math.log(10e3)-math.log(1.4513e3))/(1/(25+273.15) - 1/(85+273.15))
    // B = (ln(R0)-ln(RT)) / (1/T0 - 1/T)
    // B/T = ln(RT) - ln(R0) + B/T0
    // T = B / (ln(RT) - ln(R0) + B/T0)
    const double T = B / (log(R) - log(R0) + B/T0) - TK;
    return T;
}

static bool logging_enabled = false;
static absolute_time_t measure_delay;

int main() {
    sleep_ms(500); // Wait for usb to disconnect before connecting
    stdio_init_all();

    printf("\n\n\n\n\n");
    printf("PicoTempGauge\n");

    if (watchdog_caused_reboot() &&
        watchdog_enable_caused_reboot()) printf("Rebooted by Watchdog!\n");

    pico_unique_board_id_t board_id;
    char board_id_str[BOARD_ID_STR_LEN] = "";
    pico_get_unique_board_id(&board_id);
    pico_get_unique_board_id_string(board_id_str, BOARD_ID_STR_LEN);
    printf("Board ID: %s\n", board_id_str);

    add_repeating_timer_ms(WATCHDOG_UPDATE_TIMER_MS, watchdog_update_timer_callback, NULL, &watchdog_update_timer);

    // Start watchdog
    watchdog_enable(WATCHDOG_TIMEOUT_MS, true); // Pause watchdog on debug
    
    load_settings();

    printf("Initializing peripherals...\n");
    init_status_led(pio0);
    setup_i2c();
    init_display();
    adc_init();

#ifndef SH1106
    if (settings.disp_contrast == 0) {
        show_popup_centered(63, 31, 2, 2000, 7, 2, "Display\n  Off");
    }
#endif

    adc_gpio_init(TEMP_SENSOR_PIN);
    adc_select_input(ADC_CHAN);

    measure_delay = get_absolute_time();

    char cmd_buf[32];
    uint8_t cmd_len = 0;

    initialized = true;

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);
    int led = 0;
    while (true) {
        alive = true;

        while (true) {
            int ch = getchar_timeout_us(0);
            if (ch == PICO_ERROR_TIMEOUT) break;

            char c = ch;
            putchar(c); // Echo character
            if (cmd_len >= sizeof(cmd_buf)) {
                printf("Error: Command buffer overflow\n");
                cmd_len = 0;
            } else if (c == '\n' || c == '\r') {
                printf("\n");
                // Process command (will print help prompt if empty)
                cmd_buf[cmd_len] = 0;
                cmd_len = 0;
                process_command(cmd_buf);
            } else if (c == '\b') {
                // Backspace
                if (cmd_len > 0) cmd_len--;
                printf(" \b"); // Clear previous character in terminal
            } else {
                // Append to buffer
                cmd_buf[cmd_len++] = c;
            }
        }

        led = ~led;
        gpio_put(LED_PIN, led);
        // sleep_ms(50);

        if (absolute_time_diff_us(get_absolute_time(), measure_delay) < 0) {
            measure_delay = make_timeout_time_ms(settings.measure_interval_ms); // Measure every 100ms

            // VREF
            //  | 
            // Rdiv
            //  |-- voltage
            //  RT
            //  |
            // GND

            uint16_t result = adc_read();
            float v = result * VREF / (1<<12);
            // v = VREF * 10/20;            // 25c
            // v = VREF * 1.4513/11.4513;   // 85c

            const float t = voltage_to_temp(v);

            if (logging_enabled) {
                printf("V=%0.3fV T=%0.1f*C\n", v, t);
            }

            update_temp(t);

            set_status_led_error(t < -10 || t > 120); // Temperature goes -55 when open circuit, or 190 if short circuit

            if (settings.led_brightness) {
                uint8_t r = 0;
                uint8_t g = 0;
                uint8_t b = 0;
                // 15 (blue) - 30 (cyan) - 45 (green) - 60 yellow - 75 (red)
                // See: https://en.wikipedia.org/wiki/File:HSV-RGB-comparison.svg
                if (t <= 15) {
                    b = settings.led_brightness;
                } else if (t < 30) { // 15-30
                    float fr = (t - 15) / 15.0;
                    g = settings.led_brightness * fr;
                    b = settings.led_brightness;
                } else if (t < 45) { // 30-45
                    float fr = (45 - t) / 15.0;
                    g = settings.led_brightness;
                    b = settings.led_brightness * fr;
                } else if (t < 60) { // 45-60
                    float fr = (t - 45) / 15.0;
                    r = settings.led_brightness * fr;
                    g = settings.led_brightness;
                } else if (t < 75) { // 60-75
                    float fr = (75 - t) / 15.0;
                    r = settings.led_brightness;
                    g = settings.led_brightness * fr;
                } else { // 75+
                    r = settings.led_brightness;
                }
                set_status_led(r, g, b, false);
            } else {
                set_status_led(0, 0, 0, false);
            }
        }

        refresh_display();
    }
}

static void process_command(const char *cmd) {
    if (strcmp(cmd, "update") == 0) {
        set_status_led_error(false);
        set_status_led(0x80, 0, 0xc0, false);
        printf("Entering Bootloader...\n");
        show_popup_centered(63, 31, 2, 1000, 6, 2, "Update\n Mode");
        refresh_display();
        sleep_ms(150); // Long enough for status led to update
        enter_bootloader();
    } else if (strcmp(cmd, "log_on") == 0) {
        printf("Logging Enabled\n");
        logging_enabled = true;
    } else if (strcmp(cmd, "log_off") == 0) {
        printf("Logging Disabled\n");
        logging_enabled = false;
    } else if (strcmp(cmd, "contrast") == 0) {
        printf("Contrast: %d\n", settings.disp_contrast);
    } else if (strstr(cmd, "contrast ") == cmd) {
        int x;
        sscanf(cmd, "contrast %d", &x);
        if (x < 0 || x > 0xff) {
            printf("Error: Value must be between 0 and 255\n");
        } else {
            settings.disp_contrast = x;
            save_settings();
        }
        update_contrast();
    } else if (strcmp(cmd, "graph 0") == 0) {
        settings.graph_en = 0;
        save_settings();
    } else if (strcmp(cmd, "graph 1") == 0) {
        settings.graph_en = 1;
        save_settings();
    } else if (strcmp(cmd, "graph reset") == 0) {
        reset_graph();
    } else if (strcmp(cmd, "graph") == 0) {
        printf("Graph: %d\n", settings.graph_en);
    } else if (strcmp(cmd, "led") == 0) {
        printf("LED: %d\n", settings.led_brightness);
    } else if (strstr(cmd, "led ") == cmd) {
        int x;
        sscanf(cmd, "led %d", &x);
        if (x < 0 || x > 0xff) {
            printf("Error: Value must be between 0 and 255\n");
        } else {
            settings.led_brightness = x;
            save_settings();
        }
    } else if (strcmp(cmd, "rate") == 0) {
        printf("Rate: %d\n", settings.measure_interval_ms);
    } else if (strstr(cmd, "rate ") == cmd) {
        int x;
        sscanf(cmd, "rate %d", &x);
        if (x < 25 || x > 15000) {
            printf("Error: Value must be between 25 and 15000\n");
        } else {
            settings.measure_interval_ms = x;
            save_settings();
            measure_delay = make_timeout_time_ms(settings.measure_interval_ms);
        }
    } else if (strcmp(cmd, "defaults") == 0) {
        load_default_settings();
        save_settings();
    } else if (strcmp(cmd, "help") == 0) {
        printf("Firmware: " FIRMWARE_VERSION "\n%s"
               "Available comamands:\n"
               "  update            - Enters the bootloader for updating firmware\n"
               "  log_on            - Enables voltage/temperature logging\n"
               "  log_off           - Disables voltage/temperature logging\n"
               "  contrast          - Gets the current display contrast\n"
               "  contrast [0-255]  - Sets the current display contrast\n"
               "  graph             - Gets the graphing enable state\n"
               "  graph [0-1]       - Enables/Disables graphing\n"
               "  graph reset       - Resets the graph\n"
               "  led               - Gets the current led brightness\n"
               "  led [0-255]       - Sets the current led brightness\n"
               "  rate              - Gets the measurement interval in ms\n"
               "  rate [25+]        - Sets the measurement interval in ms\n"
               "  defaults          - Loads default settings\n"
               "  help              - Prints this help message\n",
               settings_invalid ? "WARNING: Failed to load settings on startup\n" : ""
        );
    } else {
        if (strlen(cmd) != 0) {
            printf("Unknown command: %s\n", cmd);
        }
        printf("Type 'help' to view a list of valid commands\n");
    }
}

