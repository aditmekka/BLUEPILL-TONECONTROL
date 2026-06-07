/*
 * u8g2_stm32_hal.c
 *
 *  Created on: Aug 2, 2025
 *      Author: aditr
 */

#include "u8g2_stm32_hal.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c2; // use your I2C handle

uint8_t u8x8_byte_stm32_hw_i2c(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr)
{
  static uint8_t buffer[32];		/* u8g2/u8x8 will never send more than 32 bytes between START_TRANSFER and END_TRANSFER */
  static uint8_t buf_idx;
  uint8_t *data;

  switch(msg)
  {
    case U8X8_MSG_BYTE_SEND:
      data = (uint8_t *)arg_ptr;
      while( arg_int > 0 )
      {
	buffer[buf_idx++] = *data;
	data++;
	arg_int--;
      }
      break;
    case U8X8_MSG_BYTE_START_TRANSFER:
      buf_idx = 0;
      break;
    case U8X8_MSG_BYTE_END_TRANSFER:
    	HAL_I2C_Master_Transmit(&hi2c2, u8x8_GetI2CAddress(u8x8) << 1, buffer, buf_idx, 1000);
      break;
    default:
      return 0;
  }
  return 1;
}

uint8_t u8x8_gpio_and_delay_stm32(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch(msg) {
        case U8X8_MSG_DELAY_MILLI:
            HAL_Delay(arg_int);
            break;
        case U8X8_MSG_GPIO_AND_DELAY_INIT:
            break;
        case U8X8_MSG_GPIO_RESET:
            // Implement if needed
            break;
        case U8X8_MSG_GPIO_DC:
        case U8X8_MSG_GPIO_CS:
            break;
        default:
            return 0;
    }
    return 1;
}

