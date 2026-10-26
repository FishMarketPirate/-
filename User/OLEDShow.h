#ifndef __OLEDSHOW_H
#define __OLEDSHOW_H

#include "stm32f10x.h"
#include "Time.h"

/* 显示驱动：把时间、温湿度、闹钟以及各个设置界面封装起来，
 * 主函数只需要调用这里的函数即可。 */

void OLEDShow_Init(void);        /* 初始化 OLED 并清屏 */

/* 主界面 */
void OLEDShow_ShowTime(RTC_TimeTypeDef *t);                  /* 日期 + 星期 + 时间 */
void OLEDShow_ShowTempHum(uint8_t temp, uint8_t hum, uint8_t valid); /* 温湿度，valid=0 显示 -- */
void OLEDShow_ShowAlarm(ALARM_TypeDef *a);                   /* 闹钟时间及开关状态 */

/* 修改界面（field 指定的字段用 [ ] 标出） */
void OLEDShow_EditTime(RTC_TimeTypeDef *t, uint8_t field);
void OLEDShow_EditAlarm(ALARM_TypeDef *a, uint8_t field);

/* 闹钟响铃时第 4 行的闪烁提示 */
void OLEDShow_ShowRing(uint8_t visible);

/* 在第 4 行显示一行提示信息 */
void OLEDShow_ShowMessage(char *msg);

#endif
