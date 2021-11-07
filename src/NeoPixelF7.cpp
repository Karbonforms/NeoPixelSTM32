#if defined(STM32L4xx)
#include <stm32l4xx_hal.h>
#elif defined(STM32F7xx)
#include <stm32f7xx_hal.h>
#elif defined(STM32F3xx)
#include <stm32f3xx_hal.h>
#elif defined(STM32)
#include <stm32f3xx_hal.h>
#endif

#include <chrono>

#include "NeoPixelF7_config.h"
#include "NeoPixelF7.h"

using namespace std::chrono;

TIM_HandleTypeDef g_TimHandle;
DMA_HandleTypeDef g_HdmaTim1Ch1;

volatile bool g_DataSentFlag = true;

const uint32_t      WS_2812_CLK_FREQ            = 800000;
const nanoseconds   WS_2812_ZERO_HIGH_TIME_NS   = 400ns;
const nanoseconds   WS_2812_RESET_PERIOD_NS     = 50us;

using TimerTicks = duration<uint32_t , std::ratio<1, TIMER_CLK_FREQ>>;

const TimerTicks ONE_SECOND_TICKS = 1s;

uint32_t g_AutoReloadRegister = TIMER_CLK_FREQ / WS_2812_CLK_FREQ;
uint32_t g_ShortPulse;
uint32_t g_ResetCycleCount;

bool g_init = false;

//uint16_t*                    g_PwmData; //       [(24 * NUM_KEYS) + 50];
uint16_t                    g_PwmData       [(24 * NUM_PIXELS) + 50];

void error_handler()
{
    __disable_irq();
    while (true)
    {}
}

void NeoPixelF7_show(const uint32_t* ptr, uint32_t num_pixels)
{
    while (!g_DataSentFlag);
    g_DataSentFlag = false;

    uint32_t length = 0;

    for (uint32_t j = 0; j < num_pixels; ++j)
    {
        for (int i = 23; i >= 0; i--)
        {
            g_PwmData[length++] = ptr[j] & (1 << i) ? g_ShortPulse << 1 : g_ShortPulse;
        }
    }

    for (uint32_t i = 0; i < g_ResetCycleCount + 1; i++)
    {
        g_PwmData[length++] = 0;
    }

    HAL_TIM_PWM_Start_DMA(&g_TimHandle, TIM_CHANNEL_1, (uint32_t*) g_PwmData, length);
}

void calculate_timings()
{
    g_AutoReloadRegister = (ONE_SECOND_TICKS / WS_2812_CLK_FREQ).count();
    g_ShortPulse = duration_cast<TimerTicks>(WS_2812_ZERO_HIGH_TIME_NS).count();

    const auto reset_ticks = duration_cast<TimerTicks>(WS_2812_RESET_PERIOD_NS);
    g_ResetCycleCount = reset_ticks / (ONE_SECOND_TICKS / WS_2812_CLK_FREQ);
}

