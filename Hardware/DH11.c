#include "stm32f10x.h"
#include "DH11.h"
#include "Delay.h"

uint8_t rec_data[4];

void DH11_Out_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

void DH11_In_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;   /* 上拉输入，传感器未接时总线保持高电平 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
}

void DH11_Start(void)
{
    DH11_Out_Init();

    dht11_high;
    Delay_us(30);

    dht11_low;              /* 主机拉低至少 18ms 作为起始信号 */
    Delay_ms(20);

    dht11_high;             /* 释放总线，等待 DHT11 响应 */
    Delay_us(30);

    DH11_In_Init();
}

/**
  * @brief  等待总线电平离开 level
  * @retval 1=等到，0=超时（约 1ms），用于防止传感器没接时程序卡死
  */
static uint8_t DH11_WaitLevel(uint8_t level)
{
    uint16_t timeout = 6000;

    while (Read_Data == level)
    {
        if (timeout-- == 0)
        {
            return 0;
        }
    }
    return 1;
}

/**
  * @brief  接收一个字节（DHT11 高位在前）
  */
char DH11_Rec_Byte(void)
{
    uint8_t i;
    uint8_t data = 0;

    for (i = 0; i < 8; i++)
    {
        if (!DH11_WaitLevel(0))         /* 等待 50us 低电平结束 */
        {
            return (char)data;
        }

        Delay_us(40);                    /* 高电平时长决定数据是 0 还是 1 */
        data <<= 1;
        if (Read_Data == 1)
        {
            data |= 1;
        }

        if (!DH11_WaitLevel(1))          /* 等待高电平结束 */
        {
            return (char)data;
        }
    }
    return (char)data;
}

/**
  * @brief  读取一次温湿度数据
  * @retval 1=成功，0=失败
  */
uint8_t DH11_Rec_Data(void)
{
    uint8_t RH, RL, TH, TL, CHECK;

    DH11_Start();

    if (!DH11_WaitLevel(1))         /* 等待 DHT11 拉低响应 */
    {
        return 0;
    }
    if (!DH11_WaitLevel(0))         /* 响应低电平结束 */
    {
        return 0;
    }
    if (!DH11_WaitLevel(1))         /* 响应高电平结束 */
    {
        return 0;
    }

    RH    = (uint8_t)DH11_Rec_Byte();
    RL    = (uint8_t)DH11_Rec_Byte();
    TH    = (uint8_t)DH11_Rec_Byte();
    TL    = (uint8_t)DH11_Rec_Byte();
    CHECK = (uint8_t)DH11_Rec_Byte();

    /* 读取完成后引脚已切回输入模式，总线由外部上拉拉高，无需再驱动 */

    if ((uint8_t)(RH + RL + TH + TL) != CHECK)
    {
        return 0;
    }

    rec_data[0] = RH;
    rec_data[1] = RL;
    rec_data[2] = TH;
    rec_data[3] = TL;
    return 1;
}
