/* Copyright 2019 /u/KeepItUnder
 * Copyright 2023 Hayley Hughes
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * MBIA045 datasheet available at
 * https://pc-clinic.bg/wp-content/uploads/2021/05/mbia045-datasheet-va.00-en.pdf
 */

#ifdef RGB_MATRIX_ENABLE

#    include <string.h>

#    include "config.h"
#    include "hal.h"
#    include "progmem.h"
#    include "rgb_matrix.h"

#    define MBIA045_CFGREG_DEFAULT 0b1000010000000000u

#    define MBIA045_DCLK PD4
#    define MBIA045_LE PD3
#    define MBIA045_EN PD5

#    define MBIA045_ROW_COUNT 5

typedef struct mbi_led {
    uint8_t row;
    uint8_t col;
} __attribute__((packed)) mbi_led;

typedef struct mbi_color {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} __attribute__((packed)) mbi_color;

// Map physical layout to QMK led index
const mbi_led g_mbi_leds[RGB_MATRIX_LED_COUNT] = {{0, 0}, {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5}, {0, 6}, {0, 7}, {0, 8}, {0, 9}, {0, 10}, {0, 11}, {0, 12}, {0, 13}, {0, 14}, {1, 0}, {1, 1}, {1, 2}, {1, 3}, {1, 4}, {1, 5}, {1, 6}, {1, 7}, {1, 8}, {1, 9}, {1, 10}, {1, 11}, {1, 12}, {1, 13}, {1, 14}, {2, 0}, {2, 1}, {2, 2}, {2, 3}, {2, 4}, {2, 5}, {2, 6}, {2, 7}, {2, 8}, {2, 9}, {2, 10}, {2, 11}, {2, 13}, {2, 14}, {3, 0}, {3, 2}, {3, 3}, {3, 4}, {3, 5}, {3, 6}, {3, 7}, {3, 8}, {3, 9}, {3, 10}, {3, 11}, {3, 12}, {3, 13}, {4, 0}, {4, 1}, {4, 2}, {4, 3}, {4, 5}, {4, 7}, {4, 9}, {4, 10}, {4, 11}, {4, 12}, {4, 13}, {4, 14}};

// Lightness curve using the CIE 1931 lightness formula
// Generated by the python script provided in http://jared.geek.nz/2013/feb/linear-led-pwm
const uint16_t CIE1931_16_CURVE[] PROGMEM = {
    0,    7,    14,   21,   28,   36,   43,   50,   57,   64,   71,   78,   85,   93,   100,  107,  114,  121,  128,  135,  142,  149,  157,  164,  172,  180,  189,  197,  206,  215,  225,  234,  244,  254,  265,  276,  287,  298,  310,  322,  334,  346,  359,  373,  386,  400,  414,  428,  443,  458,  474,  490,  506,  522,  539,  557,  574,  592,  610,  629,  648,  668,  688,  708,  729,  750,  771,  793,  815,  838,  861,  885,  909,  933,  958,  983,  1009, 1035, 1061, 1088, 1116, 1144, 1172, 1201,  1230,  1260,  1290,  1321,  1353,  1384,  1417,  1449,  1482,  1516,  1550,  1585,  1621,  1656,  1693,  1729,  1767,  1805,  1843,  1882,  1922,  1962,  2003,  2044,  2085,  2128,  2171,  2214,  2258,  2303,  2348,  2394,  2440,  2487,  2535,  2583,  2632,  2681,  2731,  2782,  2833,  2885,  2938,  2991,
    3045, 3099, 3154, 3210, 3266, 3323, 3381, 3439, 3498, 3558, 3618, 3679, 3741, 3803, 3867, 3930, 3995, 4060, 4126, 4193, 4260, 4328, 4397, 4466, 4536, 4607, 4679, 4752, 4825, 4899, 4973, 5049, 5125, 5202, 5280, 5358, 5437, 5517, 5598, 5680, 5762, 5845, 5929, 6014, 6100, 6186, 6273, 6361, 6450, 6540, 6630, 6722, 6814, 6907, 7001, 7095, 7191, 7287, 7385, 7483, 7582, 7682, 7782, 7884, 7986, 8090, 8194, 8299, 8405, 8512, 8620, 8729, 8838, 8949, 9060, 9173, 9286, 9400, 9516, 9632, 9749, 9867, 9986, 10106, 10227, 10348, 10471, 10595, 10720, 10845, 10972, 11100, 11228, 11358, 11489, 11620, 11753, 11887, 12021, 12157, 12294, 12432, 12570, 12710, 12851, 12993, 13136, 13279, 13424, 13570, 13718, 13866, 14015, 14165, 14317, 14469, 14622, 14777, 14933, 15089, 15247, 15406, 15566, 15727, 15890, 16053, 16217, 16383,
};

