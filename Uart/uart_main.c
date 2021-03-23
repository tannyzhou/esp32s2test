/* UART asynchronous example, that uses separate RX and TX tasks

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

#include "../AppParameter.h"

static const int RX_BUF_SIZE = 1024;
static uint8_t TX_BUF[32],TXing_BUF[32],RX_Command[32],TxBufReady;

#define TXD_PIN (GPIO_NUM_14)
#define RXD_PIN (GPIO_NUM_15)

uint8_t GetChecksum(uint8_t *Datas, uint8_t DataLen)
{
    uint8_t ChecksumData=0;
    for(;DataLen>0;DataLen--)
    {
        ChecksumData += Datas[DataLen-1];
    }
    ChecksumData = ~ChecksumData;
    ChecksumData++;
    // ESP_LOGI("CheckSum","%02X\n",ChecksumData);
    return ChecksumData;
}

void init(void) {
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

int sendData(const char* logName, const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}


static void tx_task(void *arg)
{
    static const char *TX_TASK_TAG = "TX_TASK";
    esp_log_level_set(TX_TASK_TAG, ESP_LOG_INFO);
    while (1) {
        // sendData(TX_TASK_TAG, "Hello world");
        if(TxBufReady)
        {
            ESP_LOGI(TX_TASK_TAG, "Tx %d bytes: '%s'", TX_BUF[1]+3, TX_BUF);
            ESP_LOG_BUFFER_HEXDUMP(TX_TASK_TAG, TX_BUF, TX_BUF[1]+3, ESP_LOG_INFO);
            uart_wait_tx_done(UART_NUM_1,50);
            memcpy(TXing_BUF,TX_BUF,TX_BUF[1]+3);
            uart_write_bytes(UART_NUM_1, (char *)TXing_BUF, TXing_BUF[1]+3);
            TxBufReady = 0;
        }
        vTaskDelay(25 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

static void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    uint16_t i=0;
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 50 / portTICK_RATE_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
            i = 0;
            while(i<rxBytes)
            {
                if(data[i]==0xa8)
                {
                    if(TxBufReady)
                        vTaskDelay(20 / portTICK_PERIOD_MS);
                    if(TxBufReady)
                        vTaskDelay(20 / portTICK_PERIOD_MS);
                    memcpy(RX_Command,&data[i],data[i+1]+3);
                    memcpy(TX_BUF,RX_Command,RX_Command[1]+3);
                    TX_BUF[0] = 0xf8;
                    TX_BUF[1] += 1;

                    ESP_LOGI(RX_TASK_TAG, "Command %d bytes: '%s'", RX_Command[1]+3, RX_Command);
                    ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, RX_Command, RX_Command[1]+3, ESP_LOG_INFO);
                    if(RX_Command[RX_Command[1]+2]==GetChecksum(RX_Command,RX_Command[1]+2))//checksum OK
                    {
                        ESP_LOGI(RX_TASK_TAG, "CheckSum OK");
                        TX_BUF[TX_BUF[1]+1] = 0;
                        TX_BUF[TX_BUF[1]+2]=GetChecksum(TX_BUF,TX_BUF[1]+2);
                        TxBufReady = 1;
                    }
                    if(RcrValue>0)
                    {
                        PowerNow = VoltageOutput*VoltageOutput;
                        PowerNow = (float)PowerNow / RcrValue;
                    }
                    // vTaskDelay(50 / portTICK_PERIOD_MS);
                }
                i++;
            }
        }
    }
    free(data);
    vTaskDelete(NULL);
}

void uart_main(void)
{
    init();
    TX_BUF[0]=0xa8;
    TX_BUF[1]=0x02;
    TX_BUF[2]=0x01;
    TX_BUF[3]=0;
    GetChecksum(TX_BUF, 4);
    xTaskCreate(rx_task, "uart_rx_task", 1024*2, NULL, configMAX_PRIORITIES, NULL);
    xTaskCreate(tx_task, "uart_tx_task", 1024*2, NULL, configMAX_PRIORITIES-1, NULL);
}
