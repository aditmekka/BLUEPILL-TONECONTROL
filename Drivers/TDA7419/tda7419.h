/*
 * tda7419.h
 *
 *  Created on: Aug 11, 2025
 *      Author: aditr
 */

#ifndef TDA7419_H
#define TDA7419_H

#include "main.h"
#include "stm32f1xx_hal.h" // Change this depending on your STM32 family
#include <stdlib.h>

#define TDA7419_ADDR 0x44 << 1  // 7-bit address shifted for STM32 HAL

// Register addresses (define more as needed)
#define TDA7419_INPUT        0x00  // Main Source Selector
#define TDA7419_LOUDNESS     0x01  // Main Loudness
#define TDA7419_SOFTMUTE     0x02  // Soft Mute / Clock
#define TDA7419_VOLUME       0x03  // Volume
#define TDA7419_TREBLE       0x04  // Treble
#define TDA7419_MIDDLE       0x05  // Middle
#define TDA7419_BASS         0x06  // Bass
#define TDA7419_INPUT2       0x07  // Second Source Selector
#define TDA7419_SUB_M_B      0x08  // Subwoofer / Middle / Bass frequency
#define TDA7419_MIX_GAIN     0x09  // Mixing / Gain Effect
#define TDA7419_ATT_LF       0x0A  // Left Front Attenuator
#define TDA7419_ATT_RF       0x0B  // Right Front Attenuator
#define TDA7419_ATT_LR       0x0C  // Left Rear Attenuator
#define TDA7419_ATT_RR       0x0D  // Right Rear Attenuator
#define TDA7419_MIX_LEVEL    0x0E  // Mixing Level Control
#define TDA7419_ATT_SUB      0x0F  // Subwoofer Attenuator
#define TDA7419_SA_CLKMODE   0x10  // Spectrum Analyzer / Clock / AC Mode
#define TDA7419_TEST         0x11  // Testing Audio Processor


#define TDA7419_NUM_BANDS 7

typedef struct {
    GPIO_TypeDef *clkPort;
    uint16_t clkPin;
    ADC_HandleTypeDef *hadc;
    uint32_t adcChannel;
} TDA7419_SA_Config;

// Function declarations
void TDA7419_Init(I2C_HandleTypeDef *hi2c);
void TDA7419_SetInput(uint8_t input, uint8_t gain, uint8_t auto_z);
void TDA7419_SetLoudness(int8_t dB, uint8_t c_freq, _Bool treble_boost, _Bool soft);
void TDA7419_SetAttenuation(uint8_t reg, int8_t db, uint8_t soft);
void TDA7419_SetVolume(int8_t dB, uint8_t soft);
void TDA7419_SetAttLF(int8_t dB, uint8_t soft);
void TDA7419_SetAttRF(int8_t dB, uint8_t soft);
void TDA7419_SetAttLR(int8_t dB, uint8_t soft);
void TDA7419_SetAttRR(int8_t dB, uint8_t soft);
void TDA7419_SetAttSub(int8_t dB, uint8_t soft);
void TDA7419_SoftMuteConfig(_Bool mute_en, _Bool mute_mode, uint8_t mute_time, uint8_t soft_step_time, _Bool clock_mode);
void TDA7419_SetTreble(int8_t gain, uint8_t corner, uint8_t ref);
void TDA7419_SetCenterSMB(uint8_t sub, uint8_t mid, uint8_t bass, _Bool bass_dc, _Bool smooth_filter);
void TDA7419_SetMiddle(int8_t gain, uint8_t q, uint8_t slope);
void TDA7419_SetBass(int8_t gain, uint8_t q, uint8_t slope);
void TDA7419_ReadSpectrum(TDA7419_SA_Config *cfg, uint16_t *levels);

#endif
