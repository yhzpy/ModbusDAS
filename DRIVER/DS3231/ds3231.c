/**
 * @file    ds3231.c
 * @brief   DS3231 RTC + AT24C32 EEPROM Driver Implementation
 * TODO: Step 1 - implement I2C transactions.
 */

#include "ds3231.h"

/* TODO Step 1: #include "i2c.h" for hi2c1 */

/* BCD conversion helpers */
static uint8_t bcd2bin(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
static uint8_t bin2bcd(uint8_t bin) { return ((bin / 10) << 4) | (bin % 10); }

int8_t DS3231_Init(void)
{
    /* TODO Step 1:
     * 1. Read status register (0x0F)
     * 2. Check OSF bit (bit 7): if set, time is invalid (battery was removed)
     * 3. Clear OSF bit
     * 4. Disable 32kHz output (clear bit 3 of status reg) - save power
     * 5. Ensure control reg (0x0E) = 0x1C (BBSQW=1, others default)
     */
    return DS3231_OK;
}

int8_t DS3231_GetDateTime(DS3231_DateTime_t *dt)
{
    /* TODO Step 1:
     * 1. Read registers 0x00-0x06 (7 bytes)
     * 2. Convert BCD to binary:
     *    dt->sec   = bcd2bin(buf[0] & 0x7F)
     *    dt->min   = bcd2bin(buf[1] & 0x7F)
     *    dt->hour  = bcd2bin(buf[2] & 0x3F)  // 24h mode
     *    dt->wday  = bcd2bin(buf[3] & 0x07)
     *    dt->mday  = bcd2bin(buf[4] & 0x3F)
     *    dt->month = bcd2bin(buf[5] & 0x1F)  // bit7 = century, ignore
     *    dt->year  = bcd2bin(buf[6])
     */
    dt->sec = 0; dt->min = 0; dt->hour = 0;
    dt->wday = 1; dt->mday = 1; dt->month = 1; dt->year = 26;
    return DS3231_OK;
}

int8_t DS3231_SetDateTime(const DS3231_DateTime_t *dt)
{
    /* TODO Step 1:
     * 1. Convert binary to BCD
     * 2. Write registers 0x00-0x06 (7 bytes in one I2C transaction)
     * 3. Hour register: clear bit6 (force 24h mode)
     */
    (void)dt;
    return DS3231_OK;
}

int16_t DS3231_ReadTemperature(void)
{
    /* TODO Step 1:
     * 1. Read registers 0x11-0x12 (2 bytes)
     * 2. temp_msb is signed int8 (two's complement)
     * 3. temp_lsb upper 2 bits = fractional (0.25 C per bit)
     * 4. return (int16_t)(temp_msb * 100 + (temp_lsb >> 6) * 25)
     */
    return 2500; /* placeholder: 25.00 C */
}

uint32_t DS3231_GetTimestamp(void)
{
    /* TODO Step 1:
     * Convert DateTime to seconds since 2000-01-01 00:00:00
     * Simple days-since-epoch calculation (no need for full mktime)
     */
    return 0; /* placeholder */
}

/*---- AT24C32 EEPROM ----*/

int8_t AT24C32_Write(uint16_t memAddr, const uint8_t *data, uint16_t len)
{
    /* TODO Step 1:
     * AT24C32 page size = 32 bytes
     * 1. Split write into page-aligned chunks
     * 2. For each chunk:
     *    a. Start condition, send addr 0x57 (write)
     *    b. Send 16-bit memory address (MSB first)
     *    c. Send up to 32 data bytes (must not cross page boundary!)
     *    d. Stop condition
     *    e. Wait 5ms for internal write cycle (or ACK polling)
     */
    (void)memAddr; (void)data; (void)len;
    return DS3231_OK;
}

int8_t AT24C32_Read(uint16_t memAddr, uint8_t *data, uint16_t len)
{
    /* TODO Step 1:
     * Sequential read (no page limit):
     * 1. Start, addr 0x57 (write), send 16-bit memory address, repeated start
     * 2. Addr 0x57 (read), clock out `len` bytes with ACK, NACK on last byte
     * 3. Stop
     */
    (void)memAddr; (void)data; (void)len;
    return DS3231_OK;
}
