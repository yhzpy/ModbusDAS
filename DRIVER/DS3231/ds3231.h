/**
 * @file    ds3231.h
 * @brief   DS3231 RTC + AT24C32 EEPROM Driver (I2C1, addr=0x68 / 0x57)
 *
 * DS3231: High-precision RTC (+-2ppm), I2C address 0x68
 * AT24C32: 32Kbit EEPROM on same module, I2C address 0x57
 *
 * Step 1 implementation target.
 */
/* 所有带移位的操作最好都加上括号 所有带移位的操作最好都加上括号 所有带移位的操作最好都加上括号 所有带移位的操作最好都加上括号*/
/*因为移位的优先级很低  & | ~ 这些优先级都比他高  你不加括号就会最后才移位 包出问题的*/
#ifndef __DS3231_H
#define __DS3231_H

#include <stdint.h>

/* I2C Addresses */
#define DS3231_ADDR         0x68
#define AT24C32_ADDR        0x57
/* AT24C32 EEPROM */
#define AT24C32_PAGE_SIZE     32u
#define AT24C32_WRITE_CYCLE   5u    /* tWR: 内部写周期最大 5ms */


/* DS3231 Registers */
#define DS3231_REG_SEC      0x00
#define DS3231_REG_MIN      0x01
#define DS3231_REG_HOUR     0x02
#define DS3231_REG_DAY      0x03    /* Day of week (1-7) */
#define DS3231_REG_DATE     0x04
#define DS3231_REG_MONTH    0x05
#define DS3231_REG_YEAR     0x06
#define DS3231_REG_TEMP_H   0x11    /* Temperature MSB (signed int) */
#define DS3231_REG_TEMP_L   0x12    /* Temperature LSB (upper 2 bits = 0.25C) */
#define DS3231_REG_CTRL     0x0E
#define DS3231_REG_STATUS   0x0F

/* DateTime structure */
typedef struct {
    uint8_t sec;    /* 0-59 */
    uint8_t min;    /* 0-59 */
    uint8_t hour;   /* 0-23 (24h format) */
    uint8_t wday;   /* 1-7 (Mon-Sun) */
    uint8_t mday;   /* 1-31 */
    uint8_t month;  /* 1-12 */
    uint8_t year;   /* 0-99 (2000+year) */
} DS3231_DateTime_t;

/* Return codes */
#define DS3231_OK           0
#define DS3231_ERR_I2C     -1
#define DS3231_ERR_OSF     -2   /* Oscillator Stop Flag = power was lost */

/* bit positions */
#define DS3231_BIT_OSF      ((uint8_t)(1u << 7))  /*~ 优先级高于 <<  如果不给整体加括号  到时候要清位的时候会先取反*/
#define DS3231_BIT_EN32KHZ  ((uint8_t)(1u << 3))

#define DS3231_CTRL_BBSQW ((uint8_t)(1u << 6))  /* 使能电池方波 */
/**
 * @brief  Initialize DS3231 (check OSF flag, disable 32kHz output).
 */
int8_t DS3231_Init(void);

/**
 * @brief  Read current date/time.
 */
int8_t DS3231_GetDateTime(DS3231_DateTime_t *dt);

/**
 * @brief  Set date/time.
 */
int8_t DS3231_SetDateTime(const DS3231_DateTime_t *dt);

/**
 * @brief  Read internal temperature sensor (accuracy +-3 C).
 * @return temperature x100 (e.g. 2775 = 27.75 C)
 */
int16_t DS3231_ReadTemperature(void);

/**
 * @brief  Get Unix timestamp from DS3231 (seconds since 2000-01-01).
 */
uint32_t DS3231_GetTimestamp(void);

/*---- AT24C32 EEPROM Functions ----*/

/**
 * @brief  Write data to AT24C32 (handles page boundary, max 32 bytes/page).
 */
int8_t AT24C32_Write(uint16_t memAddr, const uint8_t *data, uint16_t len);

/**
 * @brief  Read data from AT24C32 (sequential read, any length).
 */
int8_t AT24C32_Read(uint16_t memAddr, uint8_t *data, uint16_t len);

#endif /* __DS3231_H */
