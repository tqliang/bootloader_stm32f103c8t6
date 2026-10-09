/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Bootloader - 双App分区固件升级与跳转
  ******************************************************************************
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
/* USER CODE BEGIN Includes */
#include "bootloader.h"
#include "bootloader_config.h"
#include "flash_if.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* ===== LED 指示灯宏 (PC13, 低电平亮) ===== */
#define LED_ON()    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET)
#define LED_OFF()   HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET)
#define LED_TOGGLE() HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin)

/* 升级触发引脚 */
#define UPDATE_TRIGGER_PIN       GPIO_PIN_0
#define UPDATE_TRIGGER_PORT      GPIOA
#define UPDATE_TRIGGER_ACTIVE    1       /* 1=高电平触发, 0=低电平触发 */

/* 指示含义:
   - 慢闪 (500ms):  升级模式
   - 快闪 (150ms):  无有效 App, 等待烧录
   - 快闪3次后灭:   正常启动, 跳转 App  */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static partition_info_t g_partition_info;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
/* USER CODE BEGIN PFP */
static uint8_t CheckUpdateTrigger(void);
static void Bootloader_UpdateMode(void);
static void Bootloader_NormalBoot(void);
static void LED_BlinkFast(void);
static void LED_BlinkSlow(void);
static void LED_FlashQuick(uint32_t count, uint32_t delay_ms);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ========================================================================== */
/*                   LED 指示函数                                             */
/* ========================================================================== */

static void LED_BlinkFast(void)
{
    LED_TOGGLE();
    HAL_Delay(150);
}

static void LED_BlinkSlow(void)
{
    LED_TOGGLE();
    HAL_Delay(500);
}

/**
  * @brief  LED 快闪指定次数后熄灭
  */
static void LED_FlashQuick(uint32_t count, uint32_t delay_ms)
{
    uint32_t i;
    for (i = 0; i < count; i++)
    {
        LED_ON();
        HAL_Delay(delay_ms);
        LED_OFF();
        if (i < count - 1)
        {
            HAL_Delay(delay_ms);
        }
    }
}

/* ========================================================================== */
/*                   升级触发检测                                             */
/* ========================================================================== */

static uint8_t CheckUpdateTrigger(void)
{
    uint8_t trigger = 0;

    /* 方式一: GPIO 引脚 (按键按下进入升级) */
    if (HAL_GPIO_ReadPin(UPDATE_TRIGGER_PORT, UPDATE_TRIGGER_PIN)
            == UPDATE_TRIGGER_ACTIVE)
    {
        trigger = 1;
    }

    /* 方式二: 分区信息中有 update_pending 标记 (上次升级未完成) */
    if (Bootloader_ReadPartitionInfo(&g_partition_info) == 0
            && g_partition_info.update_pending)
    {
        trigger = 1;
    }

    return trigger;
}

/* ========================================================================== */
/*                   升级模式: 版本检测 + 校验 + 激活                           */
/* ========================================================================== */

static void Bootloader_UpdateMode(void)
{
    int info_valid;
    int upgraded;
    uint32_t app_addr;

    info_valid = (Bootloader_ReadPartitionInfo(&g_partition_info) == 0);

    if (!info_valid)
    {
        /* 分区信息无效, 慢闪等待烧录 */
        goto wait_for_flash;
    }

    /* ===== 检测另一分区是否有更高版本 ===== */
    upgraded = (Bootloader_CheckAndUpgrade(&g_partition_info) == 0);

    if (upgraded)
    {
        /* 升级成功, 快速闪烁 3 次后跳转新固件 */
        LED_FlashQuick(3, 50);

        app_addr = (g_partition_info.active_partition == PARTITION_APP_B) ? APP_B_START : APP_A_START;
        Bootloader_JumpToApp(app_addr);

        /* 跳转失败, 慢闪等待 */
        goto wait_for_flash;
    }

    /* 无新固件: 回退正常启动 (防止按键误触导致卡死) */
    Bootloader_NormalBoot();
    return;

wait_for_flash:
    /* 最后尝试一次正常启动 (可能旧 App 仍可用) */
    Bootloader_NormalBoot();

    /* NormalBoot 也失败了: 进入等待烧录模式 (慢闪) */
    LED_ON();
    HAL_Delay(200);
    LED_OFF();

    while (1)
    {
        LED_BlinkSlow();
    }
}

/* ========================================================================== */
/*                   正常启动 (回滚保护 + 自动升级 + 跳转)                      */
/* ========================================================================== */

