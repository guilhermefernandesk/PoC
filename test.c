/*
 * test.c - Validacao de hardware - STM32H743ZIT6
 * -----------------------------------------------------------------------------
 * Arquivo UNICO, sem CubeMX. Depende apenas do HAL ja existente no projeto
 * de firmware (habilite no stm32h7xx_hal_conf.h):
 *
 *   #define HAL_I2C_MODULE_ENABLED
 *   #define HAL_UART_MODULE_ENABLED
 *   #define HAL_FDCAN_MODULE_ENABLED
 *
 * Substitua o main() do seu projeto por este. Nao usa main.c gerado, nao usa
 * stm32h7xx_it.c gerado e nao usa printf retargetado.
 *
 * Clock usado: HSI 64 MHz -> PLL1 (M=8, N=32, P=4) -> SYSCLK 64 MHz
 *              PLL1_Q = 64 MHz -> FDCAN
 *              HSI     = 64 MHz -> I2C4
 *              PCLK1 / PCLK2 = 64 MHz -> UART7 / USART6
 *
 * Mapeamento identificado no seu scan:
 *   I2C1 : 0x50 / 0x58  -> provaveis EEPROMs (nao usados aqui)
 *   I2C4 : 0x20 / 0x21 / 0x23 -> 3x PCA9555/PCA9535 ( LEDs )
 *
 * Os pinos do I2C4 serao descobertos automaticamente (veja i2c4_sets[]).
 * LEDs com anodo na saida do PCA => nivel ALTO acende (PCA_LED_ACTIVE_HIGH = 1).
 * RS232: USART6 (PC6/PC7, AF7) e UART7 (PE7/PE8, AF7), 115200 8N1.
 * CAN  : FDCAN1 em LOOPBACK INTERNO, classic CAN 500 kbit/s, ID 0x123.
 * -----------------------------------------------------------------------------
 */

#include "stm32h7xx_hal.h"

#include <stdarg.h>
#include <stdio.h>

/* -------------------------------------------------------------------------- */
/* Configuracao - ajuste aqui quando o esquema confirmar algo diferente       */
/* -------------------------------------------------------------------------- */

#define TEST_UART_BAUDRATE        115200UL
#define TEST_I2C4_TIMING_100K     0x10707DBCUL   /* valido para I2C4 clk = 64 MHz */
#define TEST_CAN_ID               0x123U
#define TEST_BLINK_MS             500U
#define TEST_PING_MS              1000U
#define TEST_STATUS_MS            5000U

#define PCA_ADDR_0                0x20U
#define PCA_ADDR_1                0x21U
#define PCA_ADDR_2                0x23U

#define PCA_REG_INPUT0            0x00U
#define PCA_REG_INPUT1            0x01U
#define PCA_REG_OUTPUT0           0x02U
#define PCA_REG_OUTPUT1           0x03U
#define PCA_REG_POLARITY0         0x04U
#define PCA_REG_POLARITY1         0x05U
#define PCA_REG_CONFIG0           0x06U
#define PCA_REG_CONFIG1           0x07U

/* 1 = anodo no pino do PCA (corrente sai do PCA). 0 = catodo no pino do PCA. */
#define PCA_LED_ACTIVE_HIGH       1

/* -------------------------------------------------------------------------- */
/* Handles e estado                                                            */
/* -------------------------------------------------------------------------- */

static I2C_HandleTypeDef  hi2c4;
static UART_HandleTypeDef huart6;
static UART_HandleTypeDef huart7;
static FDCAN_HandleTypeDef hfdcan1;

static uint32_t uart6_rx = 0U, uart6_err = 0U;
static uint32_t uart7_rx = 0U, uart7_err = 0U;
static uint32_t can_tx_ok = 0U, can_tx_fail = 0U, can_rx_ok = 0U;
static uint8_t  can_ready = 0U;
static uint8_t  pca_found = 0U;
static const char *i2c4_pins = "NAO DETECTADO";

/* -------------------------------------------------------------------------- */
/* Log pela propria UART (espelhado nas duas, para o caso de uma estar morta) */
/* -------------------------------------------------------------------------- */

