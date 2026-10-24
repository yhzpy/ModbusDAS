/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : ModbusDAS - Modbus RTU Data Acquisition Station
  *                   STM32F103ZET6 + FreeRTOS + LVGL
  *                   Step 0: Task skeleton (compile & boot verification)
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "fsmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>

/* FreeRTOS */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

/* SYSTEM (Zhengdian Atom) */
#include "./SYSTEM/delay/delay.h"
#include "./SYSTEM/sys/sys.h"
#include "./SYSTEM/usart/myusart.h"

/* BSP (Zhengdian Atom) */
#include "./BSP/LCD/lcd.h"
#include "./BSP/LED/led.h"
#include "./BSP/KEY/key.h"

/* Project drivers (hand-written, no CubeMX) */
#include "./DRIVER/i2c1_reg.h"
#include "./DRIVER/SHT30/sht30.h"
#include "./DRIVER/DS3231/ds3231.h"
#include "./DRIVER/MAX485/max485.h"
#include "./DRIVER/EC11/ec11.h"
#include "./DRIVER/flash_log.h"
#include "./PROTOCOL/modbus_reg_map.h"
#include "./PROTOCOL/modbus_rtu.h"

/* BSP 命名适配：BSP 库为旧版小写命名（led_init/lcd_init/lcd_clear），
 * main 骨架按新版大写命名（LED_Init/LCD_Init/LCD_Clear）调用，此处宏桥接。
 * lcd_show_string 比 LCD_ShowString 多一个 color 参数，固定用 BLACK。 */
#define LED_Init()          led_init()
#define LED0_Toggle()       LED0_TOGGLE()
#define LED1_Toggle()       LED1_TOGGLE()
#define LCD_Init()          lcd_init()
#define LCD_Clear(c)        lcd_clear(c)
#define LCD_ShowString(x, y, w, h, size, s) \
                            lcd_show_string(x, y, w, h, size, (char *)(s), BLACK)
/* USER CODE END Includes */


/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/**
 * @brief Sensor data packet - flows through Queue1 (SensorAcq -> Logger)
 */
typedef struct {
    uint32_t timestamp;     /* Unix epoch or DS3231 BCD packed */
    int16_t  temperature;   /* SHT30 temp x100 (e.g. 2534 = 25.34 C) */
    int16_t  humidity;      /* SHT30 humi x100 (e.g. 4821 = 48.21 %) */
    uint16_t flags;         /* bit0: sensor error, bit1: flash error */
} SensorData_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* Task Priorities (higher number = higher priority) */
#define PRIO_TASK_SENSOR    4   /* I2C sensor acquisition, periodic 1Hz */
#define PRIO_TASK_LOGGER    3   /* SPI Flash logging, batch write */
#define PRIO_TASK_MODBUS    5   /* RS485 Modbus RTU slave, event-driven */
#define PRIO_TASK_GUI       2   /* LVGL display, lowest RT priority */

/* Task Stack Sizes (in words, not bytes; 1 word = 4 bytes on Cortex-M3) */
#define STACK_TASK_SENSOR   256     /* 1KB */
#define STACK_TASK_LOGGER   384     /* 1.5KB */
#define STACK_TASK_MODBUS   512     /* 2KB */
#define STACK_TASK_GUI      512     /* 2KB - LVGL needs depth */

/* Queue Depths */
#define QUEUE_SENSOR_DATA_LEN   8   /* SensorAcq -> Logger */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* Task Handles */
TaskHandle_t hTaskSensor  = NULL;
TaskHandle_t hTaskLogger  = NULL;
TaskHandle_t hTaskModbus  = NULL;
TaskHandle_t hTaskGUI     = NULL;

/* Queues */
QueueHandle_t xQueueSensorData = NULL;   /* SensorData_t, Sensor -> Logger */

/* Mutexes */
SemaphoreHandle_t xMutexI2C       = NULL;  /* I2C1 bus: Sensor vs Modbus access */
SemaphoreHandle_t xMutexFlash     = NULL;  /* SPI2 W25Q64: Logger vs GUI history */
SemaphoreHandle_t xMutexModbusReg = NULL;  /* Recursive: register map R/W */

