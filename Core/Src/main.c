/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "u8g2.h"
#include "u8g2_stm32_hal.h"
#include "tda7419.h"

#include "bitmap_resources.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
u8g2_t u8g2;

typedef enum {
    BTN_NONE = 0,
    BTN_SHORT,
    BTN_LONG
} ButtonEvent_t;

typedef enum {
  EV_BTN1,
  EV_BTN2,
  EV_BTN3,
  EV_ENCODER_CW,
  EV_ENCODER_CCW,
  EV_ENCODER_SW_SHORT,
  EV_ENCODER_SW_LONG
} EventType_t;

typedef struct {
    const char *label;
    int8_t *value;
} MenuItem;

typedef struct{
	const char *label;
} SettingItems;

typedef struct{
	const char *label;
	const char *in_gain_label;
	uint8_t *gain;
} InputSelectItems;

typedef struct {
    int8_t master_volume;
    int8_t treble_tone_gain;
    int8_t middle_tone_gain;
    int8_t bass_tone_gain;
    uint8_t input_select;            // 0..n for available inputs
    _Bool soft_mute;
    _Bool buzzer_state;

    _Bool auto_z;
    uint8_t active_input;
    uint8_t in1_gain;
    uint8_t in2_gain;
    uint8_t in3_gain;

    _Bool mute_mode;
	uint8_t soft_mute_time;
	uint8_t soft_step_time;

	_Bool bass_dc;
	uint8_t treble_c_freq;
	uint8_t middle_c_freq;
	uint8_t middle_q_factor;
	uint8_t bass_c_freq;
	uint8_t bass_q_factor;
	uint8_t sub_cut_off;
	uint8_t loudness_c_freq;

	_Bool high_boost;
	uint8_t loudness_att;

	int8_t left_att;
	int8_t right_att;
	int8_t s_left_att;
	int8_t s_right_att;
	int8_t sub_att;

    uint16_t eeprom_save_interval;   // seconds; user-changeable in future (default 30)
} UserSettings_t;

/* Default settings instance */
UserSettings_t settings = {
    .master_volume = -20,
    .treble_tone_gain = 1,
    .middle_tone_gain = 2,
    .bass_tone_gain = 3,
    .input_select = 0,
    .soft_mute = 0,

    .buzzer_state = 1,

	.auto_z = 0,
	.active_input = 1,
	.in1_gain = 0,
	.in2_gain = 0,
	.in3_gain = 0,

	.mute_mode = 0,
	.soft_mute_time = 0,
	.soft_step_time = 0,

	.bass_dc = 1,
	.treble_c_freq = 0,
	.middle_c_freq = 0,
	.middle_q_factor = 0,
	.bass_c_freq = 0,
	.bass_q_factor = 0,
	.sub_cut_off = 0,
	.loudness_c_freq = 0,

	.high_boost = 0,
	.loudness_att = 0,

	.left_att = 0,
	.right_att = 0,
	.s_left_att = 0,
	.s_right_att = 0,
	.sub_att = 0,

    .eeprom_save_interval = 30
};

MenuItem main_menu_items[] = {
    {"VOLUME", &settings.master_volume},
    {"TREBLE", &settings.treble_tone_gain},
    {"MIDDLE", &settings.middle_tone_gain},
    {"BASS", &settings.bass_tone_gain}
};

MenuItem attenuator_menu_items[] = {
	{"LEFT", &settings.left_att},
	{"RIGHT", &settings.right_att},
	{"SUR L", &settings.s_left_att},
	{"SUR R", &settings.s_right_att},
	{"SUB", &settings.sub_att},
};

uint8_t attenuator_menu_slider = 0;
_Bool attenuator_selected = 0;

InputSelectItems input_select_items[] = {
	{"INPUT 1", "IN1 GAIN:", &settings.in1_gain},
	{"INPUT 2", "IN2 GAIN:", &settings.in2_gain},
	{"INPUT 3", "IN3 GAIN:", &settings.in3_gain}
};

uint8_t main_menu_slider = 0;
_Bool MAIN_SELECTED = 0;

typedef enum {
	MAIN_MENU,
	SETTING_MENU,
	INPUT_SELECT,
	MUTE_SETTING,
	TONE_SETTING,
	ATTENUATOR,
	BUZZER,
	CREDIT,

	TONE_SETTING_HIGH,
	TONE_SETTING_MID,
	TONE_SETTING_BASS,
	TONE_SETTING_SUB,
	TONE_SETTING_LOUDNESS,

	EEPROM_LOADING
} DisplayCmd_t;

uint8_t current_menu = 0;

SettingItems setting_items[] = {
    { "INPUT SELECT" },
    { "MUTE SETTING" },
    { "TONE SETTING" },
    { "ATTENUATOR" },
    { "BUZZER" },
    { "CREDIT" }
};

uint8_t setting_menu_slider = 0;
uint8_t input_select_slider = 0;
_Bool input_select_selected = 0;
uint8_t mute_setting_slider = 0;
_Bool mute_setting_selected = 0;
uint8_t tone_setting_slider = 0;
_Bool tone_setting_set_selected = 0;
uint8_t tone_setting_set_slider = 0;

typedef enum {
    TDA_EVT_CHECK,
    TDA_EVT_FULL_SYNC
} TDA_Event;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define SSD1306_Addr 0x3C

#define AT24C64_I2C        &hi2c2
#define AT24C64_ADDR       (0x50 << 1)   // A0-A2 = GND -> 0x50
#define AT24C64_PAGE_SIZE  32

#define EEPROM_MAGIC_ADDR   0x0000
#define EEPROM_DATA_ADDR    0x0004
#define EEPROM_MAGIC_VALUE  0xDEADBEEF

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define SW_ENC !HAL_GPIO_ReadPin(ENC_SW_GPIO_Port, ENC_SW_Pin)
#define BUZZER(x) HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, x)
#define constrain(x, a, b) ((x) < (a) ? (a) : ((x) > (b) ? (b) : (x)))

#define EV_BUZZER_BEEP (1 << 0)
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

TIM_HandleTypeDef htim1;

