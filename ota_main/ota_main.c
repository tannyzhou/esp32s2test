/* Advanced HTTPS OTA example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "protocol_examples_common.h"

#include "../AppParameter.h"

#if CONFIG_EXAMPLE_CONNECT_WIFI
#include "esp_wifi.h"
#endif


static const char *TAG = "https_ota";
extern const uint8_t server_cert_pem_start[] asm("_binary_ca_cert_pem_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_ca_cert_pem_end");

#define OTA_URL_SIZE 256

static esp_err_t validate_image_header(esp_app_desc_t *new_app_info)
{
    if (new_app_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_app_desc_t running_app_info;
    if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK) {
        ESP_LOGI(TAG, "Running firmware version: %s", running_app_info.version);
    }

#ifndef CONFIG_EXAMPLE_SKIP_VERSION_CHECK
    if (memcmp(new_app_info->version, running_app_info.version, sizeof(new_app_info->version)) == 0) {
        ESP_LOGW(TAG, "Current running version is the same as a new. We will not continue the update.");
        return ESP_FAIL;
    }
#endif

    return ESP_OK;
}

void example_configure_file(void)
{
    char * filepath = "/spiffs/update.bin";
    esp_ota_handle_t update_handle;
    const esp_partition_t *update_partition=esp_ota_get_next_update_partition(NULL);
    bool image_header_was_checked = false;
    uint8_t fd_data[1024];
    uint32_t DataLen=0;
    FILE *fd = NULL;
    esp_err_t err=ESP_OK;

    fd = fopen(filepath, "r");
    if (!fd) {
        ESP_LOGE(TAG, "Failed to read existing file : %s", filepath);
        /* Respond with 500 Internal Server Error */
        return ESP_FAIL;
    }
    while (1) {
        // int data_read = esp_http_client_read(client, ota_write_data, BUFFSIZE);
        int data_read = fread(fd_data, 1, sizeof(fd_data), fd);
        if (data_read > 0) {
            if (image_header_was_checked == false) {
                esp_app_desc_t new_app_info;
                if (data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
                    // check current version with downloading
                    // if (esp_efuse_check_secure_version(new_app_info.secure_version) == false) {
                    //     ESP_LOGE(TAG, "This a new app can not be downloaded due to a secure version is lower than stored in efuse.");
                    // }

                    image_header_was_checked = true;

                    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
                    if (err != ESP_OK) {
                        image_header_was_checked = false;
                        ESP_LOGE(TAG, "esp_ota_begin failed! err=0x%d", err);
                    }
                    ESP_LOGE(TAG, "esp_ota_begin address : %x", update_handle);
                }
            }
            if (image_header_was_checked == true)
            {
                DataLen += data_read;
                ESP_LOGI(TAG, "Writing data size: %d bytes, total: %d", data_read,DataLen);
                err = esp_ota_write( update_handle, (const void *)fd_data, data_read);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "esp_ota_write Err: %d", err);
                    break;
                }
            }
            vTaskDelay(1);
        }
        else if(image_header_was_checked==true)
        {
            ESP_LOGI(TAG, "esp_ota_set_boot_partition finish, set end ");
            esp_ota_end(update_handle);
            ESP_LOGI(TAG, "esp_ota_set_boot_partition set next boot partition ");
            err = esp_ota_set_boot_partition(&update_handle);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%d", err);
            }
            break;
        }
    }
    ESP_LOGI(TAG, "esp_ota_set_boot_partition close file and end update ");
    fclose(fd);
    // free(fd_data);
}

