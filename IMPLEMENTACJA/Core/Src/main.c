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
#include "dma.h"
#include "i2c.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "motors.h"
#include "ibus.h"
#include "bno055.h"
#include "pid.h"
#include "bno055_stm32.h"
#include <stdio.h>

extern UART_HandleTypeDef huart2;


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
    uint16_t roll;
    uint16_t pitch;
    uint16_t throttle;
    uint16_t yaw;
    uint16_t arm;
    uint16_t mode;
} rc_t;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile uint8_t control_loop_flag = 0;

volatile rc_t rc;

#define CTRL_DT 0.005f

#define RC_DEADBAND      0.05f     // 5% drązla
#define MAX_ANGLE_DEG    25.0f     // roll/pitch max z pilota
#define MAX_YAW_RATE_DPS 120.0f    // yaw rate z pilota


static PID_t pid_roll;
static PID_t pid_pitch;




// NASTAWY REGULATORA PID
volatile float dbg_Kp = 6.5;
volatile float dbg_Ki = 0.3;
volatile float dbg_Kd = 0.7;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static inline float clampf(float x, float lo, float hi)
{
    if (x < lo) return lo;
    if (x > hi) return hi;
    return x;
}

static inline float apply_deadband(float x, float db)
{
    if (x > -db && x < db) return 0.0f;
    // opcjonalnie: rescale po deadband, żeby nie tracić zakresu
    if (x > 0) return (x - db) / (1.0f - db);
    else       return (x + db) / (1.0f - db);
}

