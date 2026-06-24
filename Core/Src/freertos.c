/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "tasks.h"
#include "main.h"
#include "cmsis_os.h"
#include <stdio.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "tasks.h"
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
/* USER CODE BEGIN Variables */

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for sensor_task */
osThreadId_t sensor_taskHandle;
const osThreadAttr_t sensor_task_attributes = {
  .name = "sensor_task",
  .stack_size = 512 * 4,   /* +100% margin for printf(%f) ~800B stack usage */
  .priority = (osPriority_t) osPriorityHigh,
};
/* Definitions for modem_task */
osThreadId_t modem_taskHandle;
const osThreadAttr_t modem_task_attributes = {
  .name = "modem_task",
  .stack_size = 1024 * 4,  /* +100% margin for UploadFile large locals (url+host+path+header[640]+buf[HTTP_CHUNK_SIZE]) */
  .priority = (osPriority_t) osPriorityAboveNormal,
};
/* Definitions for file_task */
osThreadId_t file_taskHandle;
const osThreadAttr_t file_task_attributes = {
  .name = "file_task",
  .stack_size = 1536 * 4,  /* increased from 1024: FatFs + scan FIL on stack needs margin */
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for control_task */
osThreadId_t control_taskHandle;
const osThreadAttr_t control_task_attributes = {
  .name = "control_task",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for sd_mutex */
osMutexId_t sd_mutexHandle;
const osMutexAttr_t sd_mutex_attributes = {
  .name = "sd_mutex"
};
/* Definitions for sensor_event_flags */
osEventFlagsId_t sensor_event_flagsHandle;
const osEventFlagsAttr_t sensor_event_flags_attributes = {
  .name = "sensor_event_flags"
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartSensorTask(void *argument);
void StartModemTask(void *argument);
void StartFileTask(void *argument);
void StartControlTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */
  /* Create the mutex(es) */
  /* creation of sd_mutex */
  sd_mutexHandle = osMutexNew(&sd_mutex_attributes);

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
  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of sensor_task */
  sensor_taskHandle = osThreadNew(StartSensorTask, NULL, &sensor_task_attributes);

  /* creation of modem_task */
  modem_taskHandle = osThreadNew(StartModemTask, NULL, &modem_task_attributes);

  /* creation of file_task */
  file_taskHandle = osThreadNew(StartFileTask, NULL, &file_task_attributes);

  /* creation of control_task */
  control_taskHandle = osThreadNew(StartControlTask, NULL, &control_task_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* creation of sensor_event_flags */
  sensor_event_flagsHandle = osEventFlagsNew(&sensor_event_flags_attributes);

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartSensorTask */
/**
* @brief Function implementing the sensor_taskk thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartSensorTask */
/* USER CODE BEGIN StartSensorTask */

/* USER CODE END StartSensorTask */

/* USER CODE BEGIN Header_StartModemTask */
/**
* @brief Function implementing the modem_task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartModemTask */
/* USER CODE BEGIN StartModemTask */

/* USER CODE END StartModemTask */

/* USER CODE BEGIN Header_StartPriorityNormall */

/* USER CODE END Header_StartPriorityNormall */

/* USER CODE BEGIN Header_StartControlTask */

/* USER CODE END Header_StartControlTask */

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* Stack overflow hook called by FreeRTOS when configCHECK_FOR_STACK_OVERFLOW=2 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    /* Disable interrupts and print diagnostic */
    portDISABLE_INTERRUPTS();
    printf("\r\n[FATAL] STACK OVERFLOW in task: %s\r\n", pcTaskName);
    for (;;) {
        /* Wait for debugger or WDT reset */
    }
}

/* Malloc failure hook */
void vApplicationMallocFailedHook(void)
{
    portDISABLE_INTERRUPTS();
    printf("\r\n[FATAL] MALLOC FAILED (heap exhausted)\r\n");
    for (;;) {
        /* Wait for debugger or WDT reset */
    }
}

/* USER CODE END Application */

