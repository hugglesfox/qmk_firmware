/*
Copyright 2023 Hayley Hughes

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "quantum.h"
#include "hal.h"

// #ifdef RGB_MATRIX_ENABLE

#define SDI_RED PB14
#define SDI_GREEN PB13
#define SDI_BLUE PB12
#define LE PD3
#define DCLK PD4

#define ROW_0_LED_COUNT 15
#define ROW_1_LED_COUNT 15
#define ROW_2_LED_COUNT 14
#define ROW_3_LED_COUNT 12
#define ROW_4_LED_COUNT 12

// FIXME: Replace this with the actual keyboard config
led_config_t g_led_config = { {
  // Key Matrix to LED Index
  {   5, NO_LED, NO_LED,   0 },
  { NO_LED, NO_LED, NO_LED, NO_LED },
  {   4, NO_LED, NO_LED,   1 },
  {   3, NO_LED, NO_LED,   2 }
}, {
  // LED Index to Physical Position
  { 188,  16 }, { 187,  48 }, { 149,  64 }, { 112,  64 }, {  37,  48 }, {  38,  16 }
}, {
  // LED Index to Flag
  1, 4, 4, 4, 4, 1
} };

static uint8_t color_data[RGB_MATRIX_LED_COUNT][3];

static uint16_t led_data[5 * 16 * 3] = {0};

static PWMConfig pwmCFG = {
    2400000,  // 24 MHz (divides nicely by 72 MHz which is the MCU clock freq)
    10,
    NULL,
    {
        {PWM_OUTPUT_ACTIVE_HIGH, NULL, NUC123_PWM_CH0_PIN_PA12},  // Enable channel 0 (PA12)
        {PWM_OUTPUT_DISABLED, NULL, 0},
        {PWM_OUTPUT_DISABLED, NULL, 0},
        {PWM_OUTPUT_DISABLED, NULL, 0}
    }
};

static void enable_configuration_mode(void) {
    for (int i = 0; i < 16; i++) {
        // Hold LE high for 15 DCLK cycles
        if (i == 1) {
            LE = PAL_HIGH;
        }

        DCLK = PAL_HIGH;
        DCLK = PAL_LOW;
    }

    LE = PAL_LOW;
}

static void write_configuration(const short config) {
    enable_configuration_mode();

    // Send config data
    for (int i = 15; i >= 0; i--) {
        int bit = config >> i & 1;
        SDI_RED = bit;
        SDI_GREEN = bit;
        SDI_BLUE = bit;

        // Hold LE high for 11 DCLK cycles
        if (i == 5) {
            LE = PAL_HIGH;
        }

        DCLK = PAL_HIGH;
        DCLK = PAL_LOW;
    }

    LE = PAL_LOW;
}

// static int set_row(const int row, const int v) {
//     int val = v & 1;
//     switch (row) {
//     case 0:
//         PC4 = val;
//         return ROW_0_LED_COUNT;
//     case 1:
//         PC5 = val;
//         return ROW_1_LED_COUNT;
//     case 2:
//         PB3 = val;
//         return ROW_2_LED_COUNT;
//     case 3:
//         PB2 = val;
//         return ROW_3_LED_COUNT;
//     case 4:
//         PB8 = val;
//         return ROW_4_LED_COUNT;
//     default:
//         return -1;
//     }
// }

static void flush(void) {}

static void set_color(int index, uint8_t red, uint8_t green, uint8_t blue) {
    color_data[index][0] = red;
    color_data[index][1] = green;
    color_data[index][2] = blue;
}

static void set_color_all(uint8_t red, uint8_t green, uint8_t blue) {
    for (int i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        color_data[i][0] = red;
        color_data[i][1] = green;
        color_data[i][2] = blue;
    }
}

static void update_leds(void) {
    // Enable the row
    // int led_count = set_row(0, PAL_HIGH);

    // 16 bits per led
    for (int i = 0; i < 16; i++) {
        // MSB first
        // SDI_RED = led_data[led] >> j & 1;
        // led++;
        // SDI_GREEN = led_data[led] >> j & 1;
        // led++;
        // SDI_BLUE = led_data[led] >> j & 1;
        // led++;

        SDI_RED = PAL_HIGH;
        // SDI_GREEN = PAL_HIGH;
        // SDI_BLUE = PAL_HIGH;

        // Hold LE high for 1 clock cycle to latch data
        if (i == 15) {
            LE = PAL_HIGH;
        }

        DCLK = PAL_HIGH;
        DCLK = PAL_LOW;
    }

    LE = PAL_LOW;

    SDI_RED = PAL_LOW;
    // SDI_GREEN = PAL_LOW;
    // SDI_BLUE = PAL_LOW;

    // Global latch to enable LEDS
    for (int i = 0; i < 16; i++) {
        // Hold LE high for 3 DCLK cycles
        if (i == 13) {
            LE = PAL_HIGH;
        }

        DCLK = PAL_HIGH;
        DCLK = PAL_LOW;
    }
    // Reset LE Low
    LE = PAL_LOW;
}

static void init(void) {
    // Disable LED controllers
    PD5 = PAL_HIGH;

    // Disable all rows but row 0
    PC4 = PAL_HIGH;
    PC5 = PAL_LOW;
    PB3 = PAL_LOW;
    PB2 = PAL_LOW;
    PD8 = PAL_LOW;

    // Reset all data lines
    LE = PAL_LOW;
    DCLK = PAL_LOW;
    SDI_RED = PAL_LOW;
    SDI_GREEN = PAL_LOW;
    SDI_BLUE = PAL_LOW;

    pwmStart(&PWMD1, &pwmCFG);
    pwmEnableChannel(&PWMD1, 0, 5);  // 50% duty cycle

    // Enable the LED controllers
    PD5 = PAL_LOW;

    write_configuration(0b1000010000000000u);

    led_data[0] = 0xFFFF;

    update_leds();
}


const rgb_matrix_driver_t rgb_matrix_driver = {
    .init = init,
    .flush = flush,
    .set_color = set_color,
    .set_color_all = set_color_all
};

// #endif // RGB_MATRIX_ENABLE
