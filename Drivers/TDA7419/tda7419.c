/*
 * tda7419.c
 *
 *  Created on: Aug 11, 2025
 *      Author: aditr
 */

#include "tda7419.h"

static I2C_HandleTypeDef *tda_i2c;

static void TDA7419_WriteReg(uint8_t reg, uint8_t data) {
    uint8_t buf[2];
    buf[0] = (0b010 << 5) | (reg & 0x1F);  // prefix 010 + A4..A0
    buf[1] = data;
    HAL_I2C_Master_Transmit(tda_i2c, TDA7419_ADDR, buf, 2, HAL_MAX_DELAY);
}


void TDA7419_Init(I2C_HandleTypeDef *hi2c) {
    tda_i2c = hi2c;
}

void TDA7419_SetInput(uint8_t input, uint8_t gain, uint8_t auto_z) {
    if (input > 5) input = 0;
    if (gain > 15) gain = 0;

    uint8_t val = (gain << 3) | input;
    if (auto_z) val |= 0x80;
    TDA7419_WriteReg(TDA7419_INPUT, val);
}

void TDA7419_SetLoudness(int8_t dB, uint8_t c_freq, _Bool treble_boost, _Bool soft){
	if(c_freq > 3) c_freq = 0;

	uint8_t val;
	val = abs(dB);
	val |= (c_freq << 5);

	if(treble_boost) val |= 0x40;
	if(soft) val |= 0x80;
	TDA7419_WriteReg(TDA7419_LOUDNESS, val);
}

void TDA7419_SetAttenuation(uint8_t reg, int8_t db, uint8_t soft) {
    uint8_t val;
    if (db >= 0) val = db;
    else val = (uint8_t)(abs(db) + 16); // Convert to datasheet format

    if (soft) val |= 0x80;
    TDA7419_WriteReg(reg, val);
}

void TDA7419_SetVolume(int8_t dB, uint8_t soft){
	TDA7419_SetAttenuation(TDA7419_VOLUME, dB, soft);
}

void TDA7419_SetAttLF(int8_t dB, uint8_t soft){
	TDA7419_SetAttenuation(TDA7419_ATT_LF, dB, soft);
}

void TDA7419_SetAttRF(int8_t dB, uint8_t soft){
	TDA7419_SetAttenuation(TDA7419_ATT_RF, dB, soft);
}

void TDA7419_SetAttLR(int8_t dB, uint8_t soft){
	TDA7419_SetAttenuation(TDA7419_ATT_LR, dB, soft);
}

void TDA7419_SetAttRR(int8_t dB, uint8_t soft){
	TDA7419_SetAttenuation(TDA7419_ATT_RR, dB, soft);
}

void TDA7419_SetAttSub(int8_t dB, uint8_t soft){
	TDA7419_SetAttenuation(TDA7419_ATT_SUB, dB, soft);
}

void TDA7419_SoftMuteConfig(_Bool mute_en, _Bool mute_mode, uint8_t mute_time, uint8_t soft_step_time, _Bool clock_mode){
	if(mute_time > 2) mute_time = 0;
	if(soft_step_time > 7) soft_step_time = 0;

	uint8_t val = 0;

	if(mute_en) val |= 0x01;
	if(mute_mode) val |= 0x02;

	val |= (mute_time << 2);
	val |= (soft_step_time << 4);

	if(clock_mode) val |= 0x80;

	TDA7419_WriteReg(TDA7419_SOFTMUTE, val);
}

void TDA7419_SetTreble(int8_t gain, uint8_t corner, uint8_t ref) {
    if (corner > 3) corner = 0;
    if (ref > 1) ref = 0;

    uint8_t val = 0;
    if (gain < 0) val = abs(gain);
    else val = 0x10 | gain;

    val |= (corner << 5);
    if (ref) val |= 0x80;

    TDA7419_WriteReg(TDA7419_TREBLE, val);
}

void TDA7419_SetCenterSMB(uint8_t sub, uint8_t mid, uint8_t bass, _Bool bass_dc, _Bool smooth_filter){
	if(sub > 3) sub = 0;
	if(mid > 3) mid = 0;
	if(bass > 3) bass = 0;

	uint8_t reg = 0;
	reg |= sub;
	reg |= (mid << 2);
	reg |= (bass << 4);

	if(bass_dc)	reg |= 0x40;
	if(smooth_filter) reg |= 0x80;
	TDA7419_WriteReg(TDA7419_SUB_M_B, reg);
}

void TDA7419_SetMiddle(int8_t gain, uint8_t q, uint8_t slope) {
    if (q > 3) q = 0;
    if (slope > 1) slope = 0;

    uint8_t val = 0;
    if (gain < 0) val = abs(gain);
    else val = 0x10 | gain;

    val |= (q << 5);
    if (slope) val |= 0x80;

    TDA7419_WriteReg(TDA7419_MIDDLE, val);
}

void TDA7419_SetBass(int8_t gain, uint8_t q, uint8_t slope) {
    if (q > 3) q = 0;
    if (slope > 1) slope = 0;

    uint8_t val = 0;
    if (gain < 0) val = abs(gain);
    else val = 0x10 | gain;

    val |= (q << 5);
    if (slope) val |= 0x80;

    TDA7419_WriteReg(TDA7419_BASS, val);
}

static void SAClk_Pulse(TDA7419_SA_Config *cfg) {
    HAL_GPIO_WritePin(cfg->clkPort, cfg->clkPin, GPIO_PIN_RESET);
    HAL_Delay(1); // hold low briefly
    HAL_GPIO_WritePin(cfg->clkPort, cfg->clkPin, GPIO_PIN_SET);
    HAL_Delay(1); // hold high briefly
}

void TDA7419_ReadSpectrum(TDA7419_SA_Config *cfg, uint16_t *levels) {
    for (uint8_t band = 0; band < TDA7419_NUM_BANDS; band++) {
        SAClk_Pulse(cfg);  // Select next band
        HAL_Delay(1);      // Wait tSADEL (datasheet delay before reading)

        // Start ADC conversion
        ADC_ChannelConfTypeDef sConfig = {0};
        sConfig.Channel = cfg->adcChannel;
        sConfig.Rank = ADC_REGULAR_RANK_1;
        sConfig.SamplingTime = ADC_SAMPLETIME_28CYCLES_5;
        HAL_ADC_ConfigChannel(cfg->hadc, &sConfig);

        HAL_ADC_Start(cfg->hadc);
        HAL_ADC_PollForConversion(cfg->hadc, HAL_MAX_DELAY);
        levels[band] = HAL_ADC_GetValue(cfg->hadc);
        HAL_ADC_Stop(cfg->hadc);
    }
}
