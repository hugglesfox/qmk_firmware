/*
    ChibiOS Driver element - Copyright (C) 2019 /u/KeepItUnder

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include "hal.h"

#if HAL_USE_PAL || defined(__DOXYGEN__)
/**
 * @brief   PAL setup.
 * @details Digital I/O ports static configuration as defined in @p board.h.
 *          This variable is used by the HAL when initializing the PAL driver.
 */
const PALConfig pal_default_config = {
#    if NUC123_HAS_GPIOA
    {VAL_GPIOA_PMD, VAL_GPIOA_OFFD, VAL_GPIOA_DMASK, VAL_GPIOA_DBEN, VAL_GPIOA_IMD, VAL_GPIOA_IEN, VAL_GPIOA_ISRC, VAL_GPIOA_DOUT},
#    endif
#    if NUC123_HAS_GPIOB
    {VAL_GPIOB_PMD, VAL_GPIOB_OFFD, VAL_GPIOB_DMASK, VAL_GPIOB_DBEN, VAL_GPIOB_IMD, VAL_GPIOB_IEN, VAL_GPIOB_ISRC, VAL_GPIOB_DOUT},
#    endif
#    if NUC123_HAS_GPIOC
    {VAL_GPIOC_PMD, VAL_GPIOC_OFFD, VAL_GPIOC_DMASK, VAL_GPIOC_DBEN, VAL_GPIOC_IMD, VAL_GPIOC_IEN, VAL_GPIOC_ISRC, VAL_GPIOC_DOUT},
#    endif
#    if NUC123_HAS_GPIOD
    {VAL_GPIOD_PMD, VAL_GPIOD_OFFD, VAL_GPIOD_DMASK, VAL_GPIOD_DBEN, VAL_GPIOD_IMD, VAL_GPIOD_IEN, VAL_GPIOD_ISRC, VAL_GPIOD_DOUT},
#    endif
#    if NUC123_HAS_GPIOF
    {VAL_GPIOF_PMD, VAL_GPIOF_OFFD, VAL_GPIOF_DMASK, VAL_GPIOF_DBEN, VAL_GPIOF_IMD, VAL_GPIOF_IEN, VAL_GPIOF_ISRC, VAL_GPIOF_DOUT},
#    endif
};
#endif

/**
 * @brief   Early initialization code.
 * @details This initialization must be performed just after stack setup
 *          and before any other initialization.
 */
void __early_init(void) {
    NUC123_clock_init();
}

#define SDI PB14
#define LE PD3
#define DCLK PD4

static PWMConfig pwmCFG = {
    2000000,  // 2 MHz
    32768,    // 50% duty cycle
    NULL,
    {
        {PWM_OUTPUT_ACTIVE_HIGH, NULL},  // Enable channel 0 (PA12)
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL},
        {PWM_OUTPUT_DISABLED, NULL}
    }
};

/**
 * @brief   Board-specific initialization code.
 * @todo    Add your board-specific code, if any.
 */
void boardInit(void) {
    // Disable all rows but row 0
    PC4 = PAL_HIGH;
    PC5 = PAL_LOW;
    PB3 = PAL_LOW;
    PB2 = PAL_LOW;
    PD8 = PAL_LOW;

    pwmStart(&PWMD1, &pwmCFG);

    // Set all the registers to 0xFFFF
    for (int j = 0; j < 48; j++) {
        for (int i = 0; i < 16; i++) {
            SDI = PAL_HIGH;

            // Data latch
            if (i == 0  && j != 47) {
                LE = PAL_HIGH;
            }

            // Global latch (last data segment)
            if (i == 2 && j == 47) {
                LE = PAL_HIGH;
            }

            // Trigger clock
            DCLK = PAL_HIGH;
            DCLK = PAL_LOW;
            LE = PAL_LOW;
        }
    }
}
