/**
 * @file    max485.h
 * @brief   MAX485/SP3485 RS485 Transceiver HAL Wrapper
 *
 * Hardware: MAX3485 module (3.3V), connected to USART2 (PA2/PA3)
 * Direction control: PD2 (HIGH=TX, LOW=RX), RE+DE tied together
 *
 * This file wraps the direction control and DMA send/receive logic.
 * The actual Modbus protocol lives in PROTOCOL/modbus_rtu.c
 *
 * Step 3 implementation target.
 */

#ifndef __MAX485_H
#define __MAX485_H

#include <stdint.h>

/* Direction control pin */
#define RS485_DE_PORT       GPIOD
#define RS485_DE_PIN        GPIO_PIN_2

/* Direction states */
#define RS485_DIR_RX        0
#define RS485_DIR_TX        1

/**
 * @brief  Initialize RS485 (GPIO direction pin + USART2 DMA + IDLE interrupt).
 *         Call AFTER MX_USART2_UART_Init().
 */
void RS485_Init(void);

/**
 * @brief  Set transceiver direction.
 * @param  dir: RS485_DIR_TX or RS485_DIR_RX
 *
 * IMPORTANT: After last byte transmitted, must wait for USART TC flag
 * before switching to RX. Otherwise last byte gets truncated.
 * TXE != TC: TXE means "data register empty" (next byte can be loaded),
 *            TC means "shift register empty" (all bits clocked out).
 */
void RS485_SetDir(uint8_t dir);

/**
 * @brief  Send data via DMA, wait for TC, then switch back to RX.
 * @param  data: bytes to send
 * @param  len: byte count
 *
 * This is blocking in the sense that it waits for TC before returning.
 * Use HAL_UART_Transmit_DMA + poll TC flag (or DMA TX complete callback).
 */
void RS485_SendData(uint8_t *data, uint16_t len);

/**
 * @brief  Start DMA receive (non-blocking, IDLE interrupt signals completion).
 * @param  buf: receive buffer
 * @param  maxLen: buffer size
 */
void RS485_StartReceive(uint8_t *buf, uint16_t maxLen);

/**
 * @brief  Get received length after IDLE interrupt (stop DMA, read counter).
 * @return number of bytes received
 */
uint16_t RS485_GetReceivedLength(void);

#endif /* __MAX485_H */
