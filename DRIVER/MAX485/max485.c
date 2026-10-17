/**
 * @file    max485.c
 * @brief   MAX485 RS485 Transceiver Implementation
 * TODO: Step 3 - implement DMA TX/RX with direction control.
 */

#include "max485.h"
#include "delay.h"
/* TODO Step 3: #include "usart.h" for huart2, hdma_usart2_rx, hdma_usart2_tx */

static uint8_t *s_rxBuf = NULL;
static uint16_t s_rxMaxLen = 0;

void RS485_Init(void)
{
    /* TODO Step 3:
     * 1. PD2 as push-pull output, default LOW (receive mode)
     * 2. Enable USART2 DMA RX (circular or normal mode, buffer 256 bytes)
     * 3. Enable USART2 DMA TX (normal mode)
     * 4. Enable USART2 IDLE interrupt in NVIC (priority >= 12 for FromISR safety)
     * 5. Start DMA receive
     *
     * NVIC priority note:
     *   configMAX_SYSCALL_INTERRUPT_PRIORITY = 191 (priority 11)
     *   ISR must use priority 12-15 to safely call FromISR functions.
     *   Set USART2_IRQn priority to 13 (raw 208).
     */
    RS485_SetDir(RS485_DIR_RX);
}

void RS485_SetDir(uint8_t dir)
{
    /* TODO Step 3:
     * HAL_GPIO_WritePin(RS485_DE_PORT, RS485_DE_PIN,
     *                   dir == RS485_DIR_TX ? GPIO_PIN_SET : GPIO_PIN_RESET);
     */
    (void)dir;
}

void RS485_SendData(uint8_t *data, uint16_t len)
{
    /* TODO Step 3:
     * 1. RS485_SetDir(RS485_DIR_TX)
     * 2. HAL_UART_Transmit_DMA(&huart2, data, len)
     * 3. Wait for DMA TX complete (or poll TC flag)
     * 4. while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) == RESET);
     * 5. RS485_SetDir(RS485_DIR_RX)
     *
     * CRITICAL: Step 4 must check TC not TXE!
     * TXE = "can load next byte", TC = "all bits shifted out".
     * If you switch to RX on TXE, the last byte is truncated.
     */
    (void)data; (void)len;
}

void RS485_StartReceive(uint8_t *buf, uint16_t maxLen)
{
    s_rxBuf = buf;
    s_rxMaxLen = maxLen;
    /* TODO Step 3:
     * HAL_UART_Receive_DMA(&huart2, buf, maxLen);
     */
}

uint16_t RS485_GetReceivedLength(void)
{
    /* TODO Step 3:
     * return s_rxMaxLen - __HAL_DMA_GET_COUNTER(huart2.hdmarx);
     */
    return 0;
}