/** The PWM buffers the full rows of 16 PWM registers in each MBI5042 driver
 *  The buffers are arranged in serial format
 *  (MSB_R, MSB_G, MSB_B...LSB_R, LSB_G, LSB_B)
 *  as needed by the SDO pins on the MCU (R is B14; G is B13; B is B12)
 *
 *  g_pwm_buffer has the DCLK-able output for an "R" row, a "G" row, and a "B" row of PWM
 */
uint16_t  g_pwm_buffer[MBIA045_ROW_COUNT][16 * 16];
mbi_color g_led_buffer[RGB_MATRIX_LED_COUNT];

bool    g_pwm_buffer_update_required = false;
uint8_t g_pwm_buffer_row             = 0;

void MBIA045_init(void);
void MBIA045_set_current_gain(uint8_t gain);
void MBIA045_write_config_register(uint16_t regValue);
void MBIA045_set_color(int index, uint8_t red, uint8_t green, uint8_t blue);
void MBIA045_set_color_all(uint8_t red, uint8_t green, uint8_t blue);
void MBIA045_update_pwm_buffers(void);
void MBIA045_flush(void);
void MBIA045_disable_rows(void);
void MBIA045_select_row(int row);

void MBIA045_init(void) {
    /* Initialise all PWM arrays to zero.
     * Perform one group transfer to turn LEDs off
     *
     * If there's a DMA requirement, set up DMA subsystems
     */
    for (int i = 0; i < MBIA045_ROW_COUNT; i++) {
        for (int j = 0; j < 256; j++) {
            g_pwm_buffer[i][j] = 0;
        }
    }

    /* Setup PWM0 control registers for 2MHz GCLK
     * and also 50 percentage PWM duty (makes a nice clock)
     *
     * Set PWM1 to be used as a 2kHz timer (interrupt but no output pin)
     * which is used for row refresh/enable.
     */

    // Use the HCLK
    // Note we need to 0 the bits first as the clksel register has a reset value
    // of 0xFFFF_FFFF
    CLK->CLKSEL1 &= ~CLK_CLKSEL1_PWM01_S_Msk;
    CLK->CLKSEL1 |= 2 << CLK_CLKSEL1_PWM01_S_Pos;

    CLK->CLKSEL2 &= ~CLK_CLKSEL2_PWM01_S_E_Msk;
    CLK->CLKSEL2 |= 2 << CLK_CLKSEL2_PWM01_S_E_Pos;

    // Enable the PWM peripheral clock
    CLK->APBCLK |= 1 << CLK_APBCLK_PWM01_EN_Pos;

    // Set prescaler for PWM0 and PWM1 to 1
    PWMA->PPR |= 1 << PWM_PPR_CP01_Pos;

    // Enable PWM interrupt vector
    // Interrupt priority value taken from chibios defaults
    nvicEnableVector(NUC123_PWMA_NUMBER, 3);

    // Set clock division to 1
    PWMA->CSR |= 1 << PWM_CSR_CSR2_Pos;

    // Set pin PA12 to PWM0
    SYS->GPA_MFP |= 1 << 12;

    // Enable PWM0 output
    PWMA->POE |= 1 << PWM_POE_PWM0_Pos;

    // Enable PWM0 and PWM1 auto reload
    PWMA->PCR |= (1 << PWM_PCR_CH0MOD_Pos | 1 << PWM_PCR_CH1MOD_Pos);

    // Enable PWM1 reset interrupt
    PWMA->PIER |= 1 << PWM_PIER_PWMIE1_Pos;

    // Set PWM0 freq to 9MHz and 50% duty cycle (it's just a nice clock)
    //
    // freq = HCLK/[(prescale+1)*(clock divider)*(CNR+1)]
    // 72MHz/(1 + 1)*(1)*(3+1)
    //
    // duty = (CMR+1)/(CNR+1)
    // (1+1)/(3+1)
    PWMA->CNR0 = 3;
    PWMA->CMR0 = 1;

    // Set PWM1 freq to 1.8kHz (duty doesn't matter)
    //
    // freq = HCLK/[(prescale+1)*(clock divider)*(CNR+1)]
    // 72MHz/(1 + 1)*(1)*(3+1)
    PWMA->CNR1 = 19999;
    PWMA->CMR1 = 1;

    // Start PWM channel 0 and 1
    PWMA->PCR |= (1 << PWM_PCR_CH0EN_Pos | 1 << PWM_PCR_CH1EN_Pos);

    MBIA045_disable_rows();

    // Enable the LED controllers
    MBIA045_EN = PAL_LOW;

    MBIA045_set_current_gain(0b000011u);
}