/* Modbus Register Table (global, protected by xMutexModbusReg) */
ModbusRegMap_t g_regMap;

/* Status counters (for diagnostics) */
volatile uint32_t g_sensorErrCount  = 0;
volatile uint32_t g_queueOvfCount   = 0;
volatile uint32_t g_modbusFrameCnt  = 0;
volatile uint32_t g_modbusCrcErrCnt = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* Task function prototypes */
static void Task_SensorAcq(void *pvParameters);
static void Task_Logger(void *pvParameters);
static void Task_Modbus(void *pvParameters);
static void Task_GUI(void *pvParameters);

/* FreeRTOS hook prototypes */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName);
void vApplicationMallocFailedHook(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_FSMC_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();            /* Step 3: RS485 Modbus (PA2/PA3 + DMA1 CH6/CH7) */
  MX_TIM4_Init();

  /* Hand-written peripheral init (no CubeMX) */
  I2C1_Init();                    /* Step 1: I2C1 bus for SHT30/DS3231/AT24C32 */
  /* EC11_Init();                 // Step 5: rotary encoder */

  /* USER CODE BEGIN 2 */

  /* BSP init */
  LED_Init();
  LCD_Init();
  LCD_Clear(WHITE);
  LCD_ShowString(30, 50, 200, 24, 24, (uint8_t *)"ModbusDAS v0.1");
  LCD_ShowString(30, 80, 200, 16, 16, (uint8_t *)"Booting FreeRTOS...");

  /* Step 1 验证：扫描 I2C 总线，串口打印发现的设备 */
  I2C1_Scan();

  /* Initialize Modbus register map with defaults */
  memset(&g_regMap, 0, sizeof(g_regMap));
  g_regMap.reg[MB_REG_SLAVE_ADDR - MB_REG_BASE] = 1;       /* default slave addr = 1 */
  g_regMap.reg[MB_REG_SAMPLE_PERIOD - MB_REG_BASE] = 1;    /* default 1 second */
  g_regMap.reg[MB_REG_BAUD_CODE - MB_REG_BASE] = 3;        /* default 115200 */

  /* Step 3: RS485 Modbus RTU slave（USART2 已在 MX_USART2_UART_Init 初始化）
   * 内部：PD2 方向脚 + 启动 DMA 循环接收 + IDLE 断帧 */
  ModbusRTU_Init(1, &g_regMap);

  /* Create IPC objects BEFORE creating tasks */
  xQueueSensorData = xQueueCreate(QUEUE_SENSOR_DATA_LEN, sizeof(SensorData_t));
  xMutexI2C        = xSemaphoreCreateMutex();
  xMutexFlash      = xSemaphoreCreateMutex();
  xMutexModbusReg  = xSemaphoreCreateRecursiveMutex();

  configASSERT(xQueueSensorData != NULL);
  configASSERT(xMutexI2C != NULL);
  configASSERT(xMutexFlash != NULL);
  configASSERT(xMutexModbusReg != NULL);

  /* Create application tasks */
  xTaskCreate(Task_SensorAcq, "Sensor",  STACK_TASK_SENSOR, NULL, PRIO_TASK_SENSOR, &hTaskSensor);
  xTaskCreate(Task_Logger,    "Logger",  STACK_TASK_LOGGER, NULL, PRIO_TASK_LOGGER, &hTaskLogger);
  xTaskCreate(Task_Modbus,    "Modbus",  STACK_TASK_MODBUS, NULL, PRIO_TASK_MODBUS, &hTaskModbus);
  xTaskCreate(Task_GUI,       "GUI",     STACK_TASK_GUI,    NULL, PRIO_TASK_GUI,    &hTaskGUI);

  configASSERT(hTaskSensor != NULL);
  configASSERT(hTaskLogger != NULL);
  configASSERT(hTaskModbus != NULL);
  configASSERT(hTaskGUI != NULL);

  printf("\r\n=== ModbusDAS Boot OK ===\r\n");
  printf("FreeRTOS Heap: %d bytes, Tasks: 4\r\n", configTOTAL_HEAP_SIZE);

  /* USER CODE END 2 */

  /* Start scheduler - never returns if heap sufficient */
  vTaskStartScheduler();

  /* USER CODE BEGIN WHILE */
  /* Should never reach here - if you do, heap is too small */
  while (1)
  {
  }
  /* USER CODE END WHILE */

  /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  *        HSE 8MHz -> PLL x9 -> SYSCLK 72MHz
  *        AHB=72MHz, APB1=36MHz, APB2=72MHz
  *        FLASH_LATENCY_2 (required for >24MHz)
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

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
}