void NeoPixelF7_init()
{
    if (g_init) return;

    calculate_timings();

#if defined(STM32L4xx) | defined(STM32F3xx)
    __HAL_RCC_DMA1_CLK_ENABLE();
    HAL_NVIC_SetPriority(DMA1_Channel2_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
#elif defined(STM32F7xx)
    __HAL_RCC_DMA2_CLK_ENABLE();
    HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
#endif

    TIM_ClockConfigTypeDef          clock_source_config     = {0};
    TIM_OC_InitTypeDef              config_oc               = {0};

    g_TimHandle.Instance = TIM1;
    g_TimHandle.Init.Period = g_AutoReloadRegister - 1;
    if (HAL_TIM_Base_Init(&g_TimHandle) != HAL_OK)
    {
        error_handler();
    }
    clock_source_config.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    if (HAL_TIM_ConfigClockSource(&g_TimHandle, &clock_source_config) != HAL_OK)
    {
        error_handler();
    }
    if (HAL_TIM_PWM_Init(&g_TimHandle) != HAL_OK)
    {
        error_handler();
    }
    config_oc.OCMode = TIM_OCMODE_PWM1;
    if (HAL_TIM_PWM_ConfigChannel(&g_TimHandle, &config_oc, TIM_CHANNEL_1) != HAL_OK)
    {
        error_handler();
    }

    GPIO_InitTypeDef gpio_init_struct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();
    gpio_init_struct.Pin = GPIO_PIN_8;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_NOPULL;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW;
#if defined (STM32F3xx)
    gpio_init_struct.Alternate = GPIO_AF6_TIM1;
#else
    gpio_init_struct.Alternate = GPIO_AF1_TIM1;
#endif
    HAL_GPIO_Init(GPIOA, &gpio_init_struct);

    g_init = true;
}

//void NeoPixelF7_reset()
//{
//    g_init = false;
//
//}

// called by HAL_TIM_Base_Init
extern "C" void HAL_TIM_Base_MspInit(TIM_HandleTypeDef* tim_baseHandle)
{
    if (tim_baseHandle->Instance == TIM1)
    {
        __HAL_RCC_TIM1_CLK_ENABLE();
#if defined(STM32L4xx)
        g_HdmaTim1Ch1.Instance = DMA1_Channel2;
        g_HdmaTim1Ch1.Init.Request = DMA_REQUEST_7;
#elif defined(STM32F7xx)
        g_HdmaTim1Ch1.Instance = DMA2_Stream1;
        g_HdmaTim1Ch1.Init.Channel = DMA_CHANNEL_6;
#elif defined(STM32F3xx)
        g_HdmaTim1Ch1.Instance = DMA1_Channel2;
#endif
        g_HdmaTim1Ch1.Init.Direction = DMA_MEMORY_TO_PERIPH;
        g_HdmaTim1Ch1.Init.MemInc = DMA_MINC_ENABLE;
        g_HdmaTim1Ch1.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
        g_HdmaTim1Ch1.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
#if defined(STM32F7xx)
        g_HdmaTim1Ch1.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
#endif
        if (HAL_DMA_Init(&g_HdmaTim1Ch1) != HAL_OK)
        {
            error_handler();
        }

        __HAL_LINKDMA(tim_baseHandle, hdma[TIM_DMA_ID_CC1], g_HdmaTim1Ch1);
    }
}

// called by HAL_TIM_Base_DeInit
extern "C" void HAL_TIM_Base_MspDeInit(TIM_HandleTypeDef* tim_baseHandle)
{
    if (tim_baseHandle->Instance == TIM1)
    {
        __HAL_RCC_TIM1_CLK_DISABLE();
        HAL_DMA_DeInit(tim_baseHandle->hdma[TIM_DMA_ID_CC1]);
    }
}

extern "C" void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef*)
{
    HAL_TIM_PWM_Stop_DMA(&g_TimHandle, TIM_CHANNEL_1);
    g_DataSentFlag = true;
}

#if defined(STM32F7xx)
extern "C" void DMA2_Stream1_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&g_HdmaTim1Ch1);
}
#elif defined(STM32L4xx) | defined(STM32F3xx)
extern "C" void DMA1_Channel2_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&g_HdmaTim1Ch1);
}
#endif

uint32_t Pixels::create_color(uint8_t red, uint8_t green, uint8_t blue)
{
    return (green << 16 | red << 8 | blue);
}

void Pixels::set_color(uint32_t index, uint32_t color)
{
    if (index >= len_) return;
    pixels_[index] = color;
}

void Pixels::set_rgb(uint32_t index, uint8_t red, uint8_t green, uint8_t blue)
{
    if (index >= len_) return;
    pixels_[index] = (green << 16 | red << 8 | blue);
}

void Pixels::clear_rgb(uint32_t index)
{
    if (index >= len_) return;
    pixels_[index] = 0;
}

void Pixels::show()
{
    NeoPixelF7_show(pixels_, len_);
}

void Pixels::begin()
{
    NeoPixelF7_init();
}

Pixels::Pixels(uint32_t* arr, uint32_t len)
:pixels_(arr)
,len_(len)
{}