/**
 * @brief Set LED driver current gain
 * @detail The MBI5042 has a 6-bit current gain value (000000b ~ 111111b)
 * used to adjust the global brightness of the LEDs attached to the PWM outputs
 *
 * Default value is 101011b
 *
 * Use MBI5042_CURRENT_GAIN to pass from keyboard config
 */
void MBIA045_set_current_gain(uint8_t gain) {
    /** MB data transfer requires:
     *      Tell chip config register change is coming (Enable Write Configutarion)
     *      Pass config register data (Write Configuration Register)
     */
    uint16_t regConfig = 0b00111111u & gain;

    regConfig <<= 4;
    regConfig |= MBIA045_CFGREG_DEFAULT;

    MBIA045_write_config_register(regConfig);
}

/**
 * @brief   Write configuration register ONLY DURING INIT
 * @detail  Set the contents of the configuration register (see top for bitfields)
 *          Each register write needs 2 * 16 bit transfers (1 x setup and 1 x data)
 *
 * For the Ducky One 2 mini there are 3 drivers, so output all three configs at once
 */
void MBIA045_write_config_register(uint16_t regValue) {
    uint32_t b_mask;
    uint16_t tmp_r, tmp_g, tmp_b;

    /* Set Mask for GPIOB */
    b_mask    = PB->DMASK;
    PB->DMASK = ~(0x1u << 14 | 0x1u << 13 | 0x1u << 12);

    /* LE Low & DCLK Low */
    MBIA045_LE   = PAL_LOW;
    MBIA045_DCLK = PAL_LOW;

    /* Do one DCLK cycle */
    MBIA045_DCLK = PAL_HIGH;
    MBIA045_DCLK = PAL_LOW;

    /* Set LE High */
    MBIA045_LE = PAL_HIGH;

    /* Loop 15 - Enable Write Configuration */
    for (int i = 0; i < 15; i++) {
        /* Cycle DCLK */
        MBIA045_DCLK = PAL_HIGH;
        MBIA045_DCLK = PAL_LOW;
    }

    /* Reset LE Low */
    MBIA045_LE = PAL_LOW;

    /* Loop 16 - Transfer actual command data to all 3 LED drivers */
    for (int i = 0; i < 16; i++) {
        tmp_r = tmp_g = tmp_b = regValue;
        /* Set data bit */
        uint16_t bits = ((0x1u & (tmp_r >> 15)) << 14) | ((0x1u & (tmp_g >> 15)) << 13) | ((0x1u & (tmp_b >> 15)) << 12);
        PB->DOUT      = bits;

        /* Cycle DCLK */
        MBIA045_DCLK = PAL_HIGH;
        MBIA045_DCLK = PAL_LOW;

        if (i == 5) {
            MBIA045_LE = PAL_HIGH;
        }

        /* Next bit to sample */
        regValue <<= 1;
    }

    /* Put GPIOB DMASK back */
    PB->DMASK = b_mask;

    /* Reset LE Low */
    MBIA045_LE = PAL_LOW;
}

void MBIA045_set_color(int index, uint8_t red, uint8_t green, uint8_t blue) {
    /*
     * @brief Pick a colour! Any colour!
     */
    g_led_buffer[index] = (mbi_color){red, green, blue};
}

void MBIA045_set_color_all(uint8_t red, uint8_t green, uint8_t blue) {
    /*
     * brief Set every led to the provided colour
     */
    for (size_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        g_led_buffer[i] = (mbi_color){red, green, blue};
    }
}

