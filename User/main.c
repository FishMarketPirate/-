#include "stm32f10x.h"
#include "Delay.h"
#include "MyRTC.h"
#include "Time.h"
#include "Key.h"
#include "Encoder.h"
#include "DH11.h"
#include "Buzzer.h"
#include "OLEDShow.h"

#define MODE_NORMAL       0     /* ������ʾ */
#define MODE_EDIT_TIME    1     /* �޸�ʱ�� */
#define MODE_EDIT_ALARM   2     /* �޸����� */
#define MODE_RING         3     /* �������� */

#define TEMP_READ_LOOPS   200   /* ÿ 200 ����ѭ����ȡһ����ʪ�� */
#define RING_BEEP_LOOPS   20    /* ÿ 20 �ַ�תһ�η����� */
#define RING_BLINK_LOOPS  50    /* ÿ 50 ����˸һ����ʾ */
#define RING_TIMEOUT_SEC  30    /* ����� 30 �� */

static uint8_t mode = MODE_NORMAL;

/* �༭ʱʹ�õ���ʱʱ��/���ӣ�����������д�� */
static RTC_TimeTypeDef edit_time;
static ALARM_TypeDef edit_alarm;
static uint8_t edit_field = 0;



/* ��ʪ�� */
static uint8_t temp_value = 0;
static uint8_t hum_value = 0;
static uint8_t th_valid = 0;
static uint16_t temp_loop = 0;

/* ����״̬ */
static uint32_t ring_start_sec = 0;
static uint16_t ring_loop = 0;
static uint8_t ring_display_on = 1;

static uint16_t last_minute_key = 0xFFFF;   /* ������⡰�����µ�һ���ӡ� */
static uint8_t display_sec = 0xFF;          /* �ϴ�ˢ�µ����� */

/**
  * @brief  ��ʾ���������棨ʱ�� + ��ʪ�� + ���ӣ�
  */
static void Main_ShowHome(void)
{
    OLEDShow_ShowTime(&now_time);
    OLEDShow_ShowTempHum(temp_value, hum_value, th_valid);
    OLEDShow_ShowAlarm(&alarm_cfg);
    display_sec = now_time.sec;
}

/**
  * @brief  ��ȡһ����ʪ�ȣ�DHT11 ��Ҫ���� 1 ��Ĳ��������
  */
static void Main_ReadTempHum(void)
{
    th_valid = DH11_Rec_Data();
    if (th_valid)
    {
        temp_value = rec_data[2];
        hum_value = rec_data[0];
    }
}

/**
  * @brief  ������
  * @note   ����˵����
  *         KEY1(PA1) ����������롰�޸�ʱ�䡱���༭���汣�沢����
  *         KEY2(PA3) ����������롰�޸����ӡ����༭�����л��޸���
  *         ��������ת ������ǰ�޸������ֵ
  *         ����������(PA9) �������濪�����ӣ��༭��������޸ķ���
  *         ��������ʱ���������ת������������ֹͣ
  */
