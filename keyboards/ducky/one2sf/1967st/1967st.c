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

#define DATA_LATCH 1
#define GLOBAL_LATCH 7

static uint8_t color_data[RGB_MATRIX_LED_COUNT][3];

// 5 rows, 16 led channels + 1 for global latch per row
static uint16_t sdi_red_buf[5][17];
static uint16_t sdi_green_buf[5][17];
static uint16_t sdi_blue_buf[5][17];

// Takes 272 pulses to clock in all the data
static uint16_t dclk_pulse_count = 0;
static uint8_t current_row = 0;

static void setup_pwm(void) {
    // Use the HCLK
    // Note we need to 0 the bits first as the clksel register has a reset value
    // of 0xFFFF_FFFF
    CLK->CLKSEL1 &= ~CLK_CLKSEL1_PWM01_S_Msk;
    CLK->CLKSEL1 |= 2 << CLK_CLKSEL1_PWM01_S_Pos;

    CLK->CLKSEL2 &= ~CLK_CLKSEL2_PWM01_S_E_Msk;
    CLK->CLKSEL2 |= 2 << CLK_CLKSEL2_PWM01_S_E_Pos;

    CLK->CLKSEL1 &= ~CLK_CLKSEL1_PWM23_S_Msk;
    CLK->CLKSEL1 |= 2 << CLK_CLKSEL1_PWM23_S_Pos;

    CLK->CLKSEL2 &= ~CLK_CLKSEL2_PWM23_S_E_Msk;
    CLK->CLKSEL2 |= 2 << CLK_CLKSEL2_PWM23_S_E_Pos;

    // Enable the PWM peripheral clock
    CLK->APBCLK |= (1 << CLK_APBCLK_PWM01_EN_Pos | 1 << CLK_APBCLK_PWM23_EN_Pos);

    // Enable PWM interrupt vector
    // Interrupt priority value taken from chibios defaults
    nvicEnableVector(NUC123_PWMA_NUMBER, 3);
}

// PWM channel 0
static void setup_gclk(void) {
    // Set clock division to 1
    PWMA->CSR |= 4 << PWM_CSR_CSR0_Pos;

    // Set pin PA12 to PWM CH0
    SYS->GPA_MFP |= 1 << 12;

    // Enable channel 0 output
    PWMA->POE |= 1 << PWM_POE_PWM0_Pos;

    // Set prescaler to 1
    PWMA->PPR |= 1 << PWM_PPR_CP01_Pos;

    // Enable auto reload
    PWMA->PCR |= 1 << PWM_PCR_CH0MOD_Pos;

    // Set freq to 9MHz and 50% duty cycle (it's just a nice clock)
    //
    // freq = HCLK/[(prescale+1)*(clock divider)*(CNR+1)]
    // 72MHz/(1 + 1)*(1)*(3+1)
    //
    // duty = (CMR+1)/(CNR+1)
    // (1+1)/(3+1)
    PWMA->CNR0 = 3;
    PWMA->CMR0 = 1;

    // Start PWM channel 0
    PWMA->PCR |= 1 << PWM_PCR_CH0EN_Pos;
}

// PWM channel 2
static void setup_dclk(void) {
    // Set clock division to 1
    PWMA->CSR |= 3 << PWM_CSR_CSR0_Pos;

    // Set prescaler to 255
    PWMA->PPR |= 0xFF << PWM_PPR_CP23_Pos;

    // Enable duty and reset interrupts
    PWMA->PIER |= (1 << PWM_PIER_PWMDIE2_Pos) | (1 << PWM_PIER_PWMIE2_Pos);

    // Enable auto reload
    PWMA->PCR |= 1 << PWM_PCR_CH2MOD_Pos;

    // Set freq to 70kHz and 50% duty cycle (it's just a nice clock)
    //
    // freq = HCLK/[(prescale+1)*(clock divider)*(CNR+1)]
    // 72MHz/(255 + 1)*(1)*(3+1)
    //
    // duty = (CMR+1)/(CNR+1)
    // (1+1)/(3+1)
    PWMA->CNR2 = 0xFF;
    PWMA->CMR2 = 0x7F;

    // Start PWM channel 2
    PWMA->PCR |= 1 << PWM_PCR_CH2EN_Pos;
}

static void select_row(int row) {
    // Disable all rows
    PC4 = PAL_LOW;
    PC5 = PAL_LOW;
    PB3 = PAL_LOW;
    PB2 = PAL_LOW;
    PD8 = PAL_LOW;

    switch(row) {
        case -1:
        return;

        case 0:
        PC4 = PAL_HIGH;
        return;

        case 1:
        PC5 = PAL_HIGH;
        return;

        case 2:
        PB3 = PAL_HIGH;
        return;

        case 3:
        PB2 = PAL_HIGH;
        return;

        case 4:
        PD8 = PAL_HIGH;
        return;
    }
}

OSAL_IRQ_HANDLER(NUC123_PWMA_HANDLER) {
    OSAL_IRQ_PROLOGUE();

    // channel 2 duty interrupt
    if ((PWMA->PIIR >> PWM_PIIR_PWMDIF2_Pos) & 1 & ~DCLK) {
        uint8_t led_idx = dclk_pulse_count / 16;
        uint8_t clk_cycle = dclk_pulse_count % 16;
        uint8_t msb_idx = 15 - clk_cycle;

        SDI_RED = (sdi_red_buf[current_row][led_idx] >> msb_idx) & 1;
        SDI_GREEN = (sdi_green_buf[current_row][led_idx] >> msb_idx) & 1;
        SDI_BLUE = (sdi_blue_buf[current_row][led_idx] >> msb_idx) & 1;

        LE = (led_idx == 16 && clk_cycle > 12) || clk_cycle > 14;
    }

    // channel 2 underflow interrupt
    if ((PWMA->PIIR >> PWM_PIIR_PWMIF2_Pos) & 1) {
        // Toggle dclk
        DCLK ^= 1;

        // count each clock pulse [0,271]
        dclk_pulse_count += 1 & DCLK;
        dclk_pulse_count %= 272;
    }

    // Reset interrupt flags
    PWMA->PIIR |= (1 << PWM_PIIR_PWMIF2_Pos) | (1 << PWM_PIIR_PWMDIF2_Pos);

    OSAL_IRQ_EPILOGUE();
}

static void init(void) {
    // Disable LED controllers
    PD5 = PAL_HIGH;

    // Disable all rows
    select_row(0);

    // Reset all data lines
    LE = PAL_LOW;
    DCLK = PAL_LOW;
    SDI_RED = PAL_LOW;
    SDI_GREEN = PAL_LOW;
    SDI_BLUE = PAL_LOW;

    setup_pwm();
    setup_gclk();
    setup_dclk();

    // Enable the LED controllers
    PD5 = PAL_LOW;

    // write_configuration(0b1000010000000000u);
    sdi_red_buf[0][0] = 0xFFFF;
}

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

const rgb_matrix_driver_t rgb_matrix_driver = {
    .init = init,
    .flush = flush,
    .set_color = set_color,
    .set_color_all = set_color_all
};

// #endif // RGB_MATRIX_ENABLE
