#include "stm32f10x.h"
#include "Delay.h"
#include "Key.h"

/* 按键：KEY1 接 PA1，KEY2 接 PA3，按下为低电平 */
#define KEY1_PIN    GPIO_Pin_1
#define KEY2_PIN    GPIO_Pin_3
#define KEY_PORT    GPIOA

static uint8_t Key1Last = 1;
static uint8_t Key2Last = 1;

/**
  * @brief  按键初始化
  */
void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = KEY1_PIN | KEY2_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(KEY_PORT, &GPIO_InitStructure);
}

/**
  * @brief  读取一个按键的按下事件（内部消抖）
  * @param  pin  按键引脚
  * @param  last 保存上一次的电平状态
  * @retval 1=本次按下，0=没有按下
  */
static uint8_t Key_Read(uint16_t pin, uint8_t *last)
{
    uint8_t raw;

    raw = GPIO_ReadInputDataBit(KEY_PORT, pin);
    if (raw == 0 && *last == 1)         /* 由松开变为按下 */
    {
        Delay_ms(20);                    /* 消抖 */
        if (GPIO_ReadInputDataBit(KEY_PORT, pin) == 0)
        {
            *last = 0;
            return 1;
        }
    }

    *last = GPIO_ReadInputDataBit(KEY_PORT, pin);
    return 0;
}

/**
  * @brief  读取按键值
  * @retval 0=没有按键按下，1=KEY1 按下，2=KEY2 按下
  * @note   非阻塞，按住不放只返回一次
  */
uint8_t Key_GetNum(void)
{
    if (Key_Read(KEY1_PIN, &Key1Last))
    {
        return 1;
    }
    if (Key_Read(KEY2_PIN, &Key2Last))
    {
        return 2;
    }
    return 0;
}