int main(void)
{
    uint32_t tick;
    uint16_t minute_key;
    int16_t delta;
    uint8_t key;
    uint8_t enc_key;

    MyRTC_Init();        /* RTC ���������ʼ������������ Time_Init�� */
    Time_Init();         /* ������ǰʱ�䣬���Ӻ󱸼Ĵ����ָ����� */
    Key_Init();
    Encoder_Init();
    Buzzer_Init();
    OLEDShow_Init();
    Buzzer_OFF();

    Time_Read(&now_time);
    Main_ReadTempHum();  /* �ϵ��ȶ�һ����ʪ�� */
    Main_ShowHome();

    while (1)
    {
        tick = MyRTC_GetCounter();
        Time_Read(&now_time);

        key = Key_GetNum();
        enc_key = Encoder_Key_Get();
        delta = Encoder_Get();

        /* �����µ�һ����ʱ������ӣ���������ͱ༭���涼����Ӧ��
         * ��ֹ�����ý�����������ʱ�䵼��©�� */
        minute_key = (uint16_t)(((uint16_t)now_time.hour << 8) | now_time.min);
        if (minute_key != last_minute_key)
        {
            last_minute_key = minute_key;
            if (alarm_cfg.enable &&
                now_time.hour == alarm_cfg.hour &&
                now_time.min == alarm_cfg.min &&
                mode != MODE_RING)
            {
                /* ������ڱ༭���津���ģ��Ȼָ������棬���⻭����� */
                if (mode == MODE_EDIT_TIME || mode == MODE_EDIT_ALARM)
                {
                    Main_ShowHome();
                }
                mode = MODE_RING;
                ring_start_sec = tick;
                ring_loop = 0;
                ring_display_on = 1;
                Buzzer_ON();
                OLEDShow_ShowRing(1);
                /* ������һ�ֲ����İ���/�������������ϣ����������ͱ�����ͣ�� */
                key = 0;
                enc_key = 0;
                delta = 0;
            }
        }

        switch (mode)
        {
            case MODE_NORMAL:
                /* ��仯ʱ��ˢ��ʱ�䣬����Ƶ��ˢ�� */
                if (now_time.sec != display_sec)
                {
                    display_sec = now_time.sec;
                    OLEDShow_ShowTime(&now_time);
                }

                if (enc_key)            /* ��������������/�ر����� */
                {
                    alarm_cfg.enable = alarm_cfg.enable ? 0 : 1;
                    Alarm_Save();
                    OLEDShow_ShowAlarm(&alarm_cfg);
                }
                else if (key == 1)      /* KEY1�������޸�ʱ�� */
                {
                    edit_time = now_time;
                    edit_field = TIME_FIELD_HOUR;
                    mode = MODE_EDIT_TIME;
                    OLEDShow_EditTime(&edit_time, edit_field);
                }
                else if (key == 2)      /* KEY2�������޸����� */
                {
                    edit_alarm = alarm_cfg;
                    edit_field = ALARM_FIELD_HOUR;
                    mode = MODE_EDIT_ALARM;
                    OLEDShow_EditAlarm(&edit_alarm, edit_field);
                }
                break;

            case MODE_EDIT_TIME:
                if (delta != 0)         /* ��ת������������ֵ */
                {
                    Time_Adjust(&edit_time, edit_field, delta);
                    OLEDShow_EditTime(&edit_time, edit_field);
                }

                if (key == 2)           /* KEY2���л�����һ���޸��� */
                {
                    edit_field++;
                    if (edit_field >= TIME_FIELD_NUM)
                    {
                        edit_field = 0;
                    }
                    OLEDShow_EditTime(&edit_time, edit_field);
                }
                else if (key == 1)      /* KEY1�����沢���� */
                {
                    Time_Set(&edit_time);
                    Time_Read(&now_time);
                    mode = MODE_NORMAL;
                    Main_ShowHome();
                }
                else if (enc_key)       /* �����������������޸ķ��� */
                {
                    mode = MODE_NORMAL;
                    Main_ShowHome();
                }
                break;

            case MODE_EDIT_ALARM:
                if (delta != 0)
                {
                    Alarm_Adjust(&edit_alarm, edit_field, delta);
                    OLEDShow_EditAlarm(&edit_alarm, edit_field);
                }

                if (key == 2)
                {
                    edit_field++;
                    if (edit_field >= ALARM_FIELD_NUM)
                    {
                        edit_field = 0;
                    }
                    OLEDShow_EditAlarm(&edit_alarm, edit_field);
                }
                else if (key == 1)      /* �������� */
                {
                    alarm_cfg = edit_alarm;
                    Alarm_Save();
                    mode = MODE_NORMAL;
                    Main_ShowHome();
                }
                else if (enc_key)
                {
                    mode = MODE_NORMAL;
                    Main_ShowHome();
                }
                break;

            case MODE_RING:
                ring_loop++;
                if ((ring_loop % RING_BEEP_LOOPS) == 0)
                {
                    Buzzer_Turn();      /* ���������Ρ����Ρ����� */
                }
                if ((ring_loop % RING_BLINK_LOOPS) == 0)
                {
                    ring_display_on = ring_display_on ? 0 : 1;
                    OLEDShow_ShowRing(ring_display_on);
                }

                /* ����ʱʱ������� */
                if (now_time.sec != display_sec)
                {
                    display_sec = now_time.sec;
                    OLEDShow_ShowTime(&now_time);
                }

                /* ���ⰴ��/ת��������ֹͣ��30 ����Զ�ֹͣ */
                if (key != 0 || enc_key || delta != 0 ||
                    (uint32_t)(tick - ring_start_sec) >= RING_TIMEOUT_SEC)
                {
                    Buzzer_OFF();
                    mode = MODE_NORMAL;
                    Main_ShowHome();
                }
                break;

            default:
                mode = MODE_NORMAL;
                Main_ShowHome();
                break;
        }

        /* ��ʱ��ȡ��ʪ�� */
        temp_loop++;
        if (temp_loop >= TEMP_READ_LOOPS)
        {
            temp_loop = 0;
            Main_ReadTempHum();
            if (mode == MODE_NORMAL)
            {
                OLEDShow_ShowTempHum(temp_value, hum_value, th_valid);
            }
        }

        Delay_ms(10);
    }
}
