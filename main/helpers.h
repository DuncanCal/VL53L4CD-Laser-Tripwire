#ifndef HELPERS_H
#define HELPERS_H


void i2c_init_bus(void);

void i2c_receive(void *pvParameters);

void wifi_init(void);

void mqtt_start(void);

void handle_distance(uint16_t distance_mm);

#endif