static void BoardLog(const char *fmt, ...)
{
    char     buf[256];
    va_list  ap;
    int      len;

    va_start(ap, fmt);
    len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (len <= 0)
    {
        return;
    }
    if ((size_t)len >= sizeof(buf))
    {
        len = (int)sizeof(buf) - 1;
    }

    if (huart6.Instance != NULL)
    {
        (void)HAL_UART_Transmit(&huart6, (uint8_t *)buf, (uint16_t)len, 50U);
    }
    if (huart7.Instance != NULL)
    {
        (void)HAL_UART_Transmit(&huart7, (uint8_t *)buf, (uint16_t)len, 50U);
    }
}

static void Fatal(const char *msg)
{
    BoardLog("\r\n[FATAL] %s\r\n", msg);
    __disable_irq();
    for (;;)
    {
        /* travado para facilitar diagnostico com depurador */
    }
}

/* -------------------------------------------------------------------------- */
/* Clock                                                                       */
/* -------------------------------------------------------------------------- */

static void SystemClockConfig_Board(void)
{
    RCC_OscInitTypeDef       osc   = {0};
    RCC_ClkInitTypeDef       clk   = {0};
    RCC_PeriphCLKInitTypeDef per   = {0};

    /*
     * HSI = 64 MHz. PLL1: 64/8 = 8 MHz (VCO in), 8*32 = 256 MHz (VCO),
     * P=/4 -> 64 MHz (SYSCLK), Q=/4 -> 64 MHz (FDCAN).
     */
    osc.OscillatorType      = RCC_OSCILLATORTYPE_HSI;
    osc.HSIState            = RCC_HSI_ON;
    osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    osc.PLL.PLLState        = RCC_PLL_ON;
    osc.PLL.PLLSource       = RCC_PLLSOURCE_HSI;
    osc.PLL.PLLM            = 8U;
    osc.PLL.PLLN            = 32U;
    osc.PLL.PLLP            = 4U;
    osc.PLL.PLLQ            = 4U;
    osc.PLL.PLLR            = 4U;
    osc.PLL.PLLRGE          = RCC_PLL1VCIRANGE_2;  /* 4..8 MHz na entrada do VCO */
    osc.PLL.PLLVCOSEL       = RCC_PLL1VCOWIDE;     /* 256 MHz */
    osc.PLL.PLLFRACN        = 0U;

    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
    {
        Fatal("falha em HAL_RCC_OscConfig");
    }

    clk.ClockType      = RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK |
                         RCC_CLOCKTYPE_D1PCLK1 | RCC_CLOCKTYPE_PCLK1 |
                         RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_D3PCLK1;
    clk.SYSCLKSource   = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider  = RCC_HCLK_DIV1;    /* HCLK  = 64 MHz */
    clk.APB3CLKDivider = RCC_APB3_DIV1;
    clk.APB1CLKDivider = RCC_APB1_DIV1;    /* PCLK1 = 64 MHz (UART7) */
    clk.APB2CLKDivider = RCC_APB2_DIV1;    /* PCLK2 = 64 MHz (USART6) */
    clk.APB4CLKDivider = RCC_APB4_DIV1;

    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK)
    {
        Fatal("falha em HAL_RCC_ClockConfig");
    }

    per.PeriphClockSelection    = RCC_PERIPHCLK_I2C4 |
                                  RCC_PERIPHCLK_USART16 |
                                  RCC_PERIPHCLK_USART234578 |
                                  RCC_PERIPHCLK_FDCAN;
    per.I2c4ClockSelection      = RCC_I2C4CLKSOURCE_HSI;
    per.Usart16ClockSelection   = RCC_USART16CLKSOURCE_PCLK2;
    per.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_PCLK1;
    per.FdcanClockSelection     = RCC_FDCANCLKSOURCE_PLL;   /* PLL1_Q = 64 MHz */

    if (HAL_RCCEx_PeriphCLKConfig(&per) != HAL_OK)
    {
        Fatal("falha em HAL_RCCEx_PeriphCLKConfig");
    }

    SystemCoreClockUpdate();
    (void)HAL_InitTick(TICK_INT_PRIORITY);
}

/* -------------------------------------------------------------------------- */
/* UART / RS232                                                                */
/* -------------------------------------------------------------------------- */

