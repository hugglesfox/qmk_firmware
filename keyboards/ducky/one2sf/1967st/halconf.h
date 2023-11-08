#pragma once

#define HAL_USE_PWM FALSE  // LED controller uses custom PWM code
#define HAL_USE_PAL TRUE

#include_next <halconf.h>