static void Bootloader_NormalBoot(void)
{
    uint32_t app_addr = APP_A_START;
    int info_valid;

    info_valid = (Bootloader_ReadPartitionInfo(&g_partition_info) == 0);

    if (info_valid)
    {
        /* ===== 步骤 1: 回滚检测 ===== */
        /* 检测活跃分区是否损坏, 损坏则自动切回备用分区 */
        if (Bootloader_TryRollback(&g_partition_info, &app_addr) != 0)
        {
            /* 两个分区都损坏了, 快闪等待烧录 */
            LED_FlashQuick(3, 100);
            while (1) 
            { 
              LED_BlinkFast(); 
            }
        }

        /* ===== 步骤 2: 自动升级检测 ===== */
        /* 检测另一分区是否有更高版本的新固件 */
        Bootloader_CheckAndUpgrade(&g_partition_info);

        /* 重新确定活跃分区地址 */
        app_addr = (g_partition_info.active_partition == PARTITION_APP_B) ? APP_B_START : APP_A_START;
    }
    else
    {
        /* ===== 首次启动: 无分区信息, 自动检测 ===== */
        uint32_t sp_a = *(volatile uint32_t *)(APP_A_START);
        uint32_t pc_a = *(volatile uint32_t *)(APP_A_START + 4);
        uint32_t sp_b = *(volatile uint32_t *)(APP_B_START);
        uint32_t pc_b = *(volatile uint32_t *)(APP_B_START + 4);

        int a_valid = ((sp_a & 0x2FFE0000) == 0x20000000)
                   && ((pc_a & 0x08000000) == 0x08000000)
                   && !(sp_a == 0xFFFFFFFF && pc_a == 0xFFFFFFFF);
        int b_valid = ((sp_b & 0x2FFE0000) == 0x20000000)
                   && ((pc_b & 0x08000000) == 0x08000000)
                   && !(sp_b == 0xFFFFFFFF && pc_b == 0xFFFFFFFF);

        if (a_valid || b_valid)
        {
            /* 优先选择 App A */
            uint8_t chosen = a_valid ? PARTITION_APP_A : PARTITION_APP_B;
            uint32_t chosen_addr = a_valid ? APP_A_START : APP_B_START;

            /* 构造初始分区信息 */
            g_partition_info.magic            = PARTITION_MAGIC;
            g_partition_info.active_partition = chosen;

            if (a_valid) 
            {
                g_partition_info.app_a_crc = CRC32_Calculate(
                    (uint8_t *)APP_A_START, APP_A_SIZE
                  );
                g_partition_info.app_a_size    = APP_A_SIZE;
                g_partition_info.app_a_version = *(volatile uint32_t *)(APP_A_START + APP_VERSION_OFFSET);
            }
            if (b_valid) 
            {
                g_partition_info.app_b_crc = CRC32_Calculate(
                    (uint8_t *)APP_B_START, APP_B_SIZE
                  );
                g_partition_info.app_b_size    = APP_B_SIZE;
                g_partition_info.app_b_version = *(volatile uint32_t *)(APP_B_START + APP_VERSION_OFFSET);
            }

            Bootloader_WritePartitionInfo(&g_partition_info);
            app_addr = chosen_addr;
        }
        else
        {
            /* 两个 App 都无效, 快闪等待烧录 */
            LED_FlashQuick(3, 100);
            while (1) { LED_BlinkFast(); }
        }
    }

    /* ===== 最终跳转 ===== */
    {
        uint32_t app_sp = *(volatile uint32_t *)(app_addr);
        uint32_t app_pc = *(volatile uint32_t *)(app_addr + 4);

        if (((app_sp & 0x2FFE0000) == 0x20000000)
                && ((app_pc & 0x08000000) == 0x08000000))
        {
            LED_FlashQuick(3, 50);
            Bootloader_JumpToApp(app_addr);
        }
    }

    /* 跳转失败, 快闪等待 */
    while (1) 
    { 
      LED_BlinkFast(); 
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
  /* USER CODE BEGIN 2 */

  /* 上电指示: LED 亮 100ms */
  LED_ON();
  HAL_Delay(100);
  LED_OFF();

  /* 检测升级触发 */
  if (CheckUpdateTrigger())
  {
      Bootloader_UpdateMode();
  }

  /* 正常启动: 跳转 App */
  Bootloader_NormalBoot();

  /* 不应该到这里 */
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/* ========================================================================== */
/*                       外设初始化                                           */
/* ========================================================================== */

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

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

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_Pin */
  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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