static void UART6_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_USART6_CLK_ENABLE();

    /* PC6 = USART6_TX, PC7 = USART6_RX, AF7 (conferido na tabela da H743) */
    gpio.Pin       = GPIO_PIN_6 | GPIO_PIN_7;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = 7U;
    HAL_GPIO_Init(GPIOC, &gpio);

    huart6.Instance                   = USART6;
    huart6.Init.BaudRate              = TEST_UART_BAUDRATE;
    huart6.Init.WordLength            = UART_WORDLENGTH_8B;
    huart6.Init.StopBits              = UART_STOPBITS_1;
    huart6.Init.Parity                = UART_PARITY_NONE;
    huart6.Init.Mode                  = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl             = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling          = UART_OVERSAMPLING_16;
    huart6.Init.OneBitSampling        = UART_ONE_BIT_SAMPLE_DISABLE;
    huart6.Init.ClockPrescaler        = UART_PRESCALER_DIV1;
    huart6.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart6) != HAL_OK)
    {
        Fatal("falha na init da USART6");
    }
}

static void UART7_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_UART7_CLK_ENABLE();

    /* PE7 = UART7_RX, PE8 = UART7_TX, AF7 (conferido na tabela da H743) */
    gpio.Pin       = GPIO_PIN_7 | GPIO_PIN_8;
    gpio.Mode      = GPIO_MODE_AF_PP;
    gpio.Pull      = GPIO_NOPULL;
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = 7U;
    HAL_GPIO_Init(GPIOE, &gpio);

    huart7.Instance                   = UART7;
    huart7.Init.BaudRate              = TEST_UART_BAUDRATE;
    huart7.Init.WordLength            = UART_WORDLENGTH_8B;
    huart7.Init.StopBits              = UART_STOPBITS_1;
    huart7.Init.Parity                = UART_PARITY_NONE;
    huart7.Init.Mode                  = UART_MODE_TX_RX;
    huart7.Init.HwFlowCtl             = UART_HWCONTROL_NONE;
    huart7.Init.OverSampling          = UART_OVERSAMPLING_16;
    huart7.Init.OneBitSampling        = UART_ONE_BIT_SAMPLE_DISABLE;
    huart7.Init.ClockPrescaler        = UART_PRESCALER_DIV1;
    huart7.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    if (HAL_UART_Init(&huart7) != HAL_OK)
    {
        Fatal("falha na init da UART7");
    }
}

static void UART_PollEcho(UART_HandleTypeDef *huart,
                          uint32_t *rx_count, uint32_t *err_count)
{
    uint8_t ch;

    if (HAL_UART_Receive(huart, &ch, 1U, 2U) == HAL_OK)
    {
        (*rx_count)++;
        (void)HAL_UART_Transmit(huart, &ch, 1U, 10U);
    }

    if (huart->ErrorCode != HAL_UART_ERROR_NONE)
    {
        (*err_count)++;
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);
        __HAL_UART_CLEAR_PEFLAG(huart);
        huart->ErrorCode = HAL_UART_ERROR_NONE;
    }
}

/* -------------------------------------------------------------------------- */
/* I2C4 + PCA9555/9535                                                         */
/* -------------------------------------------------------------------------- */

typedef struct
{
    GPIO_TypeDef *Port;
    uint16_t      SclPin;
    uint16_t      SdaPin;
    uint8_t       Alternate;
    const char   *Name;
} I2C4PinSet;

/*
 * Pinos possiveis do I2C4 na STM32H743 (AF conferido pela tabela de alternates).
 * PF0/PF1 NOTAM I2C2 - nao estao nesta lista de proposito.
 */
static const I2C4PinSet i2c4_sets[] =
{
    { GPIOD, GPIO_PIN_12, GPIO_PIN_13, 4U, "PD12/PD13 AF4" },
    { GPIOF, GPIO_PIN_14, GPIO_PIN_15, 4U, "PF14/PF15 AF4" },
    { GPIOH, GPIO_PIN_11, GPIO_PIN_12, 4U, "PH11/PH12 AF4" },
    { GPIOB, GPIO_PIN_6,  GPIO_PIN_7,  6U, "PB6/PB7  AF6" },
    { GPIOB, GPIO_PIN_8,  GPIO_PIN_9,  6U, "PB8/PB9  AF6" },
};