/* Definitions for inputTASK */
osThreadId_t inputTASKHandle;
const osThreadAttr_t inputTASK_attributes = {
  .name = "inputTASK",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for uiMgTASK */
osThreadId_t uiMgTASKHandle;
const osThreadAttr_t uiMgTASK_attributes = {
  .name = "uiMgTASK",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal1,
};
/* Definitions for displayTASK */
osThreadId_t displayTASKHandle;
const osThreadAttr_t displayTASK_attributes = {
  .name = "displayTASK",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for toneControlTASK */
osThreadId_t toneControlTASKHandle;
const osThreadAttr_t toneControlTASK_attributes = {
  .name = "toneControlTASK",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for eepromTASK */
osThreadId_t eepromTASKHandle;
const osThreadAttr_t eepromTASK_attributes = {
  .name = "eepromTASK",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityBelowNormal,
};
/* Definitions for buzzerTask */
osThreadId_t buzzerTaskHandle;
const osThreadAttr_t buzzerTask_attributes = {
  .name = "buzzerTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for uiTaskQueue */
osMessageQueueId_t uiTaskQueueHandle;
const osMessageQueueAttr_t uiTaskQueue_attributes = {
  .name = "uiTaskQueue"
};
/* Definitions for displayQueue */
osMessageQueueId_t displayQueueHandle;
const osMessageQueueAttr_t displayQueue_attributes = {
  .name = "displayQueue"
};
/* Definitions for tda7419Queue */
osMessageQueueId_t tda7419QueueHandle;
const osMessageQueueAttr_t tda7419Queue_attributes = {
  .name = "tda7419Queue"
};
/* Definitions for eepromQueue */
osMessageQueueId_t eepromQueueHandle;
const osMessageQueueAttr_t eepromQueue_attributes = {
  .name = "eepromQueue"
};
/* Definitions for i2cMutex */
osMutexId_t i2cMutexHandle;
const osMutexAttr_t i2cMutex_attributes = {
  .name = "i2cMutex"
};
/* Definitions for universalEvent */
osEventFlagsId_t universalEventHandle;
const osEventFlagsAttr_t universalEvent_attributes = {
  .name = "universalEvent"
};
/* USER CODE BEGIN PV */
extern I2C_HandleTypeDef hi2c1;

TDA7419_SA_Config saCfg = {
    .clkPort = GPIOB,
    .clkPin = GPIO_PIN_5,
    .hadc = &hadc1,
    .adcChannel = ADC_CHANNEL_1
};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_I2C2_Init(void);
static void MX_TIM1_Init(void);
static void MX_ADC1_Init(void);
void input_TASK(void *argument);
void uiManager_TASK(void *argument);
void U8g2_TASK(void *argument);
void TDA7419_TASK(void *argument);
void EEPROM_TASK(void *argument);
void BUZZER_TASK(void *argument);

/* USER CODE BEGIN PFP */
void MX_U8G2_Init(void);
void u8g2_DrawInt(uint16_t x, uint16_t y, int16_t num);
ButtonEvent_t check_button(void);
void format_gain(char *buf, size_t size, int value);

void draw_place_holder();

void draw_main_menu(void);
void draw_setting_menu(void);
void draw_tone_setting(void);

static void handle_long_press(void);
static void handle_short_press(void);
static void handle_encoder_ccw(void);
static void handle_encoder_cw(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_TIM1_Init();
  MX_ADC1_Init();
  /* USER CODE BEGIN 2 */
  MX_U8G2_Init();
  TDA7419_Init(&hi2c1);

  HAL_TIM_Encoder_Start(&htim1, TIM_CHANNEL_ALL);
  __HAL_TIM_SET_COUNTER(&htim1, 0x7FFF);

  main_menu_slider = 0;
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();
  /* Create the mutex(es) */
  /* creation of i2cMutex */
  i2cMutexHandle = osMutexNew(&i2cMutex_attributes);

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* creation of uiTaskQueue */
  uiTaskQueueHandle = osMessageQueueNew (16, sizeof(uint16_t), &uiTaskQueue_attributes);

  /* creation of displayQueue */
  displayQueueHandle = osMessageQueueNew (16, sizeof(uint16_t), &displayQueue_attributes);

  /* creation of tda7419Queue */
  tda7419QueueHandle = osMessageQueueNew (16, sizeof(uint16_t), &tda7419Queue_attributes);

  /* creation of eepromQueue */
  eepromQueueHandle = osMessageQueueNew (16, sizeof(uint16_t), &eepromQueue_attributes);

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  DisplayCmd_t dispCmd = MAIN_MENU;
  osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);

  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of inputTASK */
  inputTASKHandle = osThreadNew(input_TASK, NULL, &inputTASK_attributes);

  /* creation of uiMgTASK */
  uiMgTASKHandle = osThreadNew(uiManager_TASK, NULL, &uiMgTASK_attributes);

  /* creation of displayTASK */
  displayTASKHandle = osThreadNew(U8g2_TASK, NULL, &displayTASK_attributes);

  /* creation of toneControlTASK */
  toneControlTASKHandle = osThreadNew(TDA7419_TASK, NULL, &toneControlTASK_attributes);

  /* creation of eepromTASK */
  eepromTASKHandle = osThreadNew(EEPROM_TASK, NULL, &eepromTASK_attributes);

  /* creation of buzzerTask */
  buzzerTaskHandle = osThreadNew(BUZZER_TASK, NULL, &buzzerTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* creation of universalEvent */
  universalEventHandle = osEventFlagsNew(&universalEvent_attributes);

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.ClockSpeed = 400000;
  hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_Encoder_InitTypeDef sConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 65535;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
  sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC1Filter = 10;
  sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
  sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
  sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
  sConfig.IC2Filter = 0;
  if (HAL_TIM_Encoder_Init(&htim1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, BUZZER_Pin|MUTE_Pin|SACLK_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : SW1_Pin ENC_SW_Pin SW3_Pin */
  GPIO_InitStruct.Pin = SW1_Pin|ENC_SW_Pin|SW3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : SW2_Pin */
  GPIO_InitStruct.Pin = SW2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(SW2_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BUZZER_Pin MUTE_Pin SACLK_Pin */
  GPIO_InitStruct.Pin = BUZZER_Pin|MUTE_Pin|SACLK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void MX_U8G2_Init(void){
	u8g2_Setup_sh1106_i2c_128x64_noname_f(
			&u8g2,
			U8G2_R0,
			u8x8_byte_stm32_hw_i2c,
			u8x8_gpio_and_delay_stm32
		);
	u8g2_SetI2CAddress(&u8g2, 0x3C);
	u8g2_InitDisplay(&u8g2);
	u8g2_SetPowerSave(&u8g2, 0);
}

void u8g2_DrawInt(uint16_t x, uint16_t y, int16_t num){
	char buffer [16];
	sprintf(buffer, "%d", num);
	u8g2_DrawStr(&u8g2, x, y, buffer);
}

ButtonEvent_t check_button(void) {
    static uint8_t button_state = 0;
    static uint32_t press_time = 0;

    if (SW_ENC && button_state == 0) {
        button_state = 1;
        press_time = HAL_GetTick();
    }
    else if (!SW_ENC && button_state == 1) {
        uint32_t duration = HAL_GetTick() - press_time;
        button_state = 0;

        if (duration < 250) return BTN_SHORT;
        if (duration >= 250) return BTN_LONG;
    }

    return BTN_NONE;
}

void format_gain(char *buf, size_t size, int value) {
    if (value >= 0) {
        snprintf(buf, size, "+%02d", value);
    } else {
        snprintf(buf, size, "%03d", value);
    }
}

void draw_place_holder(){
    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFont(&u8g2, u8g2_font_profont17_tr);
    u8g2_DrawStr(&u8g2, 10, 38, "PLACE HOLDER");
}

void draw_main_menu(void){
	u8g2_SetDrawColor(&u8g2, 1);
	u8g2_SetFont(&u8g2, u8g2_font_profont11_tr);

	if (main_menu_slider > 0)
	    u8g2_DrawStr(&u8g2, 7, 19, main_menu_items[main_menu_slider - 1].label);
	if (main_menu_slider < (sizeof(main_menu_items)/sizeof(main_menu_items[0]) - 1))
	    u8g2_DrawStr(&u8g2, 7, 53, main_menu_items[main_menu_slider + 1].label);

    u8g2_SetFont(&u8g2, u8g2_font_profont17_tr);
    u8g2_DrawStr(&u8g2, 7, 38, main_menu_items[main_menu_slider].label);

    if(!MAIN_SELECTED){
        u8g2_SetDrawColor(&u8g2, 1);
        u8g2_DrawFrame(&u8g2, 5, 25, 57, 15);
    }else if(MAIN_SELECTED){
        u8g2_SetDrawColor(&u8g2, 2);
        u8g2_DrawBox(&u8g2, 5, 25, 57, 15);
    }

    // Draw value
    char value_str[6];
    format_gain(value_str, sizeof(value_str), *(main_menu_items[main_menu_slider].value));

    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFont(&u8g2, u8g2_font_profont29_tr);
    u8g2_DrawStr(&u8g2, 65, 42, value_str);

    u8g2_SetFont(&u8g2, u8g2_font_profont11_tr);
    u8g2_DrawStr(&u8g2, 114, 42, "dB");

    u8g2_DrawXBM(&u8g2, 48, 43, 14, 15, image_ArrowDownFilled_bits);
    u8g2_DrawXBM(&u8g2, 48, 7, 14, 15, image_ArrowUpFilled_bits);

    if(!settings.soft_mute){
  	  if(settings.master_volume == -79){
  		  u8g2_DrawXBM(&u8g2, 106, 2, 18, 16, image_volume_muted_bits);
  	  }else if(settings.master_volume <= -20){
  		  u8g2_DrawXBM(&u8g2, 106, 2, 18, 16, image_volume_low_bits);
  	  }else if(settings.master_volume > -20 && settings.master_volume <= 0){
  		  u8g2_DrawXBM(&u8g2, 106, 2, 18, 16, image_volume_normal_bits);
  	  }else if(settings.master_volume > 0){
  		  u8g2_DrawXBM(&u8g2, 106, 2, 20, 16, image_volume_loud_bits);
  	  }
    }else{
  	  u8g2_DrawXBM(&u8g2, 106, 2, 18, 16, image_volume_muted_bits);
  	  u8g2_DrawStr(&u8g2, 81, 13, "MUTE");
    }

    u8g2_DrawStr(&u8g2, 81, 59, input_select_items[settings.active_input - 1].label);
    u8g2_DrawFrame(&u8g2, 79, 50, 45, 11);
}

void draw_setting_menu(void){
	uint8_t x = 0;

    u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
    u8g2_DrawStr(&u8g2, 41, 10, "SETTING");
    u8g2_SetDrawColor(&u8g2, 2);
    u8g2_DrawBox(&u8g2, 0, 0, 128, 12);

    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_DrawXBM(&u8g2, 3, 30, 15, 14, image_ArrowLeftFilled_bits);
    u8g2_DrawXBM(&u8g2, 110, 30, 15, 14, image_ArrowRightFilled_bits);

    switch(setting_menu_slider){
    case 0:
    	u8g2_DrawXBM(&u8g2, 49, 21, 30, 30, image_input_select_bits);
    	x = 32;
    	break;
    case 1:
    	u8g2_DrawXBM(&u8g2, 49, 21, 30, 30, image_mute_setting_bits);
    	x = 32;
    	break;
    case 2:
    	u8g2_DrawXBM(&u8g2, 49, 21, 30, 30, image_equalizer_bits);
    	x = 32;
    	break;
    case 3:
    	u8g2_DrawXBM(&u8g2, 49, 21, 30, 30, image_attenuator_bits);
    	x = 34;
    	break;
    case 4:
    	u8g2_DrawXBM(&u8g2, 49, 21, 30, 30, image_buzzer_bits);
    	x = 46;
    	break;
    case 5:
    	u8g2_DrawXBM(&u8g2, 49, 21, 30, 30, image_credits_bits);
    	x = 48;
    	break;
    }
    u8g2_SetFont(&u8g2, u8g2_font_haxrcorp4089_tr);
    u8g2_DrawStr(&u8g2, x, 61, setting_items[setting_menu_slider].label);
}

void draw_input_select(void){
	u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
    u8g2_DrawStr(&u8g2, 27, 10, "INPUT SELECT");
    u8g2_SetFont(&u8g2, u8g2_font_haxrcorp4089_tr);
    u8g2_DrawStr(&u8g2, 5, 24, "INPUT:");
    u8g2_DrawStr(&u8g2, 44, 24, input_select_items[settings.active_input - 1].label);

    u8g2_DrawStr(&u8g2, 5, 42, input_select_items[settings.active_input - 1].in_gain_label);
    u8g2_DrawInt(44, 42, *input_select_items[settings.active_input - 1].gain);

    u8g2_DrawStr(&u8g2, 5, 60, "Auto-Z:");
    if(!settings.auto_z) u8g2_DrawStr(&u8g2, 44, 60, "ON"); else u8g2_DrawStr(&u8g2, 44, 60, "OFF");

    if(input_select_selected){
    	u8g2_SetDrawColor(&u8g2, 2);
    	switch(input_select_slider){
    	case 0: u8g2_DrawBox(&u8g2, 0, 15, 128, 11); break;
    	case 1: u8g2_DrawBox(&u8g2, 0, 33, 128, 11); break;
    	case 2: u8g2_DrawBox(&u8g2, 0, 51, 128, 11); break;
    	}
    }else if(!input_select_selected){
    	u8g2_SetDrawColor(&u8g2, 1);
    	switch(input_select_slider){
    	case 0: u8g2_DrawFrame(&u8g2, 0, 15, 128, 11); break;
    	case 1: u8g2_DrawFrame(&u8g2, 0, 33, 128, 11); break;
    	case 2: u8g2_DrawFrame(&u8g2, 0, 51, 128, 11); break;
    	}
    }
}

void draw_mute_setting(void){

	const char* softStepTimeStr[] = {
	  "0.160ms",
	  "0.321ms",
	  "0.642ms",
	  "1.28ms",
	  "2.56ms",
	  "5.12ms",
	  "10.24ms",
	  "20.48ms"
	};

	const char* softMuteTimeStr[] = {
	  "0.480ms",
	  "0.960ms",
	  "123ms"
	};

	u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
    u8g2_DrawStr(&u8g2, 25, 10, "MUTE SETTING");
    u8g2_SetFont(&u8g2, u8g2_font_haxrcorp4089_tr);
    u8g2_DrawStr(&u8g2, 5, 24, "SOFTMUTE TIME:");
    u8g2_DrawStr(&u8g2, 84, 24, softMuteTimeStr[settings.soft_mute_time]);

    u8g2_DrawStr(&u8g2, 5, 42, "SOFTSTEP TIME:");
    u8g2_DrawStr(&u8g2, 84, 42, softStepTimeStr[settings.soft_step_time]);

    u8g2_DrawStr(&u8g2, 5, 60, "MODE:");
    if(settings.mute_mode) u8g2_DrawStr(&u8g2, 34, 60, "I2C ONLY"); else u8g2_DrawStr(&u8g2, 34, 60, "I2C & PIN");

    if(mute_setting_selected){
    	u8g2_SetDrawColor(&u8g2, 2);
    	switch(mute_setting_slider){
    	case 0: u8g2_DrawBox(&u8g2, 0, 15, 128, 11); break;
    	case 1: u8g2_DrawBox(&u8g2, 0, 33, 128, 11); break;
    	case 2: u8g2_DrawBox(&u8g2, 0, 51, 128, 11); break;
    	}
    }else if(!mute_setting_selected){
    	u8g2_SetDrawColor(&u8g2, 1);
    	switch(mute_setting_slider){
    	case 0: u8g2_DrawFrame(&u8g2, 0, 15, 128, 11); break;
    	case 1: u8g2_DrawFrame(&u8g2, 0, 33, 128, 11); break;
    	case 2: u8g2_DrawFrame(&u8g2, 0, 51, 128, 11); break;
    	}
    }
}

void draw_tone_setting(void){
    u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
    u8g2_DrawStr(&u8g2, 25, 10, "TONE SETTING");
    u8g2_SetDrawColor(&u8g2, 2);
    u8g2_DrawBox(&u8g2, 0, 0, 128, 12);

    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_DrawXBM(&u8g2, 3, 30, 15, 14, image_ArrowLeftFilled_bits);
    u8g2_DrawXBM(&u8g2, 110, 30, 15, 14, image_ArrowRightFilled_bits);

    u8g2_SetFont(&u8g2, u8g2_font_profont29_tr);
    switch(tone_setting_slider){
    case 0: u8g2_DrawStr(&u8g2, 32, 46, "HIGH"); break;
    case 1: u8g2_DrawStr(&u8g2, 41, 46, "MID"); break;
    case 2: u8g2_DrawStr(&u8g2, 32, 46, "BASS"); break;
    case 3: u8g2_DrawStr(&u8g2, 41, 46, "SUB"); break;
    case 4: u8g2_SetFont(&u8g2, u8g2_font_profont17_tr); u8g2_DrawStr(&u8g2, 28, 42, "LOUDNESS"); break;
    }
}

void draw_tone_setting_set(DisplayCmd_t cmd) {

	static const char* trebleFreq[]  = {"10.0kHz","12.5kHz","15.0kHz","17.5kHz"};
	static const char* midFreq[]     = {"500Hz","1000Hz","1500Hz","2500Hz"};
	static const char* midQ[]        = {"0.5","0.75","1","1.25"};
	static const char* bassFreq[]    = {"60Hz","80Hz","100Hz","200Hz"};
	static const char* bassQ[]       = {"1","1.25","1.5","2.0"};
	static const char* subCut[]      = {"FLAT","80Hz","120Hz","160Hz"};
	static const char* loudnessFreq[]= {"FLAT","400Hz","800Hz","2400Hz"};

	static uint8_t maxSlider = 0;

    u8g2_SetDrawColor(&u8g2, 1);

    switch(cmd) {
    case TONE_SETTING_HIGH:
    	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
        u8g2_DrawStr(&u8g2, 52, 10, "HIGH");
        u8g2_SetFont(&u8g2, u8g2_font_haxrcorp4089_tr);
        u8g2_DrawStr(&u8g2, 5, 24, "CENTER FREQ:");
        u8g2_DrawStr(&u8g2, 84, 24, trebleFreq[settings.treble_c_freq]);
        maxSlider = 0; // only one option
        break;

    case TONE_SETTING_MID:
    	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
        u8g2_DrawStr(&u8g2, 45, 10, "MIDDLE");
        u8g2_SetFont(&u8g2, u8g2_font_haxrcorp4089_tr);
        u8g2_DrawStr(&u8g2, 5, 24, "CENTER FREQ:");
        u8g2_DrawStr(&u8g2, 84, 24, midFreq[settings.middle_c_freq]);
        u8g2_DrawStr(&u8g2, 5, 42, "Q Factor:");
        u8g2_DrawStr(&u8g2, 84, 42, midQ[settings.middle_q_factor]);
        maxSlider = 1;
        break;

    case TONE_SETTING_BASS:
    	u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
        u8g2_DrawStr(&u8g2, 50, 10, "BASS");
        u8g2_SetFont(&u8g2, u8g2_font_haxrcorp4089_tr);
        u8g2_DrawStr(&u8g2, 5, 24, "CENTER FREQ:");
        u8g2_DrawStr(&u8g2, 84, 24, bassFreq[settings.bass_c_freq]);
        u8g2_DrawStr(&u8g2, 5, 42, "Q Factor:");
        u8g2_DrawStr(&u8g2, 84, 42, bassQ[settings.bass_q_factor]);
        u8g2_DrawStr(&u8g2, 5, 60, "BASS DC:");
        u8g2_DrawStr(&u8g2, 84, 60, settings.bass_dc ? "OFF":"ON");
        maxSlider = 2;
        break;

    case TONE_SETTING_SUB:
		u8g2_SetDrawColor(&u8g2, 1);
	    u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
	    u8g2_DrawStr(&u8g2, 54, 10, "SUB");

	    u8g2_SetFont(&u8g2, u8g2_font_haxrcorp4089_tr);
	    u8g2_DrawStr(&u8g2, 5, 24, "CUT OFF FREQ:");
	    u8g2_DrawStr(&u8g2, 84, 24, subCut[settings.sub_cut_off]);
	    maxSlider = 0;
    	break;

    case TONE_SETTING_LOUDNESS:
		u8g2_SetDrawColor(&u8g2, 1);
	    u8g2_SetFont(&u8g2, u8g2_font_helvB08_tr);
	    u8g2_DrawStr(&u8g2, 36, 10, "LOUDNESS");

	    u8g2_SetFont(&u8g2, u8g2_font_haxrcorp4089_tr);
	    u8g2_DrawStr(&u8g2, 5, 24, "CENTER FREQ:");
	    u8g2_DrawStr(&u8g2, 84, 24, loudnessFreq[settings.loudness_c_freq]);

	    u8g2_DrawStr(&u8g2, 5, 42, "HIGH BOOST:");
	    if(settings.high_boost) u8g2_DrawStr(&u8g2, 84, 42, "OFF"); else u8g2_DrawStr(&u8g2, 84, 42, "ON");
	    maxSlider = 1;
    	break;
    }

    if (tone_setting_set_slider > maxSlider) tone_setting_set_slider = maxSlider;

    if (tone_setting_set_selected) {
        u8g2_SetDrawColor(&u8g2, 2);
        u8g2_DrawBox(&u8g2, 0, 15 + 18*tone_setting_set_slider, 128, 11);
    } else {
        u8g2_SetDrawColor(&u8g2, 1);
        u8g2_DrawFrame(&u8g2, 0, 15 + 18*tone_setting_set_slider, 128, 11);
    }
}

void draw_attenuator_menu(void){
	u8g2_SetDrawColor(&u8g2, 1);
	u8g2_SetFont(&u8g2, u8g2_font_profont11_tr);

	if (attenuator_menu_slider > 0)
	    u8g2_DrawStr(&u8g2, 7, 19, attenuator_menu_items[attenuator_menu_slider - 1].label);
	if (attenuator_menu_slider < (sizeof(attenuator_menu_items)/sizeof(attenuator_menu_items[0]) - 1))
	    u8g2_DrawStr(&u8g2, 7, 53, attenuator_menu_items[attenuator_menu_slider + 1].label);

    u8g2_SetFont(&u8g2, u8g2_font_profont17_tr);
    u8g2_DrawStr(&u8g2, 7, 38, attenuator_menu_items[attenuator_menu_slider].label);

    if(!attenuator_selected){
        u8g2_SetDrawColor(&u8g2, 1);
        u8g2_DrawFrame(&u8g2, 5, 25, 57, 15);
    }else if(attenuator_selected){
        u8g2_SetDrawColor(&u8g2, 2);
        u8g2_DrawBox(&u8g2, 5, 25, 57, 15);
    }

    // Draw value
    char value_str[6];
    format_gain(value_str, sizeof(value_str), *(attenuator_menu_items[attenuator_menu_slider].value));

    u8g2_SetDrawColor(&u8g2, 1);
    u8g2_SetFont(&u8g2, u8g2_font_profont29_tr);
    u8g2_DrawStr(&u8g2, 65, 42, value_str);

    u8g2_SetFont(&u8g2, u8g2_font_profont11_tr);
    u8g2_DrawStr(&u8g2, 114, 42, "dB");

    u8g2_DrawXBM(&u8g2, 48, 43, 14, 15, image_ArrowDownFilled_bits);
    u8g2_DrawXBM(&u8g2, 48, 7, 14, 15, image_ArrowUpFilled_bits);
}

void draw_buzzer_setting(void){
    u8g2_DrawXBM(&u8g2, 1, -11, 30, 30, image_buzzer_bits);
    u8g2_SetFont(&u8g2, u8g2_font_profont22_tr);
    if(settings.buzzer_state) u8g2_DrawStr(&u8g2, 88, 39, "ON"); else u8g2_DrawStr(&u8g2, 88, 39, "OFF");

    u8g2_DrawStr(&u8g2, 4, 39, "BUZZER:");
    u8g2_DrawXBM(&u8g2, 50, 5, 29, 14, image_FaceConfused_bits);
    u8g2_DrawXBM(&u8g2, 0, 50, 128, 14, image_Unplug_bg_top_bits);
}

void draw_eeprom_loading(void) {
    u8g2_SetFont(&u8g2, u8g2_font_t0_16b_tr);
    u8g2_DrawStr(&u8g2, 1, 38, "EEPROM LOADING..");

    u8g2_DrawXBM(&u8g2, 50, 5, 29, 14, image_FaceConfused_bits);
}

//eeprom func

static void EEPROM_WaitReady(void) {
    while (HAL_I2C_IsDeviceReady(AT24C64_I2C, AT24C64_ADDR, 1, 10) != HAL_OK) {
        osDelay(1);
    }
}

HAL_StatusTypeDef EEPROM_WriteBlock(uint16_t memAddr, uint8_t *data, uint16_t len) {
    HAL_StatusTypeDef status = HAL_OK;

    while (len > 0) {
        uint16_t pageOffset = memAddr % AT24C64_PAGE_SIZE;
        uint16_t bytesInPage = AT24C64_PAGE_SIZE - pageOffset;
        uint16_t chunkSize = (len < bytesInPage) ? len : bytesInPage;

        uint8_t buf[2 + AT24C64_PAGE_SIZE];
        buf[0] = (uint8_t)(memAddr >> 8);
        buf[1] = (uint8_t)(memAddr & 0xFF);
        memcpy(&buf[2], data, chunkSize);

        status = HAL_I2C_Master_Transmit(AT24C64_I2C, AT24C64_ADDR, buf, 2 + chunkSize, HAL_MAX_DELAY);
        if (status != HAL_OK) return status;

        EEPROM_WaitReady();

        memAddr += chunkSize;
        data    += chunkSize;
        len     -= chunkSize;
    }

    return status;
}

HAL_StatusTypeDef EEPROM_ReadBlock(uint16_t memAddr, uint8_t *data, uint16_t len) {
    uint8_t addr[2];
    addr[0] = (uint8_t)(memAddr >> 8);
    addr[1] = (uint8_t)(memAddr & 0xFF);

    HAL_StatusTypeDef status;
    status = HAL_I2C_Master_Transmit(AT24C64_I2C, AT24C64_ADDR, addr, 2, HAL_MAX_DELAY);
    if (status != HAL_OK) return status;

    return HAL_I2C_Master_Receive(AT24C64_I2C, AT24C64_ADDR, data, len, HAL_MAX_DELAY);
}

void EEPROM_SaveSettings(void) {
    osMutexAcquire(i2cMutexHandle, osWaitForever);

    uint32_t magic = EEPROM_MAGIC_VALUE;
    EEPROM_WriteBlock(EEPROM_MAGIC_ADDR, (uint8_t*)&magic, sizeof(magic));
    EEPROM_WriteBlock(EEPROM_DATA_ADDR, (uint8_t*)&settings, sizeof(UserSettings_t));

    osMutexRelease(i2cMutexHandle);
}

void EEPROM_LoadSettings(void) {
    osMutexAcquire(i2cMutexHandle, osWaitForever);

    uint32_t magic = 0;
    EEPROM_ReadBlock(EEPROM_MAGIC_ADDR, (uint8_t*)&magic, sizeof(magic));
    if (magic == EEPROM_MAGIC_VALUE) {
        EEPROM_ReadBlock(EEPROM_DATA_ADDR, (uint8_t*)&settings, sizeof(UserSettings_t));
    }
    // else: keep defaults

    osMutexRelease(i2cMutexHandle);
}

//TDA7419 FUNCTION

static uint8_t get_input_gain(UserSettings_t *s){
	switch(s->active_input){
	case 0: return s->in1_gain;
	case 1: return s->in2_gain;
	case 2: return s->in3_gain;
	default: return 0;
	}
}

void TDA7419_InitFromSettings(UserSettings_t *s) {
    TDA7419_SetInput(s->active_input, get_input_gain(&settings), s->auto_z);
    TDA7419_SetVolume(s->master_volume, 1);
    TDA7419_SetTreble(s->treble_tone_gain, s->treble_c_freq, 1);
    TDA7419_SetMiddle(s->middle_tone_gain, s->middle_q_factor, 1);
    TDA7419_SetBass(s->bass_tone_gain, s->bass_q_factor, 1);
    TDA7419_SetLoudness(s->loudness_att, s->loudness_c_freq, s->high_boost, 0);
    TDA7419_SoftMuteConfig(s->soft_mute, s->mute_mode, s->soft_mute_time, s->soft_step_time, 0);
    TDA7419_SetCenterSMB(s->sub_cut_off, s->middle_c_freq, s->bass_c_freq, s->bass_dc, 0);

    TDA7419_SetAttLF(s->left_att, 0);
    TDA7419_SetAttRF(s->right_att, 0);
    TDA7419_SetAttLR(s->s_left_att, 0);
    TDA7419_SetAttRR(s->s_right_att, 0);
    TDA7419_SetAttSub(s->sub_att, 0);
}


//helper function

static void handle_short_press(void) {
    DisplayCmd_t dispCmd;

    if (current_menu == MAIN_MENU) {
        MAIN_SELECTED = !MAIN_SELECTED;
        dispCmd = MAIN_MENU;
        osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
        return;
    }

    if (current_menu == SETTING_MENU) {
        DisplayCmd_t target = (DisplayCmd_t)(setting_menu_slider + 2);
        current_menu = target;
        dispCmd = target;
        osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
        return;
    }

    if(current_menu == INPUT_SELECT){
    	input_select_selected = !input_select_selected;
        dispCmd = INPUT_SELECT;
        osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
        return;
    }

    if(current_menu == MUTE_SETTING){
    	mute_setting_selected = !mute_setting_selected;
        dispCmd = MUTE_SETTING;
        osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
        return;
    }

    if(current_menu == TONE_SETTING){
    	DisplayCmd_t target = (DisplayCmd_t)(tone_setting_slider + 8);
    	current_menu = target;
    	dispCmd = target;
    	osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
    	return;
    }

	if (current_menu == TONE_SETTING_HIGH ||
	    current_menu == TONE_SETTING_MID ||
	    current_menu == TONE_SETTING_BASS ||
	    current_menu == TONE_SETTING_SUB ||
	    current_menu == TONE_SETTING_LOUDNESS){
		tone_setting_set_selected = !tone_setting_set_selected;
	    dispCmd = current_menu;
	    osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
	    return;
	}

	if(current_menu == ATTENUATOR){
		attenuator_selected = !attenuator_selected;
	    dispCmd = ATTENUATOR;
	    osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
	    return;
	}

	 if(current_menu == BUZZER){
		 settings.buzzer_state = !settings.buzzer_state;
		    dispCmd = BUZZER;
		    osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
		    return;
	 }
}

static void handle_long_press(void){
	DisplayCmd_t dispCmd;

	if(current_menu == MAIN_MENU){
		current_menu = SETTING_MENU;
		dispCmd = SETTING_MENU;
		osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
		return;
	}

	if(current_menu == SETTING_MENU){
		current_menu = MAIN_MENU;
		dispCmd = MAIN_MENU;
		osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
		return;
	}

	if (current_menu == INPUT_SELECT ||
	    current_menu == MUTE_SETTING ||
	    current_menu == TONE_SETTING ||
	    current_menu == ATTENUATOR ||
	    current_menu == BUZZER ||
	    current_menu == CREDIT) {
		current_menu = SETTING_MENU;
		dispCmd = SETTING_MENU;
		osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
		return;
	}

	if (current_menu == TONE_SETTING_HIGH ||
	    current_menu == TONE_SETTING_MID ||
	    current_menu == TONE_SETTING_BASS ||
	    current_menu == TONE_SETTING_SUB ||
	    current_menu == TONE_SETTING_LOUDNESS){
		tone_setting_set_selected = 0;
	    current_menu = TONE_SETTING;
	    dispCmd = TONE_SETTING;
	    osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
	    return;
	}
}

static void handle_encoder_cw(void) {
    DisplayCmd_t dispCmd;

    if (current_menu == MAIN_MENU && !MAIN_SELECTED) {
        main_menu_slider = (main_menu_slider < 3) ? main_menu_slider + 1 : 3;
        dispCmd = MAIN_MENU;
        osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
        return;
    }

    if (current_menu == MAIN_MENU && MAIN_SELECTED) {
        switch (main_menu_slider) {
        case 0: settings.master_volume    = (settings.master_volume    <  15) ? settings.master_volume    + 1 :  15; break;
        case 1: settings.treble_tone_gain = (settings.treble_tone_gain <  15) ? settings.treble_tone_gain + 1 :  15; break;
        case 2: settings.middle_tone_gain = (settings.middle_tone_gain <  15) ? settings.middle_tone_gain + 1 :  15; break;
        case 3: settings.bass_tone_gain   = (settings.bass_tone_gain   <  15) ? settings.bass_tone_gain   + 1 :  15; break;
        }
        dispCmd = MAIN_MENU;
        osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
        return;
    }

    if (current_menu == SETTING_MENU) {
        setting_menu_slider = (setting_menu_slider < 5) ? setting_menu_slider + 1 : 5;
        dispCmd = SETTING_MENU;
        osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
        return;
    }

    if(current_menu == INPUT_SELECT && !input_select_selected){
    	input_select_slider = (input_select_slider < 2) ? input_select_slider + 1 : 2;
    	dispCmd = INPUT_SELECT;
        osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
        return;
    }

    if(current_menu == INPUT_SELECT && input_select_selected){
    	switch(input_select_slider){
    	case 0: settings.active_input = (settings.active_input < 3) ? settings.active_input + 1 : 3; break;
    	case 1:
    		switch(settings.active_input){
    		case 1: settings.in1_gain = (settings.in1_gain < 15) ? settings.in1_gain + 1 : 15; break;
    		case 2: settings.in2_gain = (settings.in2_gain < 15) ? settings.in2_gain + 1 : 15; break;
    		case 3: settings.in3_gain = (settings.in3_gain < 15) ? settings.in3_gain + 1 : 15; break;
    		}
    		break;
    	case 2: settings.auto_z = 0; break;
    	}
    	dispCmd = INPUT_SELECT;
        osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
        return;
    }

    if(current_menu == MUTE_SETTING && !mute_setting_selected){
    	mute_setting_slider = (mute_setting_slider < 2) ? mute_setting_slider + 1 : 2;
    	dispCmd = MUTE_SETTING;
        osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
        return;
    }

    if(current_menu == MUTE_SETTING && mute_setting_selected){
    	switch(mute_setting_slider){
    	case 0: settings.soft_mute_time = (settings.soft_mute_time < 2) ? settings.soft_mute_time + 1 : 2; break;
    	case 1: settings.soft_step_time = (settings.soft_step_time < 7) ? settings.soft_step_time + 1 : 7; break;
    	case 2: settings.mute_mode = 1; break;
    	}
    	dispCmd = MUTE_SETTING;
    	osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
    	return;
    }

    if(current_menu == TONE_SETTING){
    	tone_setting_slider = (tone_setting_slider < 4) ? tone_setting_slider + 1 : 4;
    	dispCmd = TONE_SETTING;
    	osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
    	return;
    }

	if ((current_menu == TONE_SETTING_HIGH ||
	    current_menu == TONE_SETTING_MID ||
	    current_menu == TONE_SETTING_BASS ||
	    current_menu == TONE_SETTING_SUB ||
	    current_menu == TONE_SETTING_LOUDNESS) && !tone_setting_set_selected){
	    tone_setting_set_slider = (tone_setting_set_slider < 2) ? tone_setting_set_slider + 1 : 2;
	    dispCmd = current_menu;
	    osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
	    return;
	}

	if ((current_menu == TONE_SETTING_HIGH ||
	    current_menu == TONE_SETTING_MID ||
	    current_menu == TONE_SETTING_BASS ||
	    current_menu == TONE_SETTING_SUB ||
	    current_menu == TONE_SETTING_LOUDNESS) && tone_setting_set_selected){

		switch(current_menu){
		case TONE_SETTING_HIGH: settings.treble_c_freq = (settings.treble_c_freq < 3) ? settings.treble_c_freq + 1 : 3; break;
		case TONE_SETTING_MID:
			if(tone_setting_set_slider == 0) settings.middle_c_freq = (settings.middle_c_freq < 3) ? settings.middle_c_freq + 1 : 3;
			if(tone_setting_set_slider == 1) settings.middle_q_factor = (settings.middle_q_factor < 3) ? settings.middle_q_factor + 1 : 3;
			break;
		case TONE_SETTING_BASS:
			if(tone_setting_set_slider == 0) settings.bass_c_freq = (settings.bass_c_freq < 3) ? settings.bass_c_freq + 1 : 3;
			if(tone_setting_set_slider == 1) settings.bass_q_factor = (settings.bass_q_factor < 3) ? settings.bass_q_factor + 1 : 3;
			if(tone_setting_set_slider == 2) settings.bass_dc = 0;
			break;
		case TONE_SETTING_SUB: settings.sub_cut_off = (settings.sub_cut_off < 3) ? settings.sub_cut_off + 1 : 3; break;
		case TONE_SETTING_LOUDNESS:
			if(tone_setting_set_slider == 0) settings.loudness_c_freq = (settings.loudness_c_freq < 3) ? settings.loudness_c_freq + 1 : 3;
			if(tone_setting_set_slider == 1) settings.high_boost = 0;
			break;
		}

	    dispCmd = current_menu;
	    osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
	    return;
	}

	if(current_menu == ATTENUATOR && !attenuator_selected){
        attenuator_menu_slider = (attenuator_menu_slider < 4) ? attenuator_menu_slider + 1 : 4;
        dispCmd = ATTENUATOR;
        osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
        return;
	}

	if(current_menu == ATTENUATOR && attenuator_selected){
		switch(attenuator_menu_slider){
		case 0: settings.left_att = (settings.left_att < 15) ? settings.left_att + 1 : 15; break;
		case 1: settings.right_att = (settings.right_att < 15) ? settings.right_att + 1 : 15; break;
		case 2: settings.s_left_att = (settings.s_left_att < 15) ? settings.s_left_att + 1 : 15; break;
		case 3: settings.s_right_att = (settings.s_right_att < 15) ? settings.s_right_att + 1 : 15; break;
		case 4: settings.sub_att = (settings.sub_att < 15) ? settings.sub_att + 1 : 15; break;
		}
		dispCmd = ATTENUATOR;
		osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
		return;
	}
}

static void handle_encoder_ccw(void){
	DisplayCmd_t dispCmd;

    if (current_menu == MAIN_MENU && !MAIN_SELECTED) {
        main_menu_slider = (main_menu_slider > 0) ? main_menu_slider - 1 : 0;
        dispCmd = MAIN_MENU;
        osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
        return;
    }

    if (current_menu == MAIN_MENU && MAIN_SELECTED) {
        switch (main_menu_slider) {
        case 0: settings.master_volume    = (settings.master_volume    >  -79) ? settings.master_volume    - 1 : -79; break;
        case 1: settings.treble_tone_gain = (settings.treble_tone_gain >  -15) ? settings.treble_tone_gain - 1 : -15; break;
        case 2: settings.middle_tone_gain = (settings.middle_tone_gain >  -15) ? settings.middle_tone_gain - 1 : -15; break;
        case 3: settings.bass_tone_gain   = (settings.bass_tone_gain   >  -15) ? settings.bass_tone_gain   - 1 : -15; break;
        }
        dispCmd = MAIN_MENU;
        osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
        return;
    }

    if (current_menu == SETTING_MENU) {
        setting_menu_slider = (setting_menu_slider > 0) ? setting_menu_slider - 1 : 0;
        dispCmd = SETTING_MENU;
        osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
        return;
    }

    if(current_menu == INPUT_SELECT && !input_select_selected){
    	input_select_slider = (input_select_slider > 0) ? input_select_slider - 1 : 0;
    	dispCmd = INPUT_SELECT;
        osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
        return;
    }

    if(current_menu == INPUT_SELECT && input_select_selected){
    	switch(input_select_slider){
    	case 0: settings.active_input = (settings.active_input > 1) ? settings.active_input - 1 : 1; break;
    	case 1:
    		switch(settings.active_input){
    		case 1: settings.in1_gain = (settings.in1_gain > 0) ? settings.in1_gain - 1 : 0; break;
    		case 2: settings.in2_gain = (settings.in2_gain > 0) ? settings.in2_gain - 1 : 0; break;
    		case 3: settings.in3_gain = (settings.in3_gain > 0) ? settings.in3_gain - 1 : 0; break;
    		}
    		break;
    	case 2: settings.auto_z = 1; break;
    	}
    	dispCmd = INPUT_SELECT;
        osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
        return;
    }

    if(current_menu == MUTE_SETTING && !mute_setting_selected){
    	mute_setting_slider = (mute_setting_slider > 0) ? mute_setting_slider - 1 : 0;
    	dispCmd = MUTE_SETTING;
        osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
        return;
    }

    if(current_menu == MUTE_SETTING && mute_setting_selected){
    	switch(mute_setting_slider){
    	case 0: settings.soft_mute_time = (settings.soft_mute_time > 0) ? settings.soft_mute_time - 1 : 0; break;
    	case 1: settings.soft_step_time = (settings.soft_step_time > 0) ? settings.soft_step_time - 1 : 0; break;
    	case 2: settings.mute_mode = 0; break;
    	}
    	dispCmd = MUTE_SETTING;
    	osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
    	return;
    }

    if(current_menu == TONE_SETTING){
    	tone_setting_slider = (tone_setting_slider > 0) ? tone_setting_slider - 1 : 0;
    	dispCmd = TONE_SETTING;
    	osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
    	return;
    }

	if ((current_menu == TONE_SETTING_HIGH ||
	    current_menu == TONE_SETTING_MID ||
	    current_menu == TONE_SETTING_BASS ||
	    current_menu == TONE_SETTING_SUB ||
	    current_menu == TONE_SETTING_LOUDNESS) && !tone_setting_set_selected){
	    tone_setting_set_slider = (tone_setting_set_slider > 0) ? tone_setting_set_slider - 1 : 0;
	    dispCmd = current_menu;
	    osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
	    return;
	}

	if ((current_menu == TONE_SETTING_HIGH ||
	    current_menu == TONE_SETTING_MID ||
	    current_menu == TONE_SETTING_BASS ||
	    current_menu == TONE_SETTING_SUB ||
	    current_menu == TONE_SETTING_LOUDNESS) && tone_setting_set_selected){

		switch(current_menu){
		case TONE_SETTING_HIGH: settings.treble_c_freq = (settings.treble_c_freq > 0) ? settings.treble_c_freq - 1 : 0; break;
		case TONE_SETTING_MID:
			if(tone_setting_set_slider == 0) settings.middle_c_freq = (settings.middle_c_freq > 0) ? settings.middle_c_freq - 1 : 0;
			if(tone_setting_set_slider == 1) settings.middle_q_factor = (settings.middle_q_factor > 0) ? settings.middle_q_factor - 1 : 0;
			break;
		case TONE_SETTING_BASS:
			if(tone_setting_set_slider == 0) settings.bass_c_freq = (settings.bass_c_freq > 0) ? settings.bass_c_freq - 1 : 0;
			if(tone_setting_set_slider == 1) settings.bass_q_factor = (settings.bass_q_factor > 0) ? settings.bass_q_factor - 1 : 0;
			if(tone_setting_set_slider == 2) settings.bass_dc = 1;
			break;
		case TONE_SETTING_SUB: settings.sub_cut_off = (settings.sub_cut_off > 0) ? settings.sub_cut_off - 1 : 0; break;
		case TONE_SETTING_LOUDNESS:
			if(tone_setting_set_slider == 0) settings.loudness_c_freq = (settings.loudness_c_freq > 0) ? settings.loudness_c_freq - 1 : 0;
			if(tone_setting_set_slider == 1) settings.high_boost = 1;
			break;
		}

	    dispCmd = current_menu;
	    osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
	    return;
	}

	if(current_menu == ATTENUATOR && !attenuator_selected){
        attenuator_menu_slider = (attenuator_menu_slider > 0) ? attenuator_menu_slider - 1 : 0;
        dispCmd = ATTENUATOR;
        osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
        return;
	}

	if(current_menu == ATTENUATOR && attenuator_selected){
		switch(attenuator_menu_slider){
		case 0: settings.left_att = (settings.left_att > -79) ? settings.left_att - 1 : -79; break;
		case 1: settings.right_att = (settings.right_att > -79) ? settings.right_att - 1 : -79; break;
		case 2: settings.s_left_att = (settings.s_left_att > -79) ? settings.s_left_att - 1 : -79; break;
		case 3: settings.s_right_att = (settings.s_right_att > -79) ? settings.s_right_att - 1 : -79; break;
		case 4: settings.sub_att = (settings.sub_att > -79) ? settings.sub_att - 1 : -79; break;
		}
		dispCmd = ATTENUATOR;
		osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);
		return;
	}
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_input_TASK */
/**
  * @brief  Function implementing the inputTASK thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_input_TASK */
void input_TASK(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
    static int16_t prev_count = 0X7FFF;
    static int16_t acc = 0;
    EventType_t evt;

  for(;;)
  {
      int16_t curr_count = __HAL_TIM_GET_COUNTER(&htim1);
      int16_t delta = (int16_t)(curr_count - prev_count);
      prev_count = curr_count;

      if ((acc > 0 && delta < 0) || (acc < 0 && delta > 0)) {
          acc = 0;
      }

      acc += delta;

      while (acc >= 2) {
          evt = EV_ENCODER_CW;
          osMessageQueuePut(uiTaskQueueHandle, &evt, 0, 0);
          acc -= 2;
      }

      while (acc <= -2) {
          evt = EV_ENCODER_CCW;
          osMessageQueuePut(uiTaskQueueHandle, &evt, 0, 0);
          acc += 2;
      }

	  ButtonEvent_t ev = check_button();

	  if(ev == BTN_SHORT){
		  evt = EV_ENCODER_SW_SHORT;
		  osMessageQueuePut(uiTaskQueueHandle, &evt, 0, 0);
	  }

	  if(ev == BTN_LONG){
		  evt = EV_ENCODER_SW_LONG;
		  osMessageQueuePut(uiTaskQueueHandle, &evt, 0, 0);
	  }

	  osDelay(5);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_uiManager_TASK */
/**
* @brief Function implementing the uiMgTASK thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_uiManager_TASK */
void uiManager_TASK(void *argument)
{
  /* USER CODE BEGIN uiManager_TASK */
  /* Infinite loop */
  TDA_Event tdaEvt;
  EventType_t evt;

  for(;;)
  {
	  osMessageQueueGet(uiTaskQueueHandle, &evt, NULL, osWaitForever);

	  if(settings.buzzer_state){
		  osEventFlagsSet(universalEventHandle, EV_BUZZER_BEEP);
	  }

	  switch(evt){
	  case EV_ENCODER_SW_SHORT: handle_short_press(); break;
	  case EV_ENCODER_SW_LONG:  handle_long_press();  break;
	  case EV_ENCODER_CW:       handle_encoder_cw();  break;
	  case EV_ENCODER_CCW:      handle_encoder_ccw(); break;
	  case EV_BTN1: /*------implement later-------*/  break;
	  case EV_BTN2: /*------implement later-------*/  break;
	  case EV_BTN3: /*------implement later-------*/  break;
	  }

	  tdaEvt = TDA_EVT_CHECK;
	  osMessageQueuePut(tda7419QueueHandle, &tdaEvt, 0, 0);
  }
  /* USER CODE END uiManager_TASK */
}

/* USER CODE BEGIN Header_U8g2_TASK */
/**
* @brief Function implementing the displayTASK thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_U8g2_TASK */
void U8g2_TASK(void *argument)
{
  /* USER CODE BEGIN U8g2_TASK */
  /* Infinite loop */
  DisplayCmd_t dispCmd;

  for(;;)
  {
	  osMessageQueueGet(displayQueueHandle, &dispCmd, NULL, osWaitForever);

	  osMutexAcquire(i2cMutexHandle, osWaitForever);

	  u8g2_ClearBuffer(&u8g2);
	  u8g2_SetBitmapMode(&u8g2, 1);
	  u8g2_SetFontMode(&u8g2, 1);

	  switch(dispCmd){
	  case MAIN_MENU:    	draw_main_menu();    		break;
	  case SETTING_MENU: 	draw_setting_menu(); 		break;
	  case INPUT_SELECT: 	draw_input_select(); 		break;
	  case MUTE_SETTING: 	draw_mute_setting(); 		break;
	  case TONE_SETTING: 	draw_tone_setting(); 		break;
	  case ATTENUATOR:	 	draw_attenuator_menu(); 	break;
	  case BUZZER:		 	draw_buzzer_setting();		break;
	  case CREDIT:		 	draw_place_holder();		break;
	  case EEPROM_LOADING: 	draw_eeprom_loading(); 		break;

	  case TONE_SETTING_HIGH: 		draw_tone_setting_set(TONE_SETTING_HIGH); 		break;
	  case TONE_SETTING_MID: 		draw_tone_setting_set(TONE_SETTING_MID); 		break;
	  case TONE_SETTING_BASS: 		draw_tone_setting_set(TONE_SETTING_BASS); 		break;
	  case TONE_SETTING_SUB:		draw_tone_setting_set(TONE_SETTING_SUB);		break;
	  case TONE_SETTING_LOUDNESS: 	draw_tone_setting_set(TONE_SETTING_LOUDNESS); 	break;
	  }

	  u8g2_SendBuffer(&u8g2);

	  osMutexRelease(i2cMutexHandle);
  }
  /* USER CODE END U8g2_TASK */
}

/* USER CODE BEGIN Header_TDA7419_TASK */
/**
* @brief Function implementing the toneControlTASK thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_TDA7419_TASK */


void TDA7419_TASK(void *argument) {
    TDA_Event evt;
    UserSettings_t prev = settings;   // shadow copy

    // --- one-time full sync at startup ---
    osEventFlagsWait(universalEventHandle, 0x01, osFlagsWaitAny, osWaitForever);
    TDA7419_InitFromSettings(&settings);

    for (;;) {
        if (osMessageQueueGet(tda7419QueueHandle, &evt, NULL, osWaitForever) == osOK) {
            if (evt == TDA_EVT_CHECK) {
                // --- compare and update only what changed ---
                if (prev.master_volume != settings.master_volume) {
                    TDA7419_SetVolume(settings.master_volume, 1);
                }

                if (prev.treble_tone_gain != settings.treble_tone_gain ||
                    prev.treble_c_freq   != settings.treble_c_freq) {
                    TDA7419_SetTreble(settings.treble_tone_gain, settings.treble_c_freq, 1);
                }

                if (prev.middle_tone_gain != settings.middle_tone_gain ||
                    prev.middle_q_factor  != settings.middle_q_factor) {
                    TDA7419_SetMiddle(settings.middle_tone_gain, settings.middle_q_factor, 1);
                }

                if (prev.bass_tone_gain != settings.bass_tone_gain ||
                    prev.bass_q_factor  != settings.bass_q_factor) {
                    TDA7419_SetBass(settings.bass_tone_gain, settings.bass_q_factor, 1);
                }

                if (prev.active_input != settings.active_input ||
                    prev.in1_gain     != settings.in1_gain ||
                    prev.in2_gain     != settings.in2_gain ||
                    prev.in3_gain     != settings.in3_gain ||
                    prev.auto_z       != settings.auto_z) {
                    TDA7419_SetInput(settings.active_input, get_input_gain(&settings), settings.auto_z);
                }

                if (prev.loudness_att      != settings.loudness_att ||
                    prev.loudness_c_freq  != settings.loudness_c_freq ||
                    prev.high_boost       != settings.high_boost) {
                    TDA7419_SetLoudness(settings.loudness_att, settings.loudness_c_freq, settings.high_boost, 0);
                }

                if (prev.soft_mute      != settings.soft_mute ||
                    prev.mute_mode      != settings.mute_mode ||
                    prev.soft_mute_time != settings.soft_mute_time ||
                    prev.soft_step_time != settings.soft_step_time) {
                    TDA7419_SoftMuteConfig(settings.soft_mute, settings.mute_mode, settings.soft_mute_time, settings.soft_step_time, 0);
                }

                if (prev.sub_cut_off   != settings.sub_cut_off ||
                    prev.middle_c_freq != settings.middle_c_freq ||
                    prev.bass_c_freq   != settings.bass_c_freq ||
                    prev.bass_dc       != settings.bass_dc) {
                    TDA7419_SetCenterSMB(settings.sub_cut_off, settings.middle_c_freq, settings.bass_c_freq, settings.bass_dc, 0);
                }

                if (prev.left_att   != settings.left_att)   TDA7419_SetAttLF(settings.left_att, 0);
                if (prev.right_att  != settings.right_att)  TDA7419_SetAttRF(settings.right_att, 0);
                if (prev.s_left_att != settings.s_left_att) TDA7419_SetAttLR(settings.s_left_att, 0);
                if (prev.s_right_att!= settings.s_right_att)TDA7419_SetAttRR(settings.s_right_att, 0);
                if (prev.sub_att    != settings.sub_att)   	TDA7419_SetAttSub(settings.sub_att, 0);
            }
            else if (evt == TDA_EVT_FULL_SYNC) {
                TDA7419_InitFromSettings(&settings);
            }

            // --- update shadow copy ---
            prev = settings;
        }
    }
}


/* USER CODE BEGIN Header_EEPROM_TASK */
/**
* @brief Function implementing the saveSettingsTAS thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_EEPROM_TASK */
void EEPROM_TASK(void *argument) {
    // --- load once at startup ---
    EEPROM_LoadSettings();
    osEventFlagsSet(universalEventHandle, 0x01); // signal ready

    DisplayCmd_t dispCmd = (DisplayCmd_t)current_menu;
    osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);

    UserSettings_t prev_settings;
    memcpy(&prev_settings, &settings, sizeof(UserSettings_t));

    uint32_t lastSave = osKernelGetTickCount();

    for (;;) {
        osDelay(1000);

        if ((osKernelGetTickCount() - lastSave) >= settings.eeprom_save_interval * 1000) {
            lastSave = osKernelGetTickCount();

            if (memcmp(&prev_settings, &settings, sizeof(UserSettings_t)) != 0) {
                // show loading screen
                DisplayCmd_t dispCmd = EEPROM_LOADING;
                osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);

                EEPROM_SaveSettings();

                // restore last screen
                dispCmd = (DisplayCmd_t)current_menu;
                osMessageQueuePut(displayQueueHandle, &dispCmd, 0, 0);

                memcpy(&prev_settings, &settings, sizeof(UserSettings_t));
            }
        }
    }
}

/* USER CODE BEGIN Header_BUZZER_TASK */
/**
* @brief Function implementing the buzzerTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_BUZZER_TASK */
void BUZZER_TASK(void *argument)
{
  /* USER CODE BEGIN BUZZER_TASK */
  /* Infinite loop */
  for(;;)
  {
	  uint32_t flags = osEventFlagsWait(universalEventHandle, EV_BUZZER_BEEP, osFlagsWaitAny, osWaitForever);

	  if(flags & EV_BUZZER_BEEP){
		  BUZZER(1);
		  osDelay(20);
		  BUZZER(0);
	  }
  }
  /* USER CODE END BUZZER_TASK */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM2 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM2)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
