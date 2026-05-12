/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "spi.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "Sd_spi.h"
#include "adxl355.h"
#include "ff.h"
#include "quectel_drive.h"
#include "tasks.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
FRESULT fres;
// Shadow variables for display
const char* range_str[] = {"+/- 2g", "+/- 4g", "+/- 8g"};
const char* odr_str[] = {"?", "4000Hz", "2000Hz", "1000Hz", "500Hz", "250Hz", "125Hz", "62.5Hz", "31.25Hz"};
int cur_range_idx = 0;
int cur_odr_idx = 6;
float trigger_g = 0.02f;
uint8_t hpf_enabled = 0;
uint8_t act_count = 5;
uint8_t operation_mode = 2;
volatile uint8_t g_modem_abort_enabled = 0;
osMutexId_t uart_mutexHandle = NULL;

// Placeholder variables for future power measurement peripheral
float voltage_val = 0.0f;
float current_val = 0.0f;
float power_val = 0.0f;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

void Apply_Remote_Config(const char* key, const char* val);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void Apply_Remote_Config(const char* key, const char* val) {
    if (strcmp(key, "RANGE") == 0) {
        int g = atoi(val);
        if (g <= 2) {
            ADXL355_Set_Range(ADXL355_RANGE_2G);
            cur_range_idx = 0;
        } else if (g <= 4) {
            ADXL355_Set_Range(ADXL355_RANGE_4G);
            cur_range_idx = 1;
        } else {
            ADXL355_Set_Range(ADXL355_RANGE_8G);
            cur_range_idx = 2;
        }
        printf("[CONFIG] RANGE set to %s\r\n", range_str[cur_range_idx]);
    } else if (strcmp(key, "ODR_HZ") == 0) {
        int odr = atoi(val);
        switch (odr) {
            case 4000: ADXL355_Set_ODR(ADXL355_ODR_4000HZ); cur_odr_idx = 1; break;
            case 2000: ADXL355_Set_ODR(ADXL355_ODR_2000HZ); cur_odr_idx = 2; break;
            case 1000: ADXL355_Set_ODR(ADXL355_ODR_1000HZ); cur_odr_idx = 3; break;
            case 500:  ADXL355_Set_ODR(ADXL355_ODR_500HZ);  cur_odr_idx = 4; break;
            case 250:  ADXL355_Set_ODR(ADXL355_ODR_250HZ);  cur_odr_idx = 5; break;
            case 125:  ADXL355_Set_ODR(ADXL355_ODR_125HZ);  cur_odr_idx = 6; break;
            case 62:   ADXL355_Set_ODR(ADXL355_ODR_62_5HZ); cur_odr_idx = 7; break;
            case 31:   ADXL355_Set_ODR(ADXL355_ODR_31_25HZ); cur_odr_idx = 8; break;
            default: break;
        }
        if (cur_odr_idx > 0) printf("[CONFIG] ODR set to %s\r\n", odr_str[cur_odr_idx]);
    } else if (strcmp(key, "TRIGGER_G") == 0) {
        float t = atof(val);
        if (t > 0.0f) {
            trigger_g = t;
            printf("[CONFIG] Trigger set to %.2f G\r\n", trigger_g);
        }
    } else if (strcmp(key, "HPF") == 0) {
        uint8_t enable = 0;
        if (strcasecmp(val, "ON") == 0 || strcmp(val, "1") == 0) enable = 1;
        else if (strcasecmp(val, "OFF") == 0 || strcmp(val, "0") == 0) enable = 0;
        hpf_enabled = enable;
        ADXL355_Set_HPF(hpf_enabled);
        printf("[CONFIG] HPF %s\r\n", hpf_enabled ? "ON" : "OFF");
    } else if (strcmp(key, "ACT_COUNT") == 0) {
        int c = atoi(val);
        if (c < 1) c = 1;
        if (c > 255) c = 255;
        act_count = (uint8_t)c;
        printf("[CONFIG] ACT_COUNT set to %d\r\n", c);
    } else if (strcmp(key, "OPERATION_MODE") == 0 || strcmp(key, "MODE") == 0) {
        int m = atoi(val);
        if (m == 1 || m == 2) {
            operation_mode = (uint8_t)m;
            printf("[CONFIG] OPERATION_MODE=%d\r\n", m);
        } else {
            operation_mode = 2;
            printf("[CONFIG] OPERATION_MODE invalid, forcing=2\r\n");
        }
    }
}

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
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  MX_SPI2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  Modem_Init(&huart1);
  printf("\r\n--- AWTAS INITIALIZING (AUTONOMOUS WIRELESS TRIAXIAL ADQUISITION SYSTEM) ---\r\n");

  if (ADXL355_Init(&hspi2)) {
      printf("[SENSOR] ADXL355 Initialized Successfully\r\n");
      ADXL355_LevelToZero();
  } else {
      printf("[SENSOR] ADXL355 Initialization Failed\r\n");
  }

  if (sd_mount() == 0) {
      fres = FR_OK;
  } else {
      fres = FR_NOT_READY;
  }

  /* RTOS takes over from here -- tasks handle acquisition, upload, and CLI */
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();
  uart_mutexHandle = osMutexNew(NULL);
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
  }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
// Redirect printf to UART (mutex-protected for multi-task safety)
int _write(int file, char *ptr, int len)
{
  if (uart_mutexHandle != NULL) {
    osMutexAcquire(uart_mutexHandle, osWaitForever);
  }
  HAL_StatusTypeDef status = HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, 100);
  if (uart_mutexHandle != NULL) {
    osMutexRelease(uart_mutexHandle);
  }
  if (status != HAL_OK) {
  }
  return len;
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == ADXL_INT1_Pin) {
    osEventFlagsSet(sensor_event_flagsHandle, EVT_MOTION_DETECTED);
  }
}
/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1)
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
