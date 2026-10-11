/**
  *
  * Copyright (c) 2023 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */


#include "platform.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "vl53l4cd_espidf.h"

static i2c_master_dev_handle_t vl53_i2c_handle;

void pass_device_handle(i2c_master_dev_handle_t device_handle) {

	if (device_handle != NULL) {

		vl53_i2c_handle = device_handle;

	}

}

uint8_t VL53L4CD_RdDWord(Dev_t dev, uint16_t RegisterAdress, uint32_t *value)
{	
	(void)dev;
	uint8_t status = 255;
	
	/* To be filled by customer. Return 0 if OK */
	/* Warning : For big endian platforms, fields 'RegisterAdress' and 'value' need to be swapped. */
	
	//2 byte register buffer
	uint8_t reg[2];
	
	reg[0] = (uint8_t) (RegisterAdress >> 8 );
	reg[1] = (uint8_t) (RegisterAdress & 0xFF);

	//2 byte value buffer
	uint8_t buffer[4];

	//Some I2C device needs write configurations before reading data from it so we use transmit_receive here
	esp_err_t err = i2c_master_transmit_receive(vl53_i2c_handle, reg, sizeof(reg), buffer, sizeof(buffer), -1);
	
	if (err != ESP_OK || value == NULL) 
	return status;

	//combine array values into single 32 bit value
	*value = (((uint32_t)buffer[0] << 24) | ((uint32_t)buffer[1]<<16) | ((uint32_t)buffer[2]<<8) | ((uint32_t)buffer[3]));

	return 0;
}

uint8_t VL53L4CD_RdWord(Dev_t dev, uint16_t RegisterAdress, uint16_t *value)
{	
	(void)dev;
	uint8_t status = 255;
	
	/* To be filled by customer. Return 0 if OK */
	/* Warning : For big endian platforms, fields 'RegisterAdress' and 'value' need to be swapped. */

	//2 byte register buffer
	uint8_t reg[2];
	
	reg[0] = (uint8_t) (RegisterAdress >> 8 );
	reg[1] = (uint8_t) (RegisterAdress & 0xFF);

	//2 byte value buffer
	uint8_t buffer[2];

	//Some I2C device needs write configurations before reading data from it so we use transmit_receive here
	esp_err_t err = i2c_master_transmit_receive(vl53_i2c_handle, reg, sizeof(reg), buffer, sizeof(buffer), -1);
	
	if (err != ESP_OK || value == NULL) 
	return status;

	*value = ((uint16_t)buffer[0]<<8 | (uint16_t)buffer[1]);

	return 0;
}

uint8_t VL53L4CD_RdByte(Dev_t dev, uint16_t RegisterAdress, uint8_t *value)
{	
	(void)dev;
	uint8_t status = 255;
	
	/* To be filled by customer. Return 0 if OK */
	/* Warning : For big endian platforms, fields 'RegisterAdress' and 'value' need to be swapped. */

	//2 byte register buffer
	uint8_t reg[2];
	//store upper 8 bits
	reg[0] = (uint8_t) (RegisterAdress >> 8 );
	//store lower 8 bits
	reg[1] = (uint8_t) (RegisterAdress & 0xFF);

	if (value == NULL) 
	return status;

	//Some I2C device needs write configurations before reading data from it so we use transmit_receive here
	esp_err_t err = i2c_master_transmit_receive(vl53_i2c_handle, reg, sizeof(reg), value, sizeof(*value), -1);


	return err != ESP_OK ? status : 0;
}

uint8_t VL53L4CD_WrByte(Dev_t dev, uint16_t RegisterAdress, uint8_t value)
{	
	(void)dev;
	uint8_t status = 255;

	/* To be filled by customer. Return 0 if OK */
	/* Warning : For big endian platforms, fields 'RegisterAdress' and 'value' need to be swapped. */
	
	uint8_t buffer[3];

	buffer[0] = (uint8_t)(RegisterAdress >> 8);
	buffer[1] = (uint8_t)(RegisterAdress & 0xFF);
	buffer[2] = value;

	esp_err_t err = i2c_master_transmit(vl53_i2c_handle, buffer, sizeof(buffer), -1);

	return err != ESP_OK ? status : 0;
}

uint8_t VL53L4CD_WrWord(Dev_t dev, uint16_t RegisterAdress, uint16_t value)
{	
	(void)dev;
	uint8_t status = 255;
	
	/* To be filled by customer. Return 0 if OK */
	/* Warning : For big endian platforms, fields 'RegisterAdress' and 'value' need to be swapped. */
	uint8_t buffer[4];

	buffer[0] = (uint8_t)(RegisterAdress >> 8);
	buffer[1] = (uint8_t)(RegisterAdress & 0xFF);
	buffer[2] = (uint8_t)(value>>8);
	buffer[3] = (uint8_t)(value & 0xFF);

	esp_err_t err = i2c_master_transmit(vl53_i2c_handle, buffer, sizeof(buffer), -1);




	return err == ESP_OK ? 0 : status;
}

uint8_t VL53L4CD_WrDWord(Dev_t dev, uint16_t RegisterAdress, uint32_t value)
{	
	(void)dev;
	uint8_t status = 255;

	/* To be filled by customer. Return 0 if OK */
	/* Warning : For big endian platforms, fields 'RegisterAdress' and 'value' need to be swapped. */
	uint8_t buffer[6];

	buffer[0] = (uint8_t)(RegisterAdress >> 8);
	buffer[1] = (uint8_t)(RegisterAdress & 0xFF);
	buffer[2] = (uint8_t)(value>>24);
	buffer[3] = (uint8_t)(value>>16);
	buffer[4] = (uint8_t)(value>>8);
	buffer[5] = (uint8_t)(value & 0xFF);

	esp_err_t err = i2c_master_transmit(vl53_i2c_handle, buffer, sizeof(buffer), -1);




	return err == ESP_OK ? 0 : status;
}

uint8_t VL53L4CD_WaitMs(Dev_t dev, uint32_t TimeMs)
{	
	//avoid unused paramater warning
	(void)dev;
	//uint8_t status = 255;
	/* To be filled by customer */
	vTaskDelay(pdMS_TO_TICKS(TimeMs));

	return 0;
}


