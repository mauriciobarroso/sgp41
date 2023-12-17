/**
  ******************************************************************************
  * @file           : sgp41.c
  * @author         : Mauricio Barroso Benavides
  * @date           : Nov 29, 2023
  * @brief          : todo: write brief
  ******************************************************************************
  * @attention
  *
  * MIT License
  *
  * Copyright (c) 2023 Mauricio Barroso Benavides
  *
  * Permission is hereby granted, free of charge, to any person obtaining a copy
  * of this software and associated documentation files (the "Software"), to
  * deal in the Software without restriction, including without limitation the
  * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
  * sell copies of the Software, and to permit persons to whom the Software is
  * furnished to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in
  * all copies or substantial portions of the Software.
  * 
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  * IN THE SOFTWARE.
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "sgp41.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

/* Private macros ------------------------------------------------------------*/
#define NOP() asm volatile ("nop")

/* External variables --------------------------------------------------------*/

/* Private typedef -----------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/
static const char *TAG = "sgp41";

/* Private function prototypes -----------------------------------------------*/
/**
 * @brief Function that implements the default I2C read transaction
 *
 * @param reg_addr : Register address to be read
 * @param reg_data : Pointer to the data to be read from reg_addr
 * @param data_len : Length of the data transfer
 * @param intf     : Pointer to the interface descriptor
 *
 * @return 0 if successful, non-zero otherwise
 */
static int8_t i2c_read(uint16_t reg_addr, uint8_t *reg_data, uint32_t data_len,
		                   void *intf);
/**
 * @brief Function that implements the default I2C write transaction
 *
 * @param reg_addr : Register address to be written
 * @param reg_data : Pointer to the data to be written to reg_addr
 * @param data_len : Length of the data transfer
 * @param intf     : Pointer to the interface descriptor
 *
 * @return 0 if successful, non-zero otherwise
 */
static int8_t i2c_write(uint16_t reg_addr, const uint8_t *reg_data,
		                    uint32_t data_len, void *intf);
/**
 * @brief Function that implements a micro seconds delay
 *
 * @param period_us: Time in us to delay
 */
static void delay_us(uint32_t period_us);

/* Exported functions definitions --------------------------------------------*/
/**
 * @brief Function that initializes a SGP41 instance
 */
esp_err_t sgp41_init(sgp41_t *const me, i2c_bus_t *i2c_bus, uint8_t dev_addr,
		i2c_bus_read_t read, i2c_bus_write_t write) {
	/* Print initializing message */
	ESP_LOGI(TAG, "Initializing instance...");

	/* Variable to return error code */
	esp_err_t ret = ESP_OK;

	/* Add device to bus */
	ret = i2c_bus_add_dev(i2c_bus, dev_addr, "sgp41", NULL, NULL);

	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Failed to add device");
		return ret;
	}

	/**/
	me->i2c_dev = &i2c_bus->devs.dev[i2c_bus->devs.num - 1]; /* todo: write function to get the dev from name */

	/* Execute selff test */
	ESP_LOGI(TAG, "Executing self test...");
	uint16_t test_result;
	sgp41_execute_self_test(me, &test_result);

	if (test_result != 0xD400) {
		ESP_LOGE(TAG, "Self test failed with error: 0x%X", test_result);
	}
	else {
		ESP_LOGI(TAG, "Self test executed successfully");
	}

	/* Get and print serial number */
	sgp41_get_serial_number(me, me->serial_number);
	ESP_LOGI(TAG, "Serial number: 0X%04X%04X%04X\n", me->serial_number[0],
			me->serial_number[1], me->serial_number[2]);

	/* Print successful initialization message */
	ESP_LOGI(TAG, "Instance initialized successfully");

	/* Return ESP_OK */
	return ret;
}

/**
 * @brief Function that starts the conditioning, i.e., the VOC pixel will be
 * operated at the same temperature as it is by calling the sgp41_measure_raw
 * command while the NOx pixel will be operated at a different temperature for
 * conditioning. Function that returns only the measured raw signal of the VOC
 * pixel SRAW_VOC as 2 bytes (+ 1 CRC byte).
 */
esp_err_t sgp41_execute_conditioning(sgp41_t *const me, uint16_t default_rh,
		                                 uint16_t default_t, uint16_t *sraw_voc) {
	/* Variable to return error code */
	esp_err_t ret = ESP_OK;

	/* Conditioning and get signal raw VOC */
	uint8_t data_tx[4] = {(uint8_t)((default_rh >> 8) & 0xFF), (uint8_t)(default_rh & 0xFF),
			                  (uint8_t)((default_t >> 8) & 0xFF), (uint8_t)(default_t & 0xFF)};

	if (i2c_write(SPG41_EXECUTE_CONDITIONING_CMD, data_tx, 4, me->i2c_dev) < 0) {
		return ESP_FAIL;
	}

	delay_us(50 * 1000); /* Wait for 50 ms */

	uint8_t data_rx[3] = {0};

	if (i2c_read(0, data_rx, 3, me->i2c_dev) < 0) {
		return ESP_FAIL;
	}

	/* todo: check crc */

	*sraw_voc = (uint16_t)((data_rx[0] << 8) | (data_rx[1]));

	/* Return ESP_OK */
	return ret;
}

/**
 * @brief Function that starts/continues the VOC+NOx
 * measurement mode.
 */
