idf_component_register(SRCS "main.c" "helpers.c"
                    INCLUDE_DIRS "."
                    REQUIRES vl53l4cd_uld esp_driver_i2c esp_wifi esp_event nvs_flash mqtt bt)
