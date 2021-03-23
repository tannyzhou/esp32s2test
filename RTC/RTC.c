
#include <string.h>
#include <stdio.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"


#include <sys/time.h>

#include <esp_http_server.h>
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "RTC_GMT.h"

static const char *TAG = "RTC";
typedef struct __TimeStr
{
    unsigned char HourStr[2];
    unsigned char div1;
    unsigned char MinStr[2];
    unsigned char div2;
    unsigned char SecStr[2];
}TimeStr;
typedef struct __LocTimeInfo
{
	unsigned char WeekStr[3];
	unsigned char MonthStr[4];
	unsigned char DayStr[2];
	TimeStr TimeStr;
	unsigned char YearStr[4];
}LocTimeInfo;

LocTimeInfo TimeInfoStr;
struct timeval TimeNow;
time_t now;
char strftime_buf[32];
struct tm timeinfo;

void RTC_Task(void *pvParameters)
{
    
    while(1)
    {
        time(&now);
        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
        ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);
        vTaskDelay(10000 / portTICK_RATE_MS);
    }

    vTaskDelete(NULL);
}

void GetRtcTime(uint8_t *HourGet,uint8_t *MinuteGet,uint8_t *SecondGet)
{
    time(&now);
    *SecondGet = now % 60;
    *MinuteGet = (now/60) % 60;
    *HourGet = ((now/3600) % 24) + 8;
}

void SetRtcTime(time_t GMTset)
{
    TimeNow.tv_sec = GMTset;
    sntp_sync_time(&TimeNow);
    time(&now);
}


void RTC_Main(void)
{

    // settimeofday(1610702918	,+8);
    // gettimeofday();
    // TimeNow.tv_sec = 1611018651;
    // sntp_sync_time(&TimeNow);

    // time(&now);
    SetRtcTime(1611210232);
    // Set timezone to China Standard Time
    setenv("TZ", "CST-8", 1);
    tzset();

    xTaskCreate(RTC_Task, "RTC_Task",4096, NULL,6,NULL);

    // localtime_r(&now, &timeinfo);
    // strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    // ESP_LOGI(TAG, "The current date/time in Shanghai is: %s", strftime_buf);
}