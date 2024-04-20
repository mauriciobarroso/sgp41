/**
  ******************************************************************************
  * @file           : sgp41.h
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef SGP41_H_
#define SGP41_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "driver/i2c_master.h"

/* Exported Macros -----------------------------------------------------------*/
#define SGP41_I2C_ADDR						0x59
#define SGP41_I2C_BUFFER_LEN_MAX	8

/* SGP41 commands */
#define SPG41_EXECUTE_CONDITIONING_CMD	0x2612
#define SPG41_MESASURE_RAW_SIGNALS_CMD	0x2619
#define SPG41_EXECUTE_SELF_TEST_CMD			0x280E
#define SPG41_TURN_HEATER_FF_CMD				0x3615
#define SPG41_GET_SERIAL_NUMBER_CMD			0x3682

/* Exported typedef ----------------------------------------------------------*/
typedef struct {
	i2c_master_dev_handle_t i2c_dev;					/*!< I2C device handle */
} sgp41_t;

/* Exported variables --------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
/**
 * @brief Function that initialize a SGP41 instance
 *
 * @param me       : Pointer to a sgp41_t instance
 * @param i2c_bus  : Pointer to a structure with the data to initialize the
 * 								   I2C device
 * @param dev_addr : I2C device address
 *
 * @return ESP_OK on success
 */
esp_err_t sgp41_init(sgp41_t *const me, i2c_master_bus_handle_t i2c_bus_handle,
		uint8_t dev_addr);

/**
 * @brief Function that starts the conditioning, i.e., the VOC pixel will be
 * operated at the same temperature as it is by calling the sgp41_measure_raw
 * command while the NOx pixel will be operated at a different temperature for
 * conditioning. Function that returns only the measured raw signal of the VOC
 * pixel SRAW_VOC as 2 bytes (+ 1 CRC byte).
 *
 * @param me       : Pointer to a sgp41_t instance
 * @param default_rh Default conditions for relative humidty.
 * @param default_t Default conditions for temperature.
 * @param sraw_voc u16 unsigned integer directly provides the raw signal
 * SRAW_VOC in ticks which is proportional to the logarithm of the resistance of
 * the sensing element.
 *
 * @return 0 on success, an error code otherwise
 */
esp_err_t sgp41_execute_conditioning(sgp41_t *const me, uint16_t default_rh,
		                                 uint16_t default_t, uint16_t *sraw_voc);

/**
 * @brief Function that starts/continues the VOC+NOx
 * measurement mode
 *
 * @param me                : Pointer to a sgp41_t instance
 * @param relative_humidity : Leaves humidity compensation disabled by sending
 * the default value 0x8000 (50%RH) or enables humidity compensation when
 * sending the relative humidity in ticks (ticks = %RH * 65535 / 100)
 * @param temperature       : Leaves humidity compensation disabled by sending
 * the default value 0x6666 (25 degC) or enables humidity compensation when
 * sending the temperature in ticks (ticks = (degC + 45) * 65535 / 175)
 * @param sraw_voc          : u16 unsigned integer directly provides the raw
 * signal SRAW_VOC in ticks which is proportional to the logarithm of the
 * resistance of the sensing element.
 * @param sraw_nox          : u16 unsigned integer directly provides the raw
 * signal SRAW_NOX in ticks which is proportional to the logarithm of the
 * resistance of the sensing element.
 *
 * @return 0 on success, an error code otherwise
 */
esp_err_t sgp41_measure_raw_signals(sgp41_t *const me, uint16_t relative_humidity,
		                                uint16_t temperature, uint16_t *sraw_voc,
																		uint16_t *sraw_nox);

/**
 * @brief Function that triggers the built-in self-test checking for integrity
 * of both hotplate and MOX material and returns the result of this test as 2
 * bytes.
 *
 * @param me          : Pointer to a sgp41_t instance
 * @param test_result : 0xXX 0xYY: ignore most significant byte 0xXX. The four
 * least significant bits of the least significant byte 0xYY provide information
 * if the self-test has or has not passed for each individual pixel. All zero
 * mean all tests passed successfully. Check the datasheet for more detailed
 * information.
 *
 * @return 0 on success, an error code otherwise
 */
esp_err_t sgp41_execute_self_test(sgp41_t *const me, uint16_t *test_result);

/**
 * @brief Function that turns the hotplate off and stops the
 * measurement. Subsequently, the sensor enters the idle mode.
 *
 * @param me : Pointer to a sgp41_t instance
 *
 * @return 0 on success, an error code otherwise
 */
esp_err_t sgp41_turn_heater_off(sgp41_t *const me);

/**
 * @brief Function that provides the decimal serial number of the SGP41 chip by
 * returning 3x2 bytes.
 *
 * @param me            : Pointer to a sgp41_t instance
 * @param serial_number : 48-bit unique serial number
 *
 * @return 0 on success, an error code otherwise
 */
esp_err_t sgp41_get_serial_number(sgp41_t *const me, uint16_t *serial_number);

#ifdef __cplusplus
}
#endif

#endif /* SGP41_H_ */

/***************************** END OF FILE ************************************/