void MBIA045_update_pwm_buffers(void) {
    /**
     * Pass current PWM row to MBIA045 shift registers
     *
     * LE low
     * Outer Loop 16 (one per register transfer - high to low):
     *      Inner Loop 16 (one per PWM bit):
     *          R_SDIN, G_SDIN & B_SDIN write
     *          DCLK High
     *          DCLK Low
     *          For final loop, set LE High
     *
     * Send Global Latch (16 DCLKs with LE high for last 3)
     *
     * Disable current row
     *
     * Reset PWM count:
     * Loop 3:
     *      DCLK High
     *      DCLK Low
     * LE High
     * Loop 13:
     *      DCLK High
     *      DCLK Low
     * LE Low
     *
     * Select new row (row meant for above data)
     */

    if (!g_pwm_buffer_update_required) return;

    uint32_t b_mask;

    /* Set Mask for GPIOB */
    b_mask    = PB->DMASK;
    PB->DMASK = ~(0x1u << 14 | 0x1u << 13 | 0x1u << 12);

    // LE Low & DCLK Low
    MBIA045_LE   = PAL_LOW;
    MBIA045_DCLK = PAL_LOW;

    for (int i = 0; i < 16; i++) {
        /* Inner Loop 16 */
        for (int j = 0; j < 16; j++) {
            /* R_SDIN/G_SDIN/B_SDIN write */
            PB->DOUT = g_pwm_buffer[g_pwm_buffer_row][16 * i + j];

            // If j is 15 set LE High
            if (j == 15) {
                MBIA045_LE = PAL_HIGH;
            }

            /* Cycle DCLK */
            MBIA045_DCLK = PAL_HIGH;
            MBIA045_DCLK = PAL_LOW;
        } // Inner Loop 16

        // LE Low
        MBIA045_LE = PAL_LOW;
    }

    /* Send Global Latch */
    for (int i = 0; i < 16; i++) {
        /* Cycle DCLK */
        MBIA045_DCLK = PAL_HIGH;
        MBIA045_DCLK = PAL_LOW;

        //  if i is 13 set LE high
        if (i == 13) {
            MBIA045_LE = PAL_HIGH;
        }
    }

    // Reset LE Low
    MBIA045_LE = PAL_LOW;

    // Reset Masks
    PB->DMASK = b_mask;

    // Reset PWM count
    // 3 DCLK cycles
    for (int i = 0; i < 3; i++) {
        MBIA045_DCLK = PAL_HIGH;
        MBIA045_DCLK = PAL_LOW;
    }

    // Set LE High
    MBIA045_LE = PAL_HIGH;

    // Loop 13 to generate PWM count reset
    for (int i = 0; i < 13; i++) {
        MBIA045_DCLK = PAL_HIGH;
        MBIA045_DCLK = PAL_LOW;
    }

    // Set LE Low
    MBIA045_LE = PAL_LOW;

    // Set new row
    MBIA045_select_row(g_pwm_buffer_row);

    // increment row count + check
    g_pwm_buffer_row++;
    if (g_pwm_buffer_row >= MBIA045_ROW_COUNT) {
        g_pwm_buffer_row = 0;
    }
}

/**
 * @brief Send the color buffer to the PWM buffer
 */
void MBIA045_flush(void) {
    mbi_led   led_pos;
    mbi_color color;

    for (size_t i = 0; i < RGB_MATRIX_LED_COUNT; i++) {
        led_pos = g_mbi_leds[i];
        color   = g_led_buffer[i];

        // Convert color values from 8 to 16 bit
        uint16_t cur_r = pgm_read_word(&CIE1931_16_CURVE[color.r]);
        uint16_t cur_g = pgm_read_word(&CIE1931_16_CURVE[color.g]);
        uint16_t cur_b = pgm_read_word(&CIE1931_16_CURVE[color.b]);

        // Construct the pwm buffer
        for (int i = 0; i < 16; i++) {
            uint16_t tmp_r = cur_r;
            uint16_t tmp_g = cur_g;
            uint16_t tmp_b = cur_b;

            g_pwm_buffer[led_pos.row][i + (led_pos.col * 16)] = ((0x1u & (tmp_r >> 15)) << 14) | ((0x1u & (tmp_g >> 15)) << 13) | ((0x1u & (tmp_b >> 15)) << 12);
            cur_r <<= 1;
            cur_g <<= 1;
            cur_b <<= 1;
        }
    }

    g_pwm_buffer_update_required = true;
}

/**
 * @brief Disable all LED Rows
 */
void MBIA045_disable_rows(void) {
    // Row 0
    PC4 = PAL_LOW;

    // Row 1
    PC5 = PAL_LOW;

    // Row 2
    PB3 = PAL_LOW;

    // Row 3
    PB2 = PAL_LOW;

    // Row 4
    PD9 = PAL_LOW;
}

/**
 * @brief Enable the specific LED row, disabling all others
 */
void MBIA045_select_row(int row) {
    MBIA045_disable_rows();
    switch (row) {
        case 0: // Row 0
            PC4 = PAL_HIGH;
            break;
        case 1: // Row 1
            PC5 = PAL_HIGH;
            break;
        case 2: // Row 2
            PB3 = PAL_HIGH;
            break;
        case 3: // Row 3
            PB2 = PAL_HIGH;
            break;
        case 4: // Row 4
            PD9 = PAL_HIGH;
            break;
    }
}

OSAL_IRQ_HANDLER(NUC123_PWMA_HANDLER) {
    OSAL_IRQ_PROLOGUE();

    /* Check for PWM1 underflow IRQ */
    if ((PWMA->PIIR >> PWM_PIIR_PWMIF1_Pos) & 1) {
        /* Clear interrupt flag */
        PWMA->PIIR |= 1 << PWM_PIIR_PWMIF1_Pos;
        MBIA045_update_pwm_buffers();
    }

    OSAL_IRQ_EPILOGUE();
}

const rgb_matrix_driver_t rgb_matrix_driver = {
    .init          = MBIA045_init,
    .flush         = MBIA045_flush,
    .set_color     = MBIA045_set_color,
    .set_color_all = MBIA045_set_color_all,
};

#endif
