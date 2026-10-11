#include <stdio.h>

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "vl53l4cd_espidf.h"
#include "VL53L4CD_api.h"

// Defines portable integer format macros, such as PRIu32 used when logging ADC values.
#include <inttypes.h>

// ESP-IDF logging macros, including ESP_LOGI, ESP_LOGW, and ESP_LOGE.
#include "esp_log.h"

// ESP-IDF error types and helpers, including ESP_ERROR_CHECK and esp_err_to_name.
#include "esp_err.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"

//Requires you to add dependency since ESP-MQTT is not available in ESP-IDF Version 6.0
//idf.py add-dependency "espressif/mqtt"
#include "mqtt_client.h"


//WIFI 

//Home wifi credentials
#define WIFI_NETWORK_SSID "My_Network"
#define WIFI_NETWORK_PASSWORD "Password"
#define WIFI_TIMEOUT_MS 20000

#define BROKER_URI "broker"


#define WIFI_MAXIMUM_RETRY  5

//we can control multiple bits for each event but we will only use two
//one bit for when we connect to the AP with an IP
//one bit for when we fail to connect after a max num of retries
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry_num = 0;

static const char *TAG = "wifi station";
static const char *TAG2 = "mqtt";

//FreeRTOS event group to signal when we are connected
static EventGroupHandle_t wifi_event_group;

//handle for device
static i2c_master_bus_handle_t bus_handle;
i2c_master_dev_handle_t vl53_i2c_handle;


static bool mqtt_connected = false;
static esp_mqtt_client_handle_t client = NULL;


//I2C 
void i2c_init_bus(void)
{
   
    //Configure specific I2C master bus configurations
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT, //default internal clock
        .i2c_port = -1, // tells esp to automatically select
        .scl_io_num = GPIO_NUM_22,
        .sda_io_num = GPIO_NUM_21,
        .glitch_ignore_cnt = 7, //max theshold duration of a pulse treated as noise, used to filter out noise pulses
        .flags.enable_internal_pullup = false, //enable/disable internal pull up resistors

    };

  
    //i2c device configurations
    i2c_device_config_t  dev_cfg = {

        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = 0x29, //VL53L4CD_I2C_ADDRESS
        .scl_speed_hz = 100000,

    };

    //Allocate a new I2C master bus instance
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));

    //Allocate a new I2C device instance mounted to master bus
    esp_err_t err = i2c_master_bus_add_device(bus_handle, &dev_cfg, &vl53_i2c_handle);

    if (err == ESP_OK) {
        pass_device_handle(vl53_i2c_handle);
    } else {
        ESP_ERROR_CHECK(err);
    }
}





//event handler for WIFI and MQTT event groups
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {

     //Handle WiFi Events
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START ) {

        //Connect wifi station to AP
        ESP_ERROR_CHECK(esp_wifi_connect());
        
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {

        const wifi_event_sta_disconnected_t *disconnected = event_data;
        ESP_LOGW(TAG, "Disconnected; reason=%d", disconnected->reason);

        if (s_retry_num < WIFI_MAXIMUM_RETRY) {

            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Retrying connection to AP");

        } else {

            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGI(TAG, "Connection to AP failed");
        }

    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // DHCP (Dynamic Host Configuration Protocol) completed station can now communicate on the network
        ESP_LOGI(TAG, "Connection to AP Successful");
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);

    }

    //Handle MQTT events
    switch (event_id) {

        case MQTT_EVENT_CONNECTED:
            mqtt_connected = true;
            ESP_LOGI(TAG2, "MQTT EVENT CONNECTED");

        break;

        case MQTT_EVENT_DISCONNECTED: /*!< disconnected event */
            mqtt_connected = false;
            ESP_LOGI(TAG2, "MQTT EVENT DISCONNECTED");

        break;

        case MQTT_EVENT_PUBLISHED: 
            ESP_LOGI(TAG2, "Message Published");

        break;

        default:
        break;

    }



    




}




//MQTT
void mqtt_start(void) {

    esp_mqtt_client_config_t mqtt_cfg = {

        .broker.address.uri = BROKER_URI,
        //1
        .session.disable_clean_session = false

    };

    client = esp_mqtt_client_init(&mqtt_cfg);

    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, event_handler, NULL);

    esp_mqtt_client_start(client);

}



//motion flag
static bool motion_active = false;

