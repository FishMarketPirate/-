#include "stm32f10x.h"
#include "Time.h"
#include "MyRTC.h"

/* 当前时间、闹钟设置（闹钟默认 07:00，关闭） */
RTC_TimeTypeDef now_time;
ALARM_TypeDef alarm_cfg = {7, 0, 0};

/* 闹钟保存在后备寄存器（BKP_DR2~DR4），掉电/复位后不丢失 */
#define ALARM_BKP_MAGIC        0x5AA5
#define ALARM_BKP_REG_MAGIC    BKP_DR2
#define ALARM_BKP_REG_TIME     BKP_DR3   /* 高字节=时，低字节=分 */
#define ALARM_BKP_REG_ENABLE   BKP_DR4

/* 每个月的天数（平年），闰年 2 月单独处理 */
static const uint8_t MonthDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

/**
  * @brief  判断是否闰年
  */
uint8_t IsLeapYear(uint16_t y)
{
    return (uint8_t)((y % 4 == 0 && y % 100 != 0) || (y % 400 == 0));
}

/**
  * @brief  返回某年某月的天数
  */
uint8_t Time_DaysInMonth(uint16_t y, uint8_t m)
{
    if (m < 1 || m > 12)
    {
        return 31;
    }
    if (m == 2 && IsLeapYear(y))
    {
        return 29;
    }
    return MonthDays[m - 1];
}

/**
  * @brief  由年月日计算星期（0=周日）
  * @note   蔡勒公式：结果 0=周六，换算成 0=周日
  */
uint8_t Time_WeekDay(uint16_t y, uint8_t m, uint8_t d)
{
    uint16_t k, j, h;

    if (m < 3)
    {
        m += 12;
        y--;
    }
    k = y % 100;
    j = y / 100;
    h = (uint16_t)(d + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 + 5 * j);
    return (uint8_t)((h + 6) % 7);      /* 0=周日 1=周一 ... 6=周六 */
}

/**
  * @brief  初始化：读出当前时间，并恢复闹钟设置
  * @note   需要先调用 MyRTC_Init()
  */
void Time_Init(void)
{
    Time_Read(&now_time);
    Alarm_Load();
}

/**
  * @brief  从 RTC 读取时间到结构体
  */
void Time_Read(RTC_TimeTypeDef *t)
{
    MyRTC_ReadTime();

    t->year  = MyRTC_Time[0];
    t->month = (uint8_t)MyRTC_Time[1];
    t->day   = (uint8_t)MyRTC_Time[2];
    t->hour  = (uint8_t)MyRTC_Time[3];
    t->min   = (uint8_t)MyRTC_Time[4];
    t->sec   = (uint8_t)MyRTC_Time[5];
    t->week  = Time_WeekDay(t->year, t->month, t->day);
}

/**
  * @brief  把结构体中的时间写入 RTC
  */
void Time_Set(RTC_TimeTypeDef *t)
{
    MyRTC_Time[0] = t->year;
    MyRTC_Time[1] = t->month;
    MyRTC_Time[2] = t->day;
    MyRTC_Time[3] = t->hour;
    MyRTC_Time[4] = t->min;
    MyRTC_Time[5] = t->sec;

    MyRTC_SetTime();
    t->week = Time_WeekDay(t->year, t->month, t->day);
}

/**
  * @brief  在编辑界面调整时间字段（编码器正转 step>0，反转 step<0）
  * @param  field 字段编号 TIME_FIELD_xxx
  */
