#include "stm32f10x.h"                  // Device header
#include <time.h>
#include "Delay.h"
#include "MyRTC.h"

/* 显示时间比 RTC 计数器快 8 小时（东八区）。
 * 读取时 +8h、写入时 -8h，两者互相抵消，保证设置后读回的时间准确。 */
#define TZ_OFFSET_SEC   (8 * 60 * 60)

uint16_t MyRTC_Time[] = {2026,10,25,21,31,55};

/**
  * @brief  读取 RTC 计数器
  * @note   库函数 RTC_GetCounter() 先读 CNTL 再读 CNTH，若在 CNTL 进位瞬间读取
  *         会得到一个偏大 65536 秒（约 18 小时）的值。这里读两次 CNTH 做校验。
  */
uint32_t MyRTC_GetCounter(void)
{
	uint32_t high1, high2, low;

	do
	{
		high1 = RTC->CNTH;
		low = RTC->CNTL;
		high2 = RTC->CNTH;
	} while (high1 != high2);

	return (high1 << 16) | (low & 0xFFFF);
}

/**
  * @brief  等待 RCC 标志置位（带超时）
  * @retval 1=置位 0=超时
  */
static uint8_t MyRTC_WaitFlag(uint8_t flag, uint16_t timeout_ms)
{
	uint16_t t = 0;

	while(RCC_GetFlagStatus(flag)!=SET && t<timeout_ms)
	{
		Delay_ms(1);
		t++;
	}
	return (RCC_GetFlagStatus(flag)==SET) ? 1 : 0;
}

/**
  * @brief  等待 RTC 寄存器同步（带超时）
  * @retval 1=同步完成 0=超时
  */
static uint8_t MyRTC_WaitSynchro(uint16_t timeout_ms)
{
	uint16_t t = 0;

	while(RTC_GetFlagStatus(RTC_FLAG_RSF)==RESET && t<timeout_ms)
	{
		Delay_ms(1);
		t++;
	}
	return (RTC_GetFlagStatus(RTC_FLAG_RSF)==SET) ? 1 : 0;
}

/**
  * @brief  等待 RTC 上一次写操作完成（带超时，避免 RTC 没有时钟时死等）
  */
static void MyRTC_WaitLastTask(void)
{
	uint16_t t = 100;

	while(RTC_GetFlagStatus(RTC_FLAG_RTOFF)==RESET && t>0)
	{
		Delay_ms(1);
		t--;
	}
}

/**
  * @brief  配置备份域和 RTC 时钟（只在首次上电或备份域异常时调用）
  * @note   优先使用外部 32.768kHz 晶振，最多等 5 秒；不起振则退回内部 LSI。
  */
static void MyRTC_ConfigBackupDomain(void)
{
	uint16_t wait_ms = 0;
	uint16_t prescaler;

	/* 打开外部 32.768kHz 晶振并等待起振 */
	RCC_LSEConfig(RCC_LSE_ON);
	while(RCC_GetFlagStatus(RCC_FLAG_LSERDY)!=SET && wait_ms<5000)
	{
		Delay_ms(1);
		wait_ms++;
	}

	if(RCC_GetFlagStatus(RCC_FLAG_LSERDY)==SET)
	{
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
		prescaler = 32768-1;
	}
	else
	{
		/* 晶振不起振：退回内部 LSI（精度较低，但不会上电黑屏死机） */
		RCC_LSEConfig(RCC_LSE_OFF);
		RCC_LSICmd(ENABLE);
		while(RCC_GetFlagStatus(RCC_FLAG_LSIRDY)!=SET);
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSI);
		prescaler = 40000-1;		/* LSI 约 40kHz */
	}

	RCC_RTCCLKCmd(ENABLE);

	MyRTC_WaitSynchro(500);
	MyRTC_WaitLastTask();

	/* 分频 */
	RTC_SetPrescaler(prescaler);
	MyRTC_WaitLastTask();

	/* 把 MyRTC_Time 里的初始时间写入 RTC */
	MyRTC_SetTime();

	BKP_WriteBackupRegister(BKP_DR1,0xA5A5);
}

void MyRTC_Init(void)
{
	uint32_t rtcsel;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR,ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_BKP,ENABLE);

	PWR_BackupAccessCmd(ENABLE);

	if(BKP_ReadBackupRegister(BKP_DR1)==0xA5A5)
	{
		/* 备份域已有时间：确认 RTC 时钟源是否仍在振荡。
		 * 注意 LSI 的 LSION 位在系统复位后会被清零，若不重新打开，RTC 会停走。 */
		uint8_t clock_ok = 0;

		rtcsel = RCC->BDCR & RCC_BDCR_RTCSEL;

		if(rtcsel==RCC_BDCR_RTCSEL_LSE)
		{
			RCC_LSEConfig(RCC_LSE_ON);
			clock_ok = MyRTC_WaitFlag(RCC_FLAG_LSERDY, 5000);
		}
		else if(rtcsel==RCC_BDCR_RTCSEL_LSI)
		{
			RCC_LSICmd(ENABLE);
			clock_ok = MyRTC_WaitFlag(RCC_FLAG_LSIRDY, 1000);
		}
		else if(rtcsel==RCC_BDCR_RTCSEL_HSE)
		{
			RCC_HSEConfig(RCC_HSE_ON);
			clock_ok = MyRTC_WaitFlag(RCC_FLAG_HSERDY, 2000);
		}

		if(clock_ok)
		{
			RCC_RTCCLKCmd(ENABLE);
			if(MyRTC_WaitSynchro(500))
			{
				MyRTC_WaitLastTask();
				return;			/* RTC 正常，沿用原来的时间和闹钟 */
			}
		}

		/* 时钟源异常：复位备份域后重新初始化（时间会回到初始值） */
		BKP_DeInit();
		PWR_BackupAccessCmd(ENABLE);
	}

	/* 首次上电或备份域刚被复位：重新配置 LSE/LSI、预分频和初始时间 */
	MyRTC_ConfigBackupDomain();
}


void MyRTC_SetTime(void)
{
	time_t time_cnt;
	struct tm time_date;

	time_date.tm_year = MyRTC_Time[0]-1900;
	time_date.tm_mon = MyRTC_Time[1]-1;
	time_date.tm_mday = MyRTC_Time[2];
	time_date.tm_hour = MyRTC_Time[3];
	time_date.tm_min = MyRTC_Time[4];
	time_date.tm_sec = MyRTC_Time[5];
	time_date.tm_wday = 0;		/* 下面三个成员必须初始化，否则 mktime 结果不确定 */
	time_date.tm_yday = 0;
	time_date.tm_isdst = 0;

	/* 读取时做了 +8 小时修正，写入时就要减回来，保证设置的时间准确 */
	time_cnt = mktime(&time_date) - TZ_OFFSET_SEC;

	RTC_SetCounter(time_cnt);
	MyRTC_WaitLastTask();
}


void MyRTC_ReadTime(void)
{
	time_t time_cnt;
	struct tm time_date;

	time_cnt = MyRTC_GetCounter() + TZ_OFFSET_SEC;

	time_date = *localtime(&time_cnt);

	MyRTC_Time[0] = time_date.tm_year +1900;
	MyRTC_Time[1] = time_date.tm_mon +1;
	MyRTC_Time[2] = time_date.tm_mday;
	MyRTC_Time[3] = time_date.tm_hour;
	MyRTC_Time[4] = time_date.tm_min;
	MyRTC_Time[5] = time_date.tm_sec ;
}