static inline float rc_us_to_norm(uint16_t us)
{
    float x = ((float)us - 1500.0f) / 500.0f;   // -1 .. +1
    return clampf(x, -1.0f, 1.0f);
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
  MX_DMA_Init();
  MX_TIM1_Init();
  MX_TIM6_Init();
  MX_USART1_UART_Init();
  MX_I2C3_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(&htim6);
  ibus_init();
  bno055_assignI2C(&hi2c3);

  bno055_setup();                 // reset + config
  bno055_setOperationModeNDOF();  // fuzja sensorów
  bno055_enableExternalCrystal(); // zewn zegar

  // PID - startowe nastawy
  PID_Init(&pid_roll,  dbg_Kp, dbg_Ki, dbg_Kd,  -300.0f, 300.0f,  -150.0f, 150.0f);
  PID_Init(&pid_pitch, dbg_Kp, dbg_Ki, dbg_Kd,  -300.0f, 300.0f,  -150.0f, 150.0f);

//  HAL_Delay(2000);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);

  // Jeśli trzymasz przycisk przy starcie -> kalibracja ESC
  // B1 TO JEST A5 NA STM
  if (HAL_GPIO_ReadPin(B1_cal_GPIO_Port, B1_cal_Pin) == GPIO_PIN_RESET)
  {
  	  HAL_Delay(4000);

      esc_calibrate_all();

      // po kalibracji program zatrzymany zeby odłączyć zasilanie
      while (1)
      {
          HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
          HAL_Delay(50);
      }
  }



  // WAIT NA PILOTA (iBUS)
  uint16_t last_ndtr = ibus_dma_ndtr();
  uint32_t t0 = HAL_GetTick();

  while (!ibus_is_signal_present())
  {
    ibus_process();

    // co 200ms mignij LED
    if (HAL_GetTick() - t0 > 2000)
    {
      t0 = HAL_GetTick();
      HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);

      uint16_t now = ibus_dma_ndtr();

      // jeśli NDTR się NIE zmienia -> UART/DMA nie dostaje żadnych bajtów
      // jeśli NDTR się zmienia, a frames_ok nadal 0 -> wina: parser
      last_ndtr = now;
    }
  }


  motors_arm();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  bno055_vector_t euler;
  bno055_calibration_state_t cal;

  static uint16_t led_counter = 0;

  // opcjonalnie: do wykrywania zmian nastaw i resetu integratora
  static float last_Kp = 0.0f, last_Ki = 0.0f, last_Kd = 0.0f;

  while (1)
  {
      // czeka na flagę z TIM6
      if (!control_loop_flag)
          continue;

      control_loop_flag = 0;

      // DEBUG: mruganie led co 1s
      led_counter++;
      if (led_counter >= 200)
      {
          led_counter = 0;
          HAL_GPIO_TogglePin(LD3_GPIO_Port, LD3_Pin);
      }

      // iBUS to info tylko do debugowania
      ibus_process();

      rc.roll     = ibus_read_channel(0);
      rc.pitch    = ibus_read_channel(1);
      rc.throttle = ibus_read_channel(2);
      rc.yaw      = ibus_read_channel(3);
      rc.arm      = ibus_read_channel(4);
      rc.mode     = ibus_read_channel(5);

      uint16_t throttle_us = rc.throttle;

      // --- 2) IMU ---
      euler = bno055_getVectorEuler();
      cal   = bno055_getCalibrationState();   // tylko do debug

      float roll_meas  = (float)euler.y;     // ROLL
      float pitch_meas = -(float)euler.z;    // PITCH (odwrócona oś)
      // float yaw_meas = (float)euler.x;    // YAW (nie używamy)

      // Bezpieczeństwo: niski gaz ---
      if (throttle_us < 1050)
      {
          PID_Reset(&pid_roll);
          PID_Reset(&pid_pitch);

          mixer_update(0.0f, 0.0f, 0.0f, 1000);
          continue;
      }

      // Setpointy z pilota
      float roll_in  = rc_us_to_norm(rc.roll);
      float pitch_in = rc_us_to_norm(rc.pitch);
      float yaw_in   = rc_us_to_norm(rc.yaw);

      // deadband
      roll_in  = apply_deadband(roll_in,  RC_DEADBAND);
      pitch_in = apply_deadband(pitch_in, RC_DEADBAND);
      yaw_in   = apply_deadband(yaw_in,   RC_DEADBAND);

      // (opcjonalnie) odwrócenie osi jeśli drążek działa “na odwrót”
      // roll_in  = -roll_in;
      // pitch_in = -pitch_in;
      // yaw_in   = -yaw_in;

      // ANGLE MODE dla roll/pitch
      float roll_sp  = roll_in  * MAX_ANGLE_DEG;
      float pitch_sp = pitch_in * MAX_ANGLE_DEG;

      // YAW jako RATE MODE
      float yaw_rate_sp = yaw_in * MAX_YAW_RATE_DPS;


      // LIVE TUNING: podmiana nastaw PID z debuggera
      pid_roll.kp  = dbg_Kp;
      pid_roll.ki  = dbg_Ki;
      pid_roll.kd  = dbg_Kd;

      pid_pitch.kp = dbg_Kp;
      pid_pitch.ki = dbg_Ki;
      pid_pitch.kd = dbg_Kd;

      // do debugowania - jesli zmienilismy nastawy PID -> reset integratora
      if (dbg_Kp != last_Kp || dbg_Ki != last_Ki || dbg_Kd != last_Kd)
      {
          PID_Reset(&pid_roll);
          PID_Reset(&pid_pitch);
          last_Kp = dbg_Kp;
          last_Ki = dbg_Ki;
          last_Kd = dbg_Kd;
      }

      static uint8_t uart_div = 0;

      uart_div++;
      if (uart_div >= 20) // 200Hz / 20 = 10Hz
      {
          uart_div = 0;
          printf("ROLL=%.2f  PITCH=%.2f\r\n", roll_meas, pitch_meas);
          printf("roll out_min=%.2f  out_max=%.2f\r\n", &pid_roll.out_min, &pid_roll.out_max);
          printf("pitch out_min=%.2f  out_max=%.2f\r\n", &pid_pitch.out_min, &pid_pitch.out_max);

      }

      // PID (dt = 0.005s dla TIM6 = 200Hz)
      float u_roll  = PID_Update(&pid_roll,  roll_sp,  roll_meas,  CTRL_DT);
      float u_pitch = PID_Update(&pid_pitch, pitch_sp, pitch_meas, CTRL_DT);
      float u_yaw   = yaw_rate_sp;   // YAW bez pida poki co

      // Mixer - push do silników
      mixer_update(u_roll, u_pitch, u_yaw, throttle_us);
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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 40;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable MSI Auto calibration
  */
  HAL_RCCEx_EnableMSIPLLMode();
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM6)
    {
        control_loop_flag = 1;   // flaga dla pętli głównej
    }
}

int _write(int file, char *ptr, int len)
{
    HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
    return len;
}




/* USER CODE END 4 */

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
