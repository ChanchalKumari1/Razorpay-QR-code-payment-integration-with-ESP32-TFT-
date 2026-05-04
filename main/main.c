#include "upi_defines.h"
#include "upi_wifi.h"
#include "upi_server.h"
#include "upi_api.h"
#include "upi_payment.h"

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    qr_task_queue = xQueueCreate(5, sizeof(qr_task_data_t));
    if (qr_task_queue)
    {
        xTaskCreate(qr_api_task, "qr_api_task", 16384, NULL, 5, NULL);
        ESP_LOGI(TAG, "QR API task created");
    }

    ESP_LOGI(TAG, "Starting UPI Payment System");
    wifi_init_sta();

    tft_init(); 
    ESP_LOGI(TAG, "TFT Display initialized");

    vTaskDelay(2000 / portTICK_PERIOD_MS);
    start_web_server();

    ESP_LOGI(TAG, "System Ready!");

    while (1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}