void advanced_ota_example_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting Advanced OTA example");
    struct stat st;
    do
    {
    if((stat("/spiffs/update.bin", &st) != 0))
    {
        while(FwUpdateReady==0){//while (stat("/spiffs/update.bin", &st) != 0) {
            ESP_LOGI(TAG, "no update.bin file");
            // Delete it if it exists
            //unlink("/spiffs/foo.txt");
            vTaskDelay(5000 / portTICK_RATE_MS);
        }
    }
    // vTaskDelay(20000 / portTICK_RATE_MS);
    ESP_LOGI(TAG, "update.bin file ready");
    FwUpdateReady = 0;
    esp_err_t ota_finish_err = ESP_OK;
    esp_http_client_config_t config = {
        .url = "http://192.168.4.1/update.bin",//"http://192.168.4.1/update.bin",
        // .cert_pem = NULL,
        .timeout_ms = 50000,
    };

    example_configure_file();
    ESP_LOGI(TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
    // Delete it if it exists
    unlink("/spiffs/update.bin");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    esp_restart();

#ifdef CONFIG_EXAMPLE_FIRMWARE_UPGRADE_URL_FROM_STDIN
    char url_buf[OTA_URL_SIZE];
    if (strcmp(config.url, "FROM_STDIN") == 0) {
        example_configure_stdin_stdout();
        fgets(url_buf, OTA_URL_SIZE, stdin);
        int len = strlen(url_buf);
        url_buf[len - 1] = '\0';
        config.url = url_buf;
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong firmware upgrade image url");
        abort();
    }
#endif

#ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
    config.skip_cert_common_name_check = true;
#endif

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
        //vTaskDelete(NULL);
    }

    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_read_img_desc failed");
        goto ota_end;
    }
    err = validate_image_header(&app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "image header verification failed");
        goto ota_end;
    }

    while (1) {
        ESP_LOGD(TAG, "esp_https_ota_perform");
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        // esp_https_ota_perform returns after every read operation which gives user the ability to
        // monitor the status of OTA upgrade by calling esp_https_ota_get_image_len_read, which gives length of image
        // data read so far.
        ESP_LOGD(TAG, "Image bytes read: %d", esp_https_ota_get_image_len_read(https_ota_handle));
        //esp_task_wdt_reset();
    }

    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        // the OTA image was not completely received and user can customise the response to this situation.
        ESP_LOGE(TAG, "Complete data was not received.");
    }

ota_end:
    ota_finish_err = esp_https_ota_finish(https_ota_handle);
    if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) {
        ESP_LOGI(TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
        // Delete it if it exists
        unlink("/spiffs/update.bin");
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_restart();
    } else {
        // Delete it if it exists
        unlink("/spiffs/update.bin");
        if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        }
        ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed %d", ota_finish_err);
        //vTaskDelete(NULL);
    }

    ESP_LOGI(TAG, "Restart OTA function");
    } while (1);
}

void ota_main(void)
{
//     // Initialize NVS.
//     esp_err_t err = nvs_flash_init();
//     if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
//         // 1.OTA app partition table has a smaller NVS partition size than the non-OTA
//         // partition table. This size mismatch may cause NVS initialization to fail.
//         // 2.NVS partition contains data in new format and cannot be recognized by this version of code.
//         // If this happens, we erase NVS partition and initialize NVS again.
//         ESP_ERROR_CHECK(nvs_flash_erase());
//         err = nvs_flash_init();
//     }
//     ESP_ERROR_CHECK( err );

//     ESP_ERROR_CHECK(esp_netif_init());
//     ESP_ERROR_CHECK(esp_event_loop_create_default());

//     /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
//      * Read "Establishing Wi-Fi or Ethernet Connection" section in
//      * examples/protocols/README.md for more information about this function.
//     */
//     ESP_ERROR_CHECK(example_connect());

// #if CONFIG_EXAMPLE_CONNECT_WIFI
//     /* Ensure to disable any WiFi power save mode, this allows best throughput
//      * and hence timings for overall OTA operation.
//      */
//     esp_wifi_set_ps(WIFI_PS_NONE);
// #endif // CONFIG_EXAMPLE_CONNECT_WIFI
    xTaskCreate(&advanced_ota_example_task, "advanced_ota_example_task", 1024 * 8, NULL, 5, NULL);
}

