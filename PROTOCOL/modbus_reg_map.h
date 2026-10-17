/**
 * @file    modbus_reg_map.h
 * @brief   Modbus RTU Register Map Definition for ModbusDAS
 *
 * Register addressing uses Modbus protocol address (0-based).
 * PLC-style "40001" = protocol address 0x0000.
 *
 * All registers are 16-bit (uint16_t), big-endian on wire.
 * Multi-byte values (32-bit) use two consecutive registers:
 *   low word first (little-endian word order).
 *
 * RAM cost: 256 x 2 bytes = 512 bytes (negligible on F103ZET6 64KB SRAM).
 *
 * Access rules enforced in firmware (not by Modbus protocol):
 *   R    = master can read, write attempt returns exception 02
 *   RW   = master can read and write
 *   W    = master can write, read returns 0x0000
 *   RO   = read-only, auto-updated by firmware tasks
 */

#ifndef __MODBUS_REG_MAP_H
#define __MODBUS_REG_MAP_H

#include <stdint.h>

/*============================================================================
 * Register Map Size
 *============================================================================*/
#define MB_REG_COUNT        256     /* Total holding registers in map */
#define MB_REG_BASE         0       /* Protocol address of reg[0] */

/*============================================================================
 * Register Address Definitions (protocol address = array index)
 *
 * Section 1: Live Data (RO, updated by Task_SensorAcq)
 *   Address 0x0000 - 0x000F
 *============================================================================*/
#define MB_REG_TEMPERATURE      0   /* RO, int16, unit: 0.01 C (2534 = 25.34 C) */
#define MB_REG_HUMIDITY         1   /* RO, uint16, unit: 0.01 % (4821 = 48.21 %) */
#define MB_REG_RTC_SEC          2   /* RO, BCD or binary 0-59 */
#define MB_REG_RTC_MIN          3   /* RO, 0-59 */
#define MB_REG_RTC_HOUR         4   /* RO, 0-23 */
#define MB_REG_RTC_DAY          5   /* RO, 1-31 */
#define MB_REG_RTC_MONTH        6   /* RO, 1-12 */
#define MB_REG_RTC_YEAR         7   /* RO, 0-99 (2000+year) */
#define MB_REG_LOG_COUNT_LO     8   /* RO, total log entries [15:0] */
#define MB_REG_LOG_COUNT_HI     9   /* RO, total log entries [31:16] */
#define MB_REG_STATUS           10  /* RO, status bitfield (see below) */
#define MB_REG_UPTIME_LO        11  /* RO, device uptime seconds [15:0] */
#define MB_REG_UPTIME_HI        12  /* RO, device uptime seconds [31:16] */
#define MB_REG_QUEUE_OVF        13  /* RO, sensor queue overflow count */
#define MB_REG_CRC_ERR          14  /* RO, Modbus CRC error count */
#define MB_REG_FRAME_CNT_LO    15  /* RO, Modbus frames received [15:0] */

/*============================================================================
 * Section 2: Configuration (RW, stored in AT24C32 EEPROM on power-off)
 *   Address 0x0064 - 0x006F (decimal 100-111)
 *============================================================================*/
#define MB_REG_SAMPLE_PERIOD    100 /* RW, seconds between acquisitions (1-3600) */
#define MB_REG_SLAVE_ADDR       101 /* RW, Modbus slave address (1-247) */
#define MB_REG_BAUD_CODE        102 /* RW, 0=9600 1=19200 2=38400 3=115200 */
#define MB_REG_LOG_INTERVAL     103 /* RW, how many samples per flash write (1-256) */
#define MB_REG_TEMP_OFFSET      104 /* RW, int16, temperature calibration offset x100 */
#define MB_REG_HUMI_OFFSET      105 /* RW, int16, humidity calibration offset x100 */

/*============================================================================
 * Section 3: Commands (W-only, write triggers action, reads return 0)
 *   Address 0x00C8 - 0x00CF (decimal 200-207)
 *============================================================================*/
#define MB_REG_CMD_CLEAR_LOG    200 /* W, write 0x5A5A to erase all log sectors */
#define MB_REG_CMD_FACTORY_RST  201 /* W, write 0xA5A5 to restore default config */
#define MB_REG_CMD_SYNC_RTC     202 /* W, write 0x1234 to sync RTC from regs 2-7 */
#define MB_REG_CMD_SAVE_CFG     203 /* W, write 0x6789 to save config to EEPROM */
#define MB_REG_CMD_REBOOT       204 /* W, write 0xDEAD to soft-reboot device */

