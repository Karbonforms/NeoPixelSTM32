#ifndef MIDILIGHTSF7_NEOPIXELF7_CONFIG_H
#define MIDILIGHTSF7_NEOPIXELF7_CONFIG_H

#if defined(STM32F7xx)
    #define TIMER_CLK_FREQ (216000000)
#elif defined(STM32L4xx)
    #define TIMER_CLK_FREQ (80000000)
#endif

#define NUM_PIXELS     (144)

#endif //MIDILIGHTSF7_NEOPIXELF7_CONFIG_H