static bool no_change = false;

//motion flag
static bool out_of_range = false;

//need 32 bits to accommodate operations between 16 bit int values
static int32_t last_distance_mm = -1;


//Handles distance measurement logic
//Detects if distance changes by more than a minimum delta

void handle_distance(uint16_t distance_mm) {

    //sets default threshold
    uint32_t motion_threshold_mm = 15;

    //check sensor is within range for accurate readings
    if ((distance_mm <= 0 ) || (distance_mm > 1200)){

        out_of_range = true;

        return;
    }
    else {

        out_of_range = false;

    }

    int32_t dist_change = (last_distance_mm >= 0) ? abs(distance_mm-last_distance_mm) : 0;

    if (dist_change > motion_threshold_mm) {

        motion_active = true;
        no_change = false;

    }
    else if ((dist_change < motion_threshold_mm) && (dist_change > 0)) {

        motion_active = false;
        no_change = true;

    }

    last_distance_mm = distance_mm;

}



//VL53L4CD SENSOR

static const char *TAG1 = "VL53L4CD Sensor";

void i2c_receive(void *pvParameters) {

    //VL53L4CD Device instance
    Dev_t dev = 0;

    uint8_t error_check = VL53L4CD_SensorInit(dev);

    VL53L4CD_ResultsData_t result;

    uint8_t ready = 0;


    if (error_check == VL53L4CD_ERROR_NONE) {
        VL53L4CD_StartRanging(dev);
    }

    while(1){

        VL53L4CD_CheckForDataReady(dev, &ready);

        if (ready == 1) {

            error_check = VL53L4CD_GetResult(dev, &result);

            if (error_check == VL53L4CD_ERROR_NONE && result.range_status == 0) {

                //store distance measurements
                char payload[54];

                ESP_LOGI(TAG1, "Distance in mm: %" PRIu16 "\n", result.distance_mm);

                handle_distance(result.distance_mm);

                if (motion_active && !out_of_range) {

                    snprintf(payload, sizeof(payload), "Movement detected, Distance (mm): %u",  result.distance_mm);
                    esp_mqtt_client_publish(client, "sensors/vl53l4cd/distance_mm", payload, 0, 1, 0);
                    vTaskDelay(pdMS_TO_TICKS(1000));

                } else if(out_of_range) {

                    snprintf(payload, sizeof(payload), "Out of Range");
                    esp_mqtt_client_publish(client, "sensors/vl53l4cd/distance_mm", payload, 0, 1, 0);

                } else if (!out_of_range && no_change) {

                    snprintf(payload, sizeof(payload), "No Movement detected, Distance (mm): %u",  result.distance_mm);
                    esp_mqtt_client_publish(client, "sensors/vl53l4cd/distance_mm", payload, 0, 1, 0);

                }

            }

            VL53L4CD_ClearInterrupt(dev);
            

        }
        vTaskDelay(pdMS_TO_TICKS(10));

    }

}





//WIFI


void wifi_init() {


    // Initialize Wi-Fi 
    ESP_LOGI(TAG, "Initializing Wi-Fi...");

    wifi_event_group = xEventGroupCreate();

    //Intialize TCP/IP stack, provides hardware with logic required
    //to translate raw wireless signals into structured web data
    ESP_ERROR_CHECK(esp_netif_init());


    //Event loop allows events to be sent to a dedicated background task queue where 
    //handlers are executed sequentially
    //Compared to ISR where you must hardcode direct function calls
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    //init default station
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //event handler for event generated by Wi-Fi event system 
    esp_event_handler_instance_t instance_any_id;
    //event handler for event generated by IP event system, station received IP
    esp_event_handler_instance_t instance_got_ip;

    //Connect event_handler to WIFI_EVENTS
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));

    //Connect event_handler to IP_EVENTS
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_got_ip));

    //Wifi config struct
    wifi_config_t wifi_config = {

        .sta = {
            .ssid = WIFI_NETWORK_SSID,
            .password = WIFI_NETWORK_PASSWORD,
        },
    }; 

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    
    //Wait indefinitely for wifi event bits to be set
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);


    // xEventGroupWaitBits() returns the bits before the call returned, test which event actually
    
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", WIFI_NETWORK_SSID, WIFI_NETWORK_PASSWORD);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", WIFI_NETWORK_SSID, WIFI_NETWORK_PASSWORD);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }


}




