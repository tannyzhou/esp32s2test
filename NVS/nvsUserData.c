  
/* Non-Volatile Storage (NVS) Read and Write a Value - Example
   For other examples please check:
   https://github.com/espressif/esp-idf/tree/master/examples
   This example code is in the Public Domain (or CC0 licensed, at your option.)
   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "nvsUserData.h"

#include "../tft/TftLcdSpi.h"

static const char *TAG = "nvsUserData";


void ShowUserBmp(void)
{
    uint8_t fdData[240*2*4],i;
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "userbmp");
    for(i=0;i<240;i+=4)
    {
        esp_partition_read(partition,i*240*2,fdData,sizeof(fdData));
        TftDisplay(0,i,240,i+4,fdData);
        // if(i==0)
        // {
        //     ESP_LOGI(TAG, "rgb565 data sample");
        //     ESP_LOG_BUFFER_HEXDUMP(TAG, fdData, 32, ESP_LOG_INFO);
        // }
    }
    
}

void UserBmpMixBlg(uint8_t *BlgData)
{
    uint8_t fdData[240*2*10],i;
    uint16_t j,rgbTemp;
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "userbmp");
    for(i=0;i<240;i+=10)
    {
        esp_partition_read(partition,i*240*2,fdData,sizeof(fdData));
        for(j=0;j<4800;j+=2)
        {
            rgbTemp = (*(BlgData+i*480+j));
            rgbTemp <<= 8;
            rgbTemp += (*(BlgData+i*480+j+1));
            if(rgbTemp>0)
            {
                fdData[j]=*(BlgData+i*480+j);
                fdData[j+1]=*(BlgData+i*480+j+1);
            }
        }
        TftDisplay(0,i,240,i+10,fdData);
    }
}

void WriteUserBmp(const char *filepath)
{
    nvs_handle handle;
    // char * filepath = "/spiffs/userbmp.bmp";
    FILE *fd = NULL;
    size_t chunksize;
    uint8_t fdData[240*3],i=0,j=0;
    uint16_t rgbTemp,rgbGet;
    static const char *NVS_CUSTOMER = "userData";
    static const char *DATA1 = "userbmp";

    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "userbmp");
    
    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        return;
    }
    chunksize = fread(fdData, 1, 54, fd);//read bmp header
    if(fdData[0]!=0x42 || fdData[1]!=0x4d || fdData[28]!=0x18 || fdData[18]!=0xf0 ){
        // ESP_LOGE(TAG, "errdata, not bmp data:%s", fdData);
        ESP_LOG_BUFFER_HEXDUMP(TAG, fdData, 54, ESP_LOG_INFO);
        fclose(fd);
        return;
    }
    ESP_ERROR_CHECK(esp_partition_erase_range(partition, 0, partition->size));
    for(i=0;i<240;i++)
    {
        chunksize = fread(fdData, 1, sizeof(fdData), fd);
        if(i==0)
            ESP_LOG_BUFFER_HEXDUMP(TAG, fdData, 32, ESP_LOG_INFO);
        for(j=0;j<240;j++)
        {
            // rgbGet = fdData[i*3];
            // rgbTemp = (rgbGet*32/256);
            // rgbGet = fdData[i*3+1];
            // rgbTemp += (rgbGet*64/256) << 5;
            // rgbGet = fdData[i*3+2];
            // rgbTemp += (rgbGet*32/256) << 11;

            rgbGet = fdData[j*3+2];
            rgbTemp = (rgbGet<<8)&0xf800;
            rgbGet = fdData[j*3+1];
            rgbTemp |= (rgbGet<<3)&0x07e0;
            rgbGet = fdData[j*3];
            rgbTemp |= (rgbGet>>3)&0x01f;

            fdData[j*2] = rgbTemp&0xff;
            fdData[j*2+1] = (rgbTemp>>8)&0xff;
        }
        esp_partition_write(partition, (239-i)*240*2, fdData, 240*2);
        if(i==0)
        {
            ESP_LOGI(TAG, "rgb565 data sample");
            ESP_LOG_BUFFER_HEXDUMP(TAG, fdData, 32, ESP_LOG_INFO);
        }
    }
    fclose(fd);
    ESP_LOGI(TAG, "save bmp OK");


    // uint8_t *tftData,bmpData;
    // tftData = malloc(240*2*10);
    // bmpData = malloc(240*3*10);

    // ESP_ERROR_CHECK( nvs_open( NVS_CUSTOMER, NVS_READWRITE, &handle) );
    // ESP_ERROR_CHECK( nvs_set_str( handle, DATA1, "i am string for testing") );
    // ESP_ERROR_CHECK( nvs_set_str( handle, DATA1, (char *)tftData) );
    // ESP_ERROR_CHECK( nvs_set_str( handle, DATA1, empty_data) );
    
    // ESP_ERROR_CHECK( nvs_commit(handle) );
    // nvs_close(handle);
    // free(tftData);
    // free(bmpData);


    // ESP_LOGI(TAG, "nvs test start");

    // uint8_t str[240*2*10]="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789",i,str2[50];
    // //如何确定大小？
    // // const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, NULL);
    // const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "UserData");
    // esp_partition_read(partition,2,str2,30);
    // ESP_LOGI(TAG, "%s",str2);
    // ESP_LOGI(TAG, "Read OK, erase.....");
    // ESP_ERROR_CHECK(esp_partition_erase_range(partition, 0, partition->size));
    // ESP_LOGI(TAG, "%s",str);
    // for(i=0;i<24;i++)
    //     esp_partition_write(partition, i*240*2*10, str, sizeof(str));
    // esp_partition_read(partition,2,str2,30);
    // ESP_LOGI(TAG, "%s",str2);
    // ESP_LOGI(TAG, "nvs test finish");

    
}


/*
void nvs_write_data_to_flash(void)
{
    nvs_handle handle;
    static const char *NVS_CUSTOMER = "customer data";
    static const char *DATA1 = "param 1";
    static const char *DATA2 = "param 2";
    static const char *DATA3 = "param 3";

    int32_t value_for_store = 666;

     wifi_config_t wifi_config_to_store = {
        .sta = {
            .ssid = "store_ssid:hello_kitty",
            .password = "store_password:1234567890",
        },
    };

    printf("set size:%u\r\n", sizeof(wifi_config_to_store));
    ESP_ERROR_CHECK( nvs_open( NVS_CUSTOMER, NVS_READWRITE, &handle) );
    ESP_ERROR_CHECK( nvs_set_str( handle, DATA1, "i am a string.") );
    ESP_ERROR_CHECK( nvs_set_i32( handle, DATA2, value_for_store) );
    ESP_ERROR_CHECK( nvs_set_blob( handle, DATA3, &wifi_config_to_store, sizeof(wifi_config_to_store)) );

    ESP_ERROR_CHECK( nvs_commit(handle) );
    nvs_close(handle);

}

void nvs_read_data_from_flash(void)
{
    nvs_handle handle;
    static const char *NVS_CUSTOMER = "customer data";
    static const char *DATA1 = "param 1";
    static const char *DATA2 = "param 2";
    static const char *DATA3 = "param 3";

    uint32_t str_length = 32;
    char str_data[32] = {0};
    int32_t value = 0;
    wifi_config_t wifi_config_stored;
    memset(&wifi_config_stored, 0x0, sizeof(wifi_config_stored));
    uint32_t len = sizeof(wifi_config_stored);

    ESP_ERROR_CHECK( nvs_open(NVS_CUSTOMER, NVS_READWRITE, &handle) );

    ESP_ERROR_CHECK ( nvs_get_str(handle, DATA1, str_data, &str_length) );
    ESP_ERROR_CHECK ( nvs_get_i32(handle, DATA2, &value) );
    ESP_ERROR_CHECK ( nvs_get_blob(handle, DATA3, &wifi_config_stored, &len) );

    printf("[data1]: %s len:%u\r\n", str_data, str_length);
    printf("[data2]: %d\r\n", value);
    printf("[data3]: ssid:%s passwd:%s\r\n", wifi_config_stored.sta.ssid, wifi_config_stored.sta.password);

    nvs_close(handle);
}

*/