/* USER CODE BEGIN 4 */

/*============================================================================
 * Task_SensorAcq - Periodic I2C sensor acquisition
 * Priority: 4 | Period: configurable via g_regMap (default 1s)
 *
 * Flow: Read SHT30 (temp/humi) + DS3231 (timestamp)
 *       -> Pack into SensorData_t
 *       -> Send to xQueueSensorData (for Logger task)
 *       -> Update g_regMap live registers (for Modbus task)
 *============================================================================*/
static void Task_SensorAcq(void *pvParameters)
{
    (void)pvParameters;
    SensorData_t data;
    TickType_t xLastWakeTime = xTaskGetTickCount();

    printf("[Sensor] Task started\r\n");

    for (;;)
    {
        /* TODO Step 1: Replace placeholders with real I2C reads
         * SHT30_Read(&data.temperature, &data.humidity);
         * DS3231_GetTimestamp(&data.timestamp);
         */
        data.temperature = 2500;  /* placeholder: 25.00 C */
        data.humidity    = 5000;  /* placeholder: 50.00 % */
        data.timestamp   = xTaskGetTickCount() / 1000;
        data.flags       = 0;

        /* Send to Logger queue (non-blocking, drop+count if full) */
        if (xQueueSend(xQueueSensorData, &data, 0) != pdTRUE)
        {
            g_queueOvfCount++;
        }

        /* Update Modbus register map (thread-safe with recursive mutex) */
        if (xSemaphoreTakeRecursive(xMutexModbusReg, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            g_regMap.reg[MB_REG_TEMPERATURE - MB_REG_BASE] = (uint16_t)data.temperature;
            g_regMap.reg[MB_REG_HUMIDITY - MB_REG_BASE]    = (uint16_t)data.humidity;
            g_regMap.reg[MB_REG_STATUS - MB_REG_BASE]      = data.flags;
            xSemaphoreGiveRecursive(xMutexModbusReg);
        }

        /* Periodic delay from register map config */
        uint16_t period_sec = g_regMap.reg[MB_REG_SAMPLE_PERIOD - MB_REG_BASE];
        if (period_sec == 0) period_sec = 1;
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(period_sec * 1000));
    }
}

/*============================================================================
 * Task_Logger - Batch write sensor data to W25Q64 SPI Flash
 * Priority: 3 | Event-driven: blocks on xQueueSensorData
 *
 * Flow: Receive from Queue -> accumulate 16 records
 *       -> page program to W25Q64 (with flash mutex)
 *       -> on boot: scan flash for last valid seq, resume
 *============================================================================*/
static void Task_Logger(void *pvParameters)
{
    (void)pvParameters;
    SensorData_t rxData;
    uint8_t batchCount = 0;
    /* TODO Step 2: SensorData_t batchBuf[16]; */
    /* TODO Step 2: uint32_t writeSeq = 0; // recovered from flash scan at init */

    printf("[Logger] Task started\r\n");

    for (;;)
    {
        /* Block until sensor data arrives */
        if (xQueueReceive(xQueueSensorData, &rxData, portMAX_DELAY) == pdTRUE)
        {
            batchCount++;
            /* TODO Step 2: batchBuf[batchCount-1] = rxData; */

            if (batchCount >= 16)
            {
                batchCount = 0;
                /* TODO Step 2:
                 * xSemaphoreTake(xMutexFlash, portMAX_DELAY);
                 * W25Q64_PageProgram(batchBuf, 16 * sizeof(SensorData_t));
                 * xSemaphoreGive(xMutexFlash);
                 */
                printf("[Logger] Batch write 16 records (TODO)\r\n");
            }
        }
    }
}

