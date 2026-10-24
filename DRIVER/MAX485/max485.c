/**
 * @file    max485.c
 * @brief   MAX485 RS485 Transceiver Implementation (Step 3 done)
 *
 * USART2 时钟/PA2/PA3/DMA/NVIC 由 Core/Src/usart.c 的 MspInit 完成，
 * 本文件只负责 PD2 方向控制 + DMA 收发封装。
 */

#include "max485.h"
#include "usart.h"

static uint8_t *s_rxBuf = NULL;
static uint16_t s_rxMaxLen = 0;

void RS485_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 1. PD2 方向脚：推挽输出，默认 LOW = 接收（上电不占用总线） */
    __HAL_RCC_GPIOD_CLK_ENABLE();
    GPIO_InitStruct.Pin   = RS485_DE_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(RS485_DE_PORT, &GPIO_InitStruct);

    RS485_SetDir(RS485_DIR_RX);
}

void RS485_SetDir(uint8_t dir)
{
    HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN,
                      dir == RS485_DIR_TX ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void RS485_SendData(uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0) return;

    RS485_SetDir(RS485_DIR_TX);
    if (HAL_UART_Transmit_DMA(&huart2, data, len) != HAL_OK)
    {
        RS485_SetDir(RS485_DIR_RX);     /* 启动失败，立刻回接收态 */
        return;
    }
    /* 等 HAL 状态机走完：DMA TC 中断 → 等 USART TC → gState 回 READY。
     * 不能直接轮询 TC 标志：USART2_IRQHandler 里的 HAL_UART_IRQHandler
     * 会在 TC 中断中把 TC 清掉，轮询标志会死等。
     * TC 置位代表最后一字节已完全移出，此刻才能切回接收，否则丢尾字节。 */
    while (huart2.gState != HAL_UART_STATE_READY);
    RS485_SetDir(RS485_DIR_RX);
}

void RS485_StartReceive(uint8_t *buf, uint16_t maxLen)
{
    s_rxBuf = buf;
    s_rxMaxLen = maxLen;
    HAL_UART_Receive_DMA(&huart2, buf, maxLen);
}

uint16_t RS485_GetReceivedLength(void)
{
    if (s_rxBuf == NULL || s_rxMaxLen == 0) return 0;
    /* CNDTR 是递减计数：总数 - 剩余 = 已收字节数 */
    return (uint16_t)(s_rxMaxLen - __HAL_DMA_GET_COUNTER(huart2.hdmarx));
}

//                           MAX3485
//   STM32 (USART2)   ┌─────────────────┐          RS485 总线
//                    │                 │
//   PA2 (TX) ────────▶ DI   驱动器 ──────▶ A ────┐
//                    │    (DE 控制)    │  B ────┤ 双绞线 → 主站/其他从站
//   PA3 (RX) ◀──────── RO  接收器 ◀────── A ────┘
//                    │    (RE 控制)    │  B
//   PD2 ─────────────▶ DE/RE          │
//                    └─────────────────┘