#define I2C4_SET_COUNT  (sizeof(i2c4_sets) / sizeof(i2c4_sets[0]))

static void I2C4_ClearPins(void)
{
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_12 | GPIO_PIN_13);
    HAL_GPIO_DeInit(GPIOF, GPIO_PIN_14 | GPIO_PIN_15);
    HAL_GPIO_DeInit(GPIOH, GPIO_PIN_11 | GPIO_PIN_12);
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_9);
}

static HAL_StatusTypeDef I2C4_InitWithSet(const I2C4PinSet *set)
{
    GPIO_InitTypeDef gpio = {0};

    if (hi2c4.Instance != NULL)
    {
        (void)HAL_I2C_DeInit(&hi2c4);
    }
    I2C4_ClearPins();

    gpio.Pin       = set->SclPin | set->SdaPin;
    gpio.Mode      = GPIO_MODE_AF_OD;   /* I2C = open-drain */
    gpio.Pull      = GPIO_NOPULL;       /* pull-ups fisicos na placa */
    gpio.Speed     = GPIO_SPEED_FREQ_HIGH;
    gpio.Alternate = set->Alternate;
    HAL_GPIO_Init(set->Port, &gpio);

    hi2c4.Instance                   = I2C4;
    hi2c4.Init.Timing                = TEST_I2C4_TIMING_100K;
    hi2c4.Init.OwnAddress1           = 0U;
    hi2c4.Init.AddressingMode        = I2C_ADDRESSINGMODE_7BIT;
    hi2c4.Init.DualAddressMode       = I2C_DUALADDRESS_DISABLE;
    hi2c4.Init.OwnAddress2           = 0U;
    hi2c4.Init.OwnAddress2Masks      = I2C_OA2_NOMASK;
    hi2c4.Init.GeneralCallMode       = I2C_GENERALCALL_DISABLE;
    hi2c4.Init.NoStretchMode         = I2C_NOSTRETCH_DISABLE;

    return HAL_I2C_Init(&hi2c4);
}

static uint8_t I2C4_AutoDetect(void)
{
    static const uint8_t addrs[3] = { PCA_ADDR_0, PCA_ADDR_1, PCA_ADDR_2 };
    uint32_t i;
    uint32_t j;
    uint32_t hits;

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOH_CLK_ENABLE();
    __HAL_RCC_I2C4_CLK_ENABLE();     /* MspInit gerado nao existe: habilita aqui */

    for (i = 0U; i < I2C4_SET_COUNT; i++)
    {
        if (I2C4_InitWithSet(&i2c4_sets[i]) != HAL_OK)
        {
            BoardLog("[I2C4] init falhou em %s\r\n", i2c4_sets[i].Name);
            continue;
        }

        hits = 0U;
        for (j = 0U; j < 3U; j++)
        {
            if (HAL_I2C_IsDeviceReady(&hi2c4, (uint16_t)(addrs[j] << 1), 2U, 20U) == HAL_OK)
            {
                hits++;
            }
        }

        BoardLog("[I2C4] tentativa %s -> %lu PCA(s)\r\n",
                 i2c4_sets[i].Name, (unsigned long)hits);

        if (hits > 0U)
        {
            i2c4_pins = i2c4_sets[i].Name;
            return (uint8_t)hits;
        }
    }

    return 0U;
}

static HAL_StatusTypeDef PCA_Write(uint8_t addr, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return HAL_I2C_Master_Transmit(&hi2c4, (uint16_t)(addr << 1), buf, 2U, 30U);
}

static HAL_StatusTypeDef PCA_Read(uint8_t addr, uint8_t reg, uint8_t *val)
{
    if (HAL_I2C_Master_Transmit(&hi2c4, (uint16_t)(addr << 1), &reg, 1U, 30U) != HAL_OK)
    {
        return HAL_ERROR;
    }
    return HAL_I2C_Master_Receive(&hi2c4, (uint16_t)(addr << 1), val, 1U, 30U);
}

