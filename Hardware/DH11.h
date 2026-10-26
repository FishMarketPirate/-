#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f10x.h"

#define dht11_high GPIO_SetBits(GPIOA, GPIO_Pin_10)
#define dht11_low  GPIO_ResetBits(GPIOA, GPIO_Pin_10)
#define Read_Data  GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_10)

/* 温湿度数据：rec_data[0]=湿度整数 rec_data[1]=湿度小数
 *             rec_data[2]=温度整数 rec_data[3]=温度小数 */
extern uint8_t rec_data[4];

void DH11_Out_Init(void);
void DH11_In_Init(void);
void DH11_Start(void);
char DH11_Rec_Byte(void);

/* 读取一次温湿度
 * @retval 1=读取成功 0=没有响应/校验错误（此时 rec_data 不被修改） */
uint8_t DH11_Rec_Data(void);

#endif