void Time_Adjust(RTC_TimeTypeDef *t, uint8_t field, int16_t step)
{
    int16_t v;
    uint8_t dim;

    if (step == 0)
    {
        return;
    }

    switch (field)
    {
        case TIME_FIELD_YEAR:
            /* 先把越界年份拉回合法区间，再调整 */
            if (t->year < 2000) t->year = 2000;
            if (t->year > 2037) t->year = 2037;
            v = (int16_t)(t->year + step);
            /* 限制在 2000~2037，避免 32 位 time_t 在 2038 年溢出 */
            while (v > 2037) v -= 38;
            while (v < 2000) v += 38;
            t->year = (uint16_t)v;
            break;

        case TIME_FIELD_MONTH:
            v = (int16_t)(t->month + step);
            while (v > 12) v -= 12;
            while (v < 1)  v += 12;
            t->month = (uint8_t)v;
            break;

        case TIME_FIELD_DAY:
            dim = Time_DaysInMonth(t->year, t->month);
            v = (int16_t)(t->day + step);
            while (v > (int16_t)dim) v -= (int16_t)dim;
            while (v < 1)            v += (int16_t)dim;
            t->day = (uint8_t)v;
            break;

        case TIME_FIELD_HOUR:
            v = (int16_t)(t->hour + step);
            while (v > 23) v -= 24;
            while (v < 0)  v += 24;
            t->hour = (uint8_t)v;
            break;

        case TIME_FIELD_MIN:
            v = (int16_t)(t->min + step);
            while (v > 59) v -= 60;
            while (v < 0)  v += 60;
            t->min = (uint8_t)v;
            break;

        case TIME_FIELD_SEC:
            v = (int16_t)(t->sec + step);
            while (v > 59) v -= 60;
            while (v < 0)  v += 60;
            t->sec = (uint8_t)v;
            break;

        default:
            break;
    }

    /* 调整年/月后，日期不能超过当月最大天数 */
    dim = Time_DaysInMonth(t->year, t->month);
    if (t->day > dim)
    {
        t->day = dim;
    }
    if (t->day < 1)
    {
        t->day = 1;
    }
    t->week = Time_WeekDay(t->year, t->month, t->day);
}

/**
  * @brief  在编辑界面调整闹钟字段
  */
void Alarm_Adjust(ALARM_TypeDef *a, uint8_t field, int16_t step)
{
    int16_t v;

    if (step == 0)
    {
        return;
    }

    switch (field)
    {
        case ALARM_FIELD_HOUR:
            v = (int16_t)(a->hour + step);
            while (v > 23) v -= 24;
            while (v < 0)  v += 24;
            a->hour = (uint8_t)v;
            break;

        case ALARM_FIELD_MIN:
            v = (int16_t)(a->min + step);
            while (v > 59) v -= 60;
            while (v < 0)  v += 60;
            a->min = (uint8_t)v;
            break;

        case ALARM_FIELD_ENABLE:
            /* 右转打开，左转关闭 */
            a->enable = (step > 0) ? 1 : 0;
            break;

        default:
            break;
    }
}

/**
  * @brief  从后备寄存器恢复闹钟设置
  */
void Alarm_Load(void)
{
    if (BKP_ReadBackupRegister(ALARM_BKP_REG_MAGIC) == ALARM_BKP_MAGIC)
    {
        uint16_t time = BKP_ReadBackupRegister(ALARM_BKP_REG_TIME);

        alarm_cfg.hour = (uint8_t)((time >> 8) & 0xFF);
        alarm_cfg.min  = (uint8_t)(time & 0xFF);
        if (alarm_cfg.hour > 23) alarm_cfg.hour = 7;
        if (alarm_cfg.min > 59)  alarm_cfg.min = 0;
        alarm_cfg.enable = (uint8_t)(BKP_ReadBackupRegister(ALARM_BKP_REG_ENABLE) & 0x01);
    }
    else
    {
        /* 第一次上电：使用默认设置并保存 */
        alarm_cfg.hour = 7;
        alarm_cfg.min = 0;
        alarm_cfg.enable = 0;
        Alarm_Save();
    }
}

/**
  * @brief  把闹钟设置保存到后备寄存器
  */
void Alarm_Save(void)
{
    /* 先写数据、最后写 magic，避免保存过程中掉电后 magic 有效但数据是旧值 */
    BKP_WriteBackupRegister(ALARM_BKP_REG_TIME, (uint16_t)(((uint16_t)alarm_cfg.hour << 8) | alarm_cfg.min));
    BKP_WriteBackupRegister(ALARM_BKP_REG_ENABLE, alarm_cfg.enable);
    BKP_WriteBackupRegister(ALARM_BKP_REG_MAGIC, ALARM_BKP_MAGIC);
}