esp_err_t sgp41_measure_raw_signals(sgp41_t *const me, uint16_t relative_humidity,
		                                uint16_t temperature, uint16_t *sraw_voc,
																		uint16_t *sraw_nox) {
	/* Variable to return error code */
	esp_err_t ret = ESP_OK;

	/* Get VOC and nOX raw signals */
	uint8_t data_tx[4] = {(uint8_t)((relative_humidity >> 8) & 0xFF), (uint8_t)(relative_humidity & 0xFF),
			                  (uint8_t)((temperature >> 8) & 0xFF), (uint8_t)(temperature & 0xFF)};

	if (i2c_write(SPG41_MESASURE_RAW_SIGNALS_CMD, data_tx, 4, me->i2c_dev) < 0) {
		return ESP_FAIL;
	}

	delay_us(50 * 1000); /* Wait for 50 ms */

	uint8_t data_rx[6] = {0};

	if (i2c_read(0, data_rx, 6, me->i2c_dev) < 0) {
		return ESP_FAIL;
	}

	/* todo: check crc */

	*sraw_voc = (uint16_t)((data_rx[0] << 8) | (data_rx[1]));
	*sraw_nox = (uint16_t)((data_rx[3] << 8) | (data_rx[4]));

	/* Return ESP_OK */
	return ret;
}

/**
 * @brief Function that triggers the built-in self-test checking for integrity
 * of both hotplate and MOX material and returns the result of this test as 2
 * bytes.
 */
esp_err_t sgp41_execute_self_test(sgp41_t *const me, uint16_t *test_result) {
	/* Variable to return error code */
	esp_err_t ret = ESP_OK;

	/* Execute a self test */
	if (i2c_write(SPG41_EXECUTE_SELF_TEST_CMD, NULL, 0, me->i2c_dev) < 0) {
		return ESP_FAIL;
	}

	delay_us(320 * 1000); /* Wait for 320 ms */

	uint8_t data_rx[3] = {0};

	if (i2c_read(0, data_rx, 3, me->i2c_dev) < 0) {
		return ESP_FAIL;
	}

	/* todo: check crc */

	*test_result = (uint16_t)((data_rx[0] << 8) | (data_rx[1]));

	/* Return ESP_OK */
	return ret;
}

/**
 * @brief Function that turns the hotplate off and stops the
 * measurement. Subsequently, the sensor enters the idle mode.
 */
esp_err_t sgp41_turn_heater_off(sgp41_t *const me) {
	/* Variable to return error code */
	esp_err_t ret = ESP_OK;

	/* Turn off the heater */
	if (i2c_write(SPG41_TURN_HEATER_FF_CMD, NULL, 0, me->i2c_dev) < 0) {
		return ESP_FAIL;
	}

	delay_us(1 * 1000); /* Wait for 1 ms */

	/* Return ESP_OK */
	return ret;
}

/**
 * @brief Function that provides the decimal serial number of the SGP41 chip by
 * returning 3x2 bytes.
 */
esp_err_t sgp41_get_serial_number(sgp41_t *const me, uint16_t *serial_number) {
	/* Variable to return error code */
	esp_err_t ret = ESP_OK;

	/* Get the serial number */
	if (i2c_write(SPG41_GET_SERIAL_NUMBER_CMD, NULL, 0, me->i2c_dev) < 0) {
		return ESP_FAIL;
	}

	delay_us(1 * 1000); /* Wait for 1 ms */

	uint8_t data_rx[9] = {0};

	if (i2c_read(0, data_rx, 9, me->i2c_dev) < 0) {
		return ESP_FAIL;
	}

	/* todo: check crc */

	serial_number[0] = (uint16_t)((data_rx[0] << 8) | (data_rx[1]));
	serial_number[1] = (uint16_t)((data_rx[3] << 8) | (data_rx[4]));
	serial_number[2] = (uint16_t)((data_rx[6] << 8) | (data_rx[7]));

	/* Return ESP_OK */
	return ret;
}

/* Private function definitions ----------------------------------------------*/
/**
 * @brief Function that implements the default I2C read transaction
 */
static int8_t i2c_read(uint16_t reg_addr, uint8_t *reg_data, uint32_t data_len,
		                   void *intf) {
	i2c_bus_dev_t *dev = (i2c_bus_dev_t *)intf;

	uint8_t reg[2] = {(uint8_t)((reg_addr & 0xFF00 ) >> 8),
			              (uint8_t)(reg_addr & 0x00FF)};

	return dev->read(reg_addr ? reg : NULL, reg_addr ? 2 : 0, reg_data, data_len, intf);
}

/**
 * @brief Function that implements the default I2C write transaction
 */
static int8_t i2c_write(uint16_t reg_addr, const uint8_t *reg_data,
		                    uint32_t data_len, void *intf) {
	i2c_bus_dev_t *dev = (i2c_bus_dev_t *)intf;

	uint8_t reg[2] = {(uint8_t)((reg_addr & 0xFF00 ) >> 8),
			(uint8_t)(reg_addr & 0x00FF)};

	return dev->write(reg, 2, reg_data, data_len, intf);
}

/**
 * @brief Function that implements a micro seconds delay
 */
static void delay_us(uint32_t period_us) {
	uint64_t m = (uint64_t)esp_timer_get_time();

  if (period_us) {
  	uint64_t e = (m + period_us);

  	if (m > e) { /* overflow */
  		while ((uint64_t)esp_timer_get_time() > e) {
  			NOP();
  		}
  	}

  	while ((uint64_t)esp_timer_get_time() < e) {
  		NOP();
  	}
  }
}

/***************************** END OF FILE ************************************/
