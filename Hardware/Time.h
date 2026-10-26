#ifndef __TIME_H
#define __TIME_H

#include "stm32f10x.h"

/* RTC 时间结构体（年份为完整年份，如 2026） */
typedef struct
{
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  week;     /* 0=周日 1=周一 ... 6=周六 */
    uint8_t  hour;
    uint8_t  min;
    uint8_t  sec;
} RTC_TimeTypeDef;

/* 闹钟结构体 */
typedef struct
{
    uint8_t  hour;
    uint8_t  min;
    uint8_t  enable;   /* 0=关闭 1=开启 */
} ALARM_TypeDef;

/* 修改时间时用到的字段编号 */
#define TIME_FIELD_YEAR       0
#define TIME_FIELD_MONTH      1
#define TIME_FIELD_DAY        2
#define TIME_FIELD_HOUR       3
#define TIME_FIELD_MIN        4
#define TIME_FIELD_SEC        5
#define TIME_FIELD_NUM        6

/* 修改闹钟时用到的字段编号 */
#define ALARM_FIELD_HOUR      0
#define ALARM_FIELD_MIN       1
#define ALARM_FIELD_ENABLE    2
#define ALARM_FIELD_NUM       3

extern RTC_TimeTypeDef now_time;    /* 当前时间 */
extern ALARM_TypeDef alarm_cfg;     /* 闹钟设置 */

uint8_t IsLeapYear(uint16_t y);
uint8_t Time_DaysInMonth(uint16_t y, uint8_t m);
uint8_t Time_WeekDay(uint16_t y, uint8_t m, uint8_t d);

void Time_Init(void);                                              /* 初始化，读出时间和闹钟 */
void Time_Read(RTC_TimeTypeDef *t);                                /* 从 RTC 读取当前时间 */
void Time_Set(RTC_TimeTypeDef *t);                                 /* 把时间写入 RTC */
void Time_Adjust(RTC_TimeTypeDef *t, uint8_t field, int16_t step); /* 调整时间字段 */
void Alarm_Adjust(ALARM_TypeDef *a, uint8_t field, int16_t step);  /* 调整闹钟字段 */
void Alarm_Load(void);                                             /* 从后备寄存器读闹钟 */
void Alarm_Save(void);                                             /* 把闹钟保存到后备寄存器 */

#endif
