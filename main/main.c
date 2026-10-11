// Provides standard input/output declarations (included for general C support).
#include <stdio.h>
// Defines portable integer format macros, such as PRIu32 used when logging ADC values.
#include <inttypes.h>


#include "helpers.h"

#include "VL53L4CD_api.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"


// ESP-IDF logging macros, including ESP_LOGI, ESP_LOGW, and ESP_LOGE.
#include "esp_log.h"
// ESP-IDF error types and helpers, including ESP_ERROR_CHECK and esp_err_to_name.
#include "esp_err.h"

#include "nvs_flash.h"


static const char *TAG = "wifi station";


TaskHandle_t taskhandle1 = NULL;


void app_main(void)
{   

    i2c_init_bus();


    //Intialize ESP32 Non-volatile storage (NVS) partition which stores persistent data, including wifi configuration
    //save to ret_wifi so result can be checked

    //returns a status code 
    esp_err_t ret_wifi = nvs_flash_init();

    //ESP_ERR_NVS_NO_FREE_PAGES if the NVS storage contains 
    //no empty pages (which may happen if NVS partition was truncated)
    //ESP_ERR_NOT_FOUND if no partition with label "nvs" is found in the partition table
    if (ret_wifi == ESP_ERR_NVS_NO_FREE_PAGES || ret_wifi == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        //erase NVS partition and try intialization again
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret_wifi = nvs_flash_init();
    }
    
    ESP_ERROR_CHECK(ret_wifi);
    
    if (CONFIG_LOG_MAXIMUM_LEVEL > CONFIG_LOG_DEFAULT_LEVEL) {

        //If you only want to open more logs in the wifi module, you need to make the max level greater than the default level,
        //and call esp_log_level_set() before esp_wifi_init() to improve the log level of the wifi module.
        esp_log_level_set("wifi", CONFIG_LOG_MAXIMUM_LEVEL);
    }
   
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

    // Initialize Wi-Fi connection
    wifi_init(); 

    mqtt_start();
    
    xTaskCreate(i2c_receive, "I2C_RECEIVE", 2048, NULL, 1, &taskhandle1);

}