static void PCA_ConfigureAll(void)
{
    static const uint8_t addrs[3] = { PCA_ADDR_0, PCA_ADDR_1, PCA_ADDR_2 };
    uint32_t i;

    for (i = 0U; i < 3U; i++)
    {
        /* Polaridade normal e todas as portas como saida */
        (void)PCA_Write(addrs[i], PCA_REG_POLARITY0, 0x00U);
        (void)PCA_Write(addrs[i], PCA_REG_POLARITY1, 0x00U);
        (void)PCA_Write(addrs[i], PCA_REG_CONFIG0,   0x00U);
        (void)PCA_Write(addrs[i], PCA_REG_CONFIG1,   0x00U);
    }
}

static void PCA_Blink(uint8_t phase)
{
    static const uint8_t addrs[3] = { PCA_ADDR_0, PCA_ADDR_1, PCA_ADDR_2 };
    uint8_t out0 = (phase != 0U) ? 0xAAU : 0x55U;
    uint8_t out1 = (phase != 0U) ? 0x55U : 0xAAU;
    uint32_t i;

#if !PCA_LED_ACTIVE_HIGH
    out0 = (uint8_t)~out0;
    out1 = (uint8_t)~out1;
#endif

    for (i = 0U; i < 3U; i++)
    {
        (void)PCA_Write(addrs[i], PCA_REG_OUTPUT0, out0);
        (void)PCA_Write(addrs[i], PCA_REG_OUTPUT1, out1);
    }
}

static void PCA_Readback(uint8_t addr, uint8_t *v0, uint8_t *v1)
{
    *v0 = 0xFFU;
    *v1 = 0xFFU;
    (void)PCA_Read(addr, PCA_REG_INPUT0, v0);
    (void)PCA_Read(addr, PCA_REG_INPUT1, v1);
}

/* -------------------------------------------------------------------------- */
/* FDCAN1 - loopback interno                                                    */
/* -------------------------------------------------------------------------- */

static HAL_StatusTypeDef CAN_Init(void)
{
    FDCAN_FilterTypeDef filt = {0};
    HAL_StatusTypeDef   st;

    __HAL_RCC_FDCAN_CLK_ENABLE();

    hfdcan1.Instance = FDCAN1;

    /* 64 MHz / (8 * (1 + 11 + 4)) = 500 kbit/s, ponto de amostragem 75% */
    hfdcan1.Init.FrameFormat          = FDCAN_FRAME_CLASSIC;
    hfdcan1.Init.Mode                 = FDCAN_MODE_INTERNAL_LOOPBACK;
    hfdcan1.Init.AutoRetransmission   = ENABLE;
    hfdcan1.Init.TransmitPause        = DISABLE;
    hfdcan1.Init.ProtocolException    = DISABLE;
    hfdcan1.Init.NominalPrescaler     = 8U;
    hfdcan1.Init.NominalSyncJumpWidth = 4U;
    hfdcan1.Init.NominalTimeSeg1      = 11U;
    hfdcan1.Init.NominalTimeSeg2      = 4U;
    hfdcan1.Init.DataPrescaler        = 1U;
    hfdcan1.Init.DataSyncJumpWidth    = 1U;
    hfdcan1.Init.DataTimeSeg1         = 1U;
    hfdcan1.Init.DataTimeSeg2         = 1U;
    hfdcan1.Init.MessageRAMOffset     = 0U;
    hfdcan1.Init.StdFiltersNbr        = 1U;
    hfdcan1.Init.ExtFiltersNbr        = 0U;
    hfdcan1.Init.RxFifo0ElmtsNbr      = 4U;
    hfdcan1.Init.RxFifo0ElmtSize      = FDCAN_DATA_BYTES_8;
    hfdcan1.Init.RxFifo1ElmtsNbr      = 0U;
    hfdcan1.Init.RxFifo1ElmtSize      = FDCAN_DATA_BYTES_8;
    hfdcan1.Init.RxBuffersNbr         = 0U;
    hfdcan1.Init.RxBufferSize         = FDCAN_DATA_BYTES_8;
    hfdcan1.Init.TxEventsNbr          = 0U;
    hfdcan1.Init.TxBuffersNbr         = 0U;
    hfdcan1.Init.TxFifoQueueElmtsNbr  = 4U;
    hfdcan1.Init.TxFifoQueueMode      = FDCAN_TX_FIFO_OPERATION;
    hfdcan1.Init.TxElmtSize           = FDCAN_DATA_BYTES_8;

    st = HAL_FDCAN_Init(&hfdcan1);
    if (st != HAL_OK)
    {
        return st;
    }

    /* Filtro padrao: aceita qualquer ID padrao no FIFO0 */
    filt.IdType       = FDCAN_STANDARD_ID;
    filt.FilterIndex  = 0U;
    filt.FilterType   = FDCAN_FILTER_MASK;
    filt.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filt.FilterID1    = 0U;
    filt.FilterID2    = 0U;
    st = HAL_FDCAN_ConfigFilter(&hfdcan1, &filt);
    if (st != HAL_OK)
    {
        return st;
    }

    st = HAL_FDCAN_ConfigGlobalFilter(&hfdcan1,
                                      FDCAN_ACCEPT_IN_RX_FIFO0,
                                      FDCAN_REJECT,
                                      FDCAN_REJECT_REMOTE,
                                      FDCAN_REJECT_REMOTE);
    if (st != HAL_OK)
    {
        return st;
    }

    st = HAL_FDCAN_Start(&hfdcan1);
    if (st == HAL_OK)
    {
        can_ready = 1U;
    }
    return st;
}