/*============================================================================
 * Command Magic Values
 *============================================================================*/
#define MB_CMD_MAGIC_CLEAR      0x5A5A
#define MB_CMD_MAGIC_FACTORY    0xA5A5
#define MB_CMD_MAGIC_SYNC_RTC   0x1234
#define MB_CMD_MAGIC_SAVE_CFG   0x6789
#define MB_CMD_MAGIC_REBOOT     0xDEAD

/*============================================================================
 * Status Register Bit Definitions (MB_REG_STATUS)
 *============================================================================*/
#define MB_STATUS_SHT30_ERR     (1 << 0)    /* SHT30 I2C read failure */
#define MB_STATUS_DS3231_ERR    (1 << 1)    /* DS3231 I2C read failure */
#define MB_STATUS_FLASH_ERR     (1 << 2)    /* W25Q64 SPI write/read failure */
#define MB_STATUS_EEPROM_ERR    (1 << 3)    /* AT24C32 I2C access failure */
#define MB_STATUS_QUEUE_OVF     (1 << 4)    /* Sensor queue overflowed at least once */
#define MB_STATUS_RS485_TX      (1 << 5)    /* RS485 currently transmitting */
#define MB_STATUS_LOG_FULL      (1 << 6)    /* Flash log wrapped around */
#define MB_STATUS_CFG_DIRTY     (1 << 7)    /* Config modified but not saved to EEPROM */

/*============================================================================
 * Baud Rate Code to Actual Value Mapping
 *============================================================================*/
#define MB_BAUD_9600            0
#define MB_BAUD_19200           1
#define MB_BAUD_38400           2
#define MB_BAUD_115200          3

static const uint32_t MB_BAUD_TABLE[] = { 9600, 19200, 38400, 115200 };
#define MB_BAUD_TABLE_SIZE      (sizeof(MB_BAUD_TABLE) / sizeof(MB_BAUD_TABLE[0]))

/*============================================================================
 * Modbus Function Codes
 *============================================================================*/
#define MB_FUNC_READ_HOLDING    0x03
#define MB_FUNC_READ_INPUT      0x04
#define MB_FUNC_WRITE_SINGLE    0x06
#define MB_FUNC_WRITE_MULTIPLE  0x10

/* Exception Codes */
#define MB_EX_ILLEGAL_FUNC      0x01
#define MB_EX_ILLEGAL_ADDR      0x02
#define MB_EX_ILLEGAL_DATA      0x03
#define MB_EX_SLAVE_FAILURE     0x04

/* Exception response = function code | 0x80 */
#define MB_EXCEPTION_MASK       0x80

/*============================================================================
 * Register Map Structure
 *
 * g_regMap.reg[addr] gives the 16-bit value at protocol address `addr`.
 * Access MUST be protected by xMutexModbusReg (recursive mutex) in RTOS.
 *============================================================================*/
typedef struct {
    uint16_t reg[MB_REG_COUNT];
} ModbusRegMap_t;

/*============================================================================
 * Helper Macros
 *============================================================================*/

/* Pack two uint16 into uint32 (low word first) */
#define MB_REG_GET_U32(map, lo_addr) \
    ((uint32_t)(map)->reg[lo_addr] | ((uint32_t)(map)->reg[(lo_addr)+1] << 16))

/* Split uint32 into two registers (low word first) */
#define MB_REG_SET_U32(map, lo_addr, val) do { \
    (map)->reg[(lo_addr)]   = (uint16_t)((val) & 0xFFFF); \
    (map)->reg[(lo_addr)+1] = (uint16_t)(((val) >> 16) & 0xFFFF); \
} while(0)

/* Check if address is in read-only section */
#define MB_IS_READ_ONLY(addr)   ((addr) < 100)

/* Check if address is command register (write-only) */
#define MB_IS_COMMAND(addr)     ((addr) >= 200 && (addr) <= 204)

/* Check if address is valid */
#define MB_ADDR_VALID(addr)     ((addr) < MB_REG_COUNT)

#endif /* __MODBUS_REG_MAP_H */
