/**
 * @file    modbus_rtu.h
 * @brief   Modbus RTU Slave Protocol Stack (hand-written, no FreeMODBUS)
 *
 * Implements:
 *   - CRC-16/Modbus (polynomial 0xA001, init 0xFFFF)
 *   - Frame parsing: addr + func + data + CRC
 *   - Function codes: 0x03 (read holding), 0x06 (write single), 0x10 (write multiple)
 *   - Exception responses
 *   - T3.5 inter-frame timeout detection (via TIM6 + DMA IDLE)
 *
 * Step 3 implementation target.
 */

#ifndef __MODBUS_RTU_H
#define __MODBUS_RTU_H

#include <stdint.h>
#include "modbus_reg_map.h"

/*============================================================================
 * Configuration
 *============================================================================*/
#define MB_RTU_RX_BUF_SIZE  256
#define MB_RTU_TX_BUF_SIZE  256
#define MB_RTU_MAX_REGS     125     /* Modbus spec: max 125 registers per read */

/*============================================================================
 * Types
 *============================================================================*/
typedef enum {
    MB_OK = 0,
    MB_ERR_CRC,
    MB_ERR_ADDR,
    MB_ERR_FUNC,
    MB_ERR_DATA,
    MB_ERR_TIMEOUT,
} MB_Status_t;

/*============================================================================
 * Public API
 *============================================================================*/

/**
 * @brief  Initialize Modbus RTU slave (USART2 + DMA + TIM6).
 * @param  slaveAddr: this device's Modbus address (1-247)
 * @param  regMap: pointer to global register map
 */
void ModbusRTU_Init(uint8_t slaveAddr, ModbusRegMap_t *regMap);

/**
 * @brief  Process a received frame. Call from Task_Modbus after DMA IDLE.
 * @param  rxBuf: received bytes
 * @param  rxLen: number of bytes received
 * @param  txBuf: output buffer for response
 * @param  txLen: output response length
 * @return MB_OK if response generated, MB_ERR_* if frame should be discarded
 */
MB_Status_t ModbusRTU_ProcessFrame(uint8_t *rxBuf, uint16_t rxLen,
                                    uint8_t *txBuf, uint16_t *txLen);

/**
 * @brief  Compute CRC-16/Modbus over a buffer.
 * @param  data: input bytes
 * @param  len: byte count
 * @return CRC-16 value (low byte sent first on wire)
 */
uint16_t ModbusRTU_CRC16(const uint8_t *data, uint16_t len);

/**
 * @brief  RS485 direction control (MAX485 DE/RE pin).
 * @param  tx: 1=transmit mode, 0=receive mode
 */
void RS485_SetDirection(uint8_t tx);

/**
 * @brief  Called from TIM6 update ISR when T3.5 expires (frame complete).
 *         Notifies Task_Modbus via task notification.
 */
void ModbusRTU_T35_TimeoutISR(void);

/**
 * @brief  Called from USART2 IDLE interrupt.
 *         Stops DMA, calculates length, notifies Task_Modbus.
 */
void ModbusRTU_UART_IDLE_ISR(void);

#endif /* __MODBUS_RTU_H */
