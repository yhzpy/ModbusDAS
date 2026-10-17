/**
 * @file    sht30.h
 * @brief   SHT30 Temperature/Humidity Sensor Driver (I2C1, addr=0x44)
 *
 * Hardware: SHT30-DIS module, I2C address 0x44 (ADDR pin LOW)
 * Protocol: Single-shot measurement, clock stretching enabled
 * CRC-8 polynomial: 0x31, init: 0xFF
 *
 * Step 1 implementation target.
 */

#ifndef __SHT30_H
#define __SHT30_H

#include <stdint.h>

/* I2C Address */
#define SHT30_ADDR          0x44

/* Commands (16-bit, MSB first) */
#define SHT30_CMD_MEAS_HIGH     0x2400  /* High repeatability, no stretch */
#define SHT30_CMD_MEAS_MED      0x240B  /* Medium repeatability */
#define SHT30_CMD_MEAS_LOW      0x2416  /* Low repeatability */
#define SHT30_CMD_MEAS_STRETCH  0x2C06  /* High repeatability, clock stretch */
#define SHT30_CMD_READ_STATUS   0xF32D
#define SHT30_CMD_CLEAR_STATUS  0x3041
#define SHT30_CMD_SOFT_RESET    0x30A2
#define SHT30_CMD_HEATER_ON     0x306D
#define SHT30_CMD_HEATER_OFF    0x3066

/* Return codes */
#define SHT30_OK            0
#define SHT30_ERR_I2C      -1
#define SHT30_ERR_CRC      -2
#define SHT30_ERR_TIMEOUT  -3

/**
 * @brief  Initialize SHT30 (soft reset + verify WHO_AM_I via status register).
 * @return SHT30_OK or error code
 */
int8_t SHT30_Init(void);

/**
 * @brief  Read temperature and humidity (single-shot, high repeatability).
 * @param  temp_x100: output, temperature * 100 (e.g. 2534 = 25.34 C)
 * @param  humi_x100: output, humidity * 100 (e.g. 4821 = 48.21%)
 * @return SHT30_OK or error code
 *
 * Conversion formulas (from SHT30 datasheet):
 *   T [C] = -45 + 175 * raw / 65535
 *   RH [%] = 100 * raw / 65535
 */
int8_t SHT30_Read(int16_t *temp_x100, uint16_t *humi_x100);

/**
 * @brief  Compute CRC-8 for SHT30 (poly=0x31, init=0xFF).
 */
uint8_t SHT30_CRC8(const uint8_t *data, uint8_t len);

#endif /* __SHT30_H */
