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

#include "display.h"

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

    sleep_ms(500);

    // Start watchdog
    watchdog_enable(WATCHDOG_TIMEOUT_MS, true); // Pause watchdog on debug
    
    printf("Initializing peripherals...\n");
    setup_i2c();
    init_display();
    adc_init();

    adc_gpio_init(TEMP_SENSOR_PIN);
    adc_select_input(ADC_CHAN);

    absolute_time_t measure_delay = get_absolute_time();

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
                // Process command if not empty
                if (cmd_len > 0) {
                    cmd_buf[cmd_len] = 0;
                    cmd_len = 0;
                    process_command(cmd_buf);
                }
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
            measure_delay = make_timeout_time_ms(100); // Measure every 100ms

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
        }

        refresh_display();
    }
}

static void process_command(const char *cmd) {
    if (strcmp(cmd, "update") == 0) {
        printf("Entering Bootloader...\n");
        show_popup_centered(63, 31, 2, 1000, 6, 2, "Update\n Mode");
        refresh_display();
        sleep_ms(100);
        enter_bootloader();
    } else if (strcmp(cmd, "log_on") == 0) {
        printf("Logging Enabled\n");
        logging_enabled = true;
    } else if (strcmp(cmd, "log_off") == 0) {
        printf("Logging Disabled\n");
        logging_enabled = false;
    } else if (strcmp(cmd, "help") == 0) {
        printf("Available comamands:\n"
               "- update : Enters the bootloader for updating firmware\n"
               "- log_on : Enables voltage/temperature logging\n"
               "- log_off : Disables voltage/temperature logging\n"
               "- help : Prints this help message\n"
        );
    } else {
        printf("Unknown command: %s\n", cmd);
        printf("Type 'help' to view a list of valid commands\n");
    }
}

