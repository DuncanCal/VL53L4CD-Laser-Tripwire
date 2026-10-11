idf_component_register(
        SRCS "VL53L4CD_api.c"
            "platform.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_driver_i2c freertos 
)