/*============================================================================
 * Task_Modbus - RS485 Modbus RTU slave responder
 * Priority: 5 (highest app priority - comm must not lag)
 * Event-driven: USART2 IDLE interrupt -> vTaskNotifyGiveFromISR -> parse
 *
 * Flow: Wait notification -> extract DMA rx buffer -> validate CRC
 *       -> dispatch function code (03/06/10) -> send response
 *============================================================================*/
static void Task_Modbus(void *pvParameters)
{
    (void)pvParameters;
    static uint8_t txBuf[MB_RTU_TX_BUF_SIZE];
    uint16_t txLen;

    printf("[Modbus] Task started (slave addr=%d)\r\n",
           g_regMap.reg[MB_REG_SLAVE_ADDR - MB_REG_BASE]);

    for (;;)
    {
        /* Block until USART2 IDLE notification from ISR (one frame received) */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        /* HandleFrame: CRC/addr 校验 + 功能码分发 + 重新启动 DMA 接收 */
        txLen = 0;
        if (ModbusRTU_HandleFrame(txBuf, &txLen) == MB_OK)
        {
            /* 内部：切发送 → DMA 发送 → 等 TC → 切回接收 */
            RS485_SendData(txBuf, txLen);
        }
        g_modbusFrameCnt++;
    }
}


/*============================================================================
 * Task_GUI - LVGL display handler
 * Priority: 2 (lowest - display can lag without breaking functionality)
 * Period: ~20ms (lv_task_handler cadence)
 *
 * Flow: lv_task_handler() -> flush to FSMC LCD
 *       -> read g_regMap for live data display
 *       -> handle encoder input via LVGL indev driver
 *============================================================================*/
static void Task_GUI(void *pvParameters)
{
    (void)pvParameters;

    printf("[GUI] Task started (LVGL placeholder)\r\n");

    /* TODO Step 5:
     * lv_init();
     * lv_port_disp_init();   // FSMC flush_cb
     * lv_port_indev_init();  // EC11 encoder
     * ui_create();           // build 3 screens
     */

    for (;;)
    {
        /* TODO Step 5: lv_task_handler(); */

        /* Placeholder: blink LED1 to show GUI task is alive */
        LED1_Toggle();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/*============================================================================
 * FreeRTOS Hooks
 *============================================================================*/

/**
 * @brief Stack overflow detected (configCHECK_FOR_STACK_OVERFLOW=2).
 *        Prints culprit task name, blinks LED0, halts.
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void)xTask;
    printf("\r\n!!! STACK OVERFLOW: %s !!!\r\n", pcTaskName);
    LCD_ShowString(30, 120, 200, 16, 16, (uint8_t *)"STACK OVERFLOW!");
    LCD_ShowString(30, 140, 200, 16, 16, (uint8_t *)pcTaskName);
    taskDISABLE_INTERRUPTS();
    while (1) { LED0_Toggle(); HAL_Delay(100); }
}

/**
 * @brief FreeRTOS heap exhausted on pvPortMalloc().
 */
void vApplicationMallocFailedHook(void)
{
    printf("\r\n!!! MALLOC FAILED !!!\r\n");
    LCD_ShowString(30, 160, 200, 16, 16, (uint8_t *)"MALLOC FAILED!");
    taskDISABLE_INTERRUPTS();
    while (1) { LED1_Toggle(); HAL_Delay(100); }
}

/* USER CODE END 4 */

/**
  * @brief  HAL timebase callback (TIM1, NOT SysTick).
  *         SysTick belongs to FreeRTOS scheduler exclusively.
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
  /* TODO Step 3: if (htim->Instance == TIM6) -> Modbus T3.5 frame timeout */
  /* TODO Step 5: if (htim->Instance == TIM7) -> lv_tick_inc(1) */
  /* USER CODE END Callback 1 */
}

/**
  * @brief  Error handler - disable interrupts and halt.
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  printf("\r\n!!! Error_Handler !!!\r\n");
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  printf("Assertion failed: %s:%lu\r\n", file, line);
  /* USER CODE END 6 */
}
#endif
