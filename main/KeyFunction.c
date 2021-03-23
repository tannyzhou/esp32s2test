
#include <string.h>
#include <stdio.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include <sys/time.h>

#include "KeyFunction.h"



static const char *TAG = "Key";

#define KeyDelay 20

KeyInfo KeyIn[6]={
    {KeyUpPin,KeyRelease,0,0},
    {KeyDownPin,KeyRelease,0,0},
    {KeyLeftPin,KeyRelease,0,0},
    {KeyRightPin,KeyRelease,0,0},
    {KeyCenterPin,KeyRelease,0,0},
    {KeyFirePin,KeyRelease,0,0}
};

void Key_Task(void *pvParameters)
{

    while(1)
    {
        for(int i=0;i<6;i++)
        {
            if(gpio_get_level(KeyIn[i].KeyPin)==0)
            {
                if(KeyIn[i].KeyCount<0)
                    KeyIn[i].KeyCount=0;
                else if(KeyIn[i].KeyCount<0x7fff)
                    KeyIn[i].KeyCount++;
                if(KeyIn[i].KeyCount>3000/KeyDelay)
                    KeyIn[i].KeyStatus = KeyLongpress;
                else if(KeyIn[i].KeyCount>50/KeyDelay)
                    KeyIn[i].KeyStatus = KeyShortpress;
            }
            else
            {
                if(KeyIn[i].KeyCount>0)
                    KeyIn[i].KeyCount=0;
                else if(KeyIn[i].KeyCount > (-800/KeyDelay))
                {
                    KeyIn[i].KeyCount--;
                }
                else
                    KeyIn[i].KeyPressCount = 0;

                if(KeyIn[i].KeyStatus==KeyShortpress)
                {
                    KeyIn[i].KeyStatus = KeyShortpressRelease;
                    KeyIn[i].KeyPressCount++;
                    ESP_LOGI(TAG, "Key %d ShortPress, %d clicks", KeyIn[i].KeyPin,KeyIn[i].KeyPressCount);
                }
                if(KeyIn[i].KeyStatus==KeyLongpress)
                {
                    KeyIn[i].KeyStatus = KeyLongpressRelease;
                    // ESP_LOGI(TAG, "Key %d LongPress", KeyIn[i].KeyPin);
                    // KeyIn[i].KeyPressCount++;
                }
            }
        }
        vTaskDelay(KeyDelay / portTICK_RATE_MS);
    }

    vTaskDelete(NULL);
}


void KeyInit(void)
{
    gpio_config_t GpioConfig;
    for(int i=0;i<6;i++)
    {
        GpioConfig.pin_bit_mask = 1ULL << (KeyIn[i].KeyPin);
        GpioConfig.mode = GPIO_MODE_INPUT;
        GpioConfig.pull_up_en = GPIO_PULLDOWN_ENABLE;
        GpioConfig.pull_down_en = GPIO_PULLDOWN_DISABLE;
        GpioConfig.intr_type = GPIO_INTR_DISABLE;
        
        gpio_config(&GpioConfig);
    }

    xTaskCreate(Key_Task, "Key_Task",4096, NULL,6,NULL);
}