static void CAN_SendAndCheck(void)
{
    static const FDCAN_TxHeaderTypeDef txHdr =
    {
        .Identifier          = TEST_CAN_ID,
        .IdType              = FDCAN_STANDARD_ID,
        .TxFrameType         = FDCAN_DATA_FRAME,
        .DataLength          = FDCAN_DLC_BYTES_8,
        .ErrorStateIndicator = FDCAN_ESI_ACTIVE,
        .BitRateSwitch       = FDCAN_BRS_OFF,
        .FDFormat            = FDCAN_CLASSIC_CAN,
        .TxEventFifoControl  = FDCAN_NO_TX_EVENTS,
        .MessageMarker       = 0U
    };
    static uint8_t counter = 0U;

    uint8_t data[8];
    uint8_t rxData[8];
    FDCAN_RxHeaderTypeDef rxHdr;

    data[0] = 0xA5U;
    data[1] = counter++;
    data[2] = (uint8_t)(HAL_GetTick() >> 8);
    data[3] = (uint8_t)(HAL_GetTick());
    data[4] = 0x10U;
    data[5] = 0x20U;
    data[6] = 0x30U;
    data[7] = 0x40U;

    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &txHdr, data) == HAL_OK)
    {
        can_tx_ok++;
    }
    else
    {
        can_tx_fail++;
    }

    if (HAL_FDCAN_GetRxFifoFillLevel(&hfdcan1, FDCAN_RX_FIFO0) > 0U)
    {
        if (HAL_FDCAN_GetRxMessage(&hfdcan1, FDCAN_RX_FIFO0, &rxHdr, rxData) == HAL_OK)
        {
            can_rx_ok++;
        }
    }
}

/* -------------------------------------------------------------------------- */
/* main                                                                        */
/* -------------------------------------------------------------------------- */

int main(void)
{
    uint32_t t_blink   = 0U;
    uint32_t t_ping    = 0U;
    uint32_t t_can     = 0U;
    uint32_t t_status  = 0U;
    uint8_t  phase     = 0U;
    uint8_t  v0;
    uint8_t  v1;

    HAL_Init();
    SystemClockConfig_Board();

    /* UARTs primeiro, para poder logar enquanto testamos I2C/CAN */
    UART6_Init();
    UART7_Init();

    BoardLog("\r\n============================================================\r\n");
    BoardLog(" STM32H743ZIT6 - validacao de barramentos (sem CubeMX)\r\n");
    BoardLog(" HSI 64MHz -> PLL1 SYSCLK 64MHz | PCLK1/PCLK2 64MHz\r\n");
    BoardLog(" UART6: PC6/PC7 AF7 @ %lu 8N1 | UART7: PE7/PE8 AF7 @ %lu 8N1\r\n",
             TEST_UART_BAUDRATE, TEST_UART_BAUDRATE);
    BoardLog(" Dica: se nao houver eco, inverta o par TX/RX no conector RS232.\r\n");
    BoardLog("============================================================\r\n");

    if (CAN_Init() == HAL_OK)
    {
        BoardLog("[CAN ] FDCAN1 loopback interno 500kbps: OK\r\n");
    }
    else
    {
        BoardLog("[CAN ] FDCAN1: FALHA NA INICIALIZACAO\r\n");
    }

    pca_found = I2C4_AutoDetect();

    if (pca_found == 0U)
    {
        BoardLog("[I2C4] Nenhum PCA em 0x20/0x21/0x23. Confira SCL/SDA e pull-ups.\r\n");
    }
    else
    {
        BoardLog("[I2C4] pinos %s | PCA respondendo: %u\r\n",
                 i2c4_pins, (unsigned int)pca_found);
        PCA_ConfigureAll();

        /* Leitura inicial de confirmacao dos 3 expansores */
        PCA_Readback(PCA_ADDR_0, &v0, &v1);
        BoardLog("[PCA ] 0x20 input0=0x%02X input1=0x%02X\r\n",
                 (unsigned int)v0, (unsigned int)v1);
        PCA_Readback(PCA_ADDR_1, &v0, &v1);
        BoardLog("[PCA ] 0x21 input0=0x%02X input1=0x%02X\r\n",
                 (unsigned int)v0, (unsigned int)v1);
        PCA_Readback(PCA_ADDR_2, &v0, &v1);
        BoardLog("[PCA ] 0x23 input0=0x%02X input1=0x%02X\r\n",
                 (unsigned int)v0, (unsigned int)v1);
    }

    BoardLog("\r\n[LOOP] Pings a cada 1s, LEDs a cada 500ms, status a cada 5s.\r\n");
    BoardLog("[LOOP] Digite nos terminais das duas UARTs para testar o ECO.\r\n\r\n");

    for (;;)
    {
        uint32_t now = HAL_GetTick();

        UART_PollEcho(&huart6, &uart6_rx, &uart6_err);
        UART_PollEcho(&huart7, &uart7_rx, &uart7_err);

        if ((now - t_blink) >= TEST_BLINK_MS)
        {
            t_blink = now;
            phase   = (uint8_t)(phase ^ 1U);
            if (pca_found > 0U)
            {
                PCA_Blink(phase);
            }
        }

        if ((now - t_ping) >= TEST_PING_MS)
        {
            static const char ping6[] = "\r\n[UART6] PING - eco ativo\r\n";
            static const char ping7[] = "\r\n[UART7] PING - eco ativo\r\n";

            t_ping = now;
            if (huart6.Instance != NULL)
            {
                (void)HAL_UART_Transmit(&huart6, (uint8_t *)ping6,
                                        (uint16_t)(sizeof(ping6) - 1U), 50U);
            }
            if (huart7.Instance != NULL)
            {
                (void)HAL_UART_Transmit(&huart7, (uint8_t *)ping7,
                                        (uint16_t)(sizeof(ping7) - 1U), 50U);
            }
        }

        if ((now - t_can) >= TEST_BLINK_MS)
        {
            t_can = now;
            if (can_ready != 0U)
            {
                CAN_SendAndCheck();
            }
        }

        if ((now - t_status) >= TEST_STATUS_MS)
        {
            FDCAN_ErrorCountersTypeDef ec = {0};

            if (can_ready != 0U)
            {
                (void)HAL_FDCAN_GetErrorCounters(&hfdcan1, &ec);
            }

            t_status = now;
            BoardLog("[STAT] U6 rx=%lu err=%lu | U7 rx=%lu err=%lu | "
                     "PCA=%u @%s | CAN tx=%lu/%lu rx=%lu txerr=%lu rxerr=%lu\r\n",
                     (unsigned long)uart6_rx, (unsigned long)uart6_err,
                     (unsigned long)uart7_rx, (unsigned long)uart7_err,
                     (unsigned int)pca_found, i2c4_pins,
                     (unsigned long)can_tx_ok, (unsigned long)can_tx_fail,
                     (unsigned long)can_rx_ok,
                     (unsigned long)ec.TxErrorCnt, (unsigned long)ec.RxErrorCnt);
        }
    }
}
