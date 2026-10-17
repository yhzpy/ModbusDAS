/**
 * @file    flash_log.h
 * @brief   W25Q64 Ring Log Layer (wear-leveled circular storage)
 *
 * Storage layout (W25Q64 = 8MB = 128 sectors x 64KB, or 2048 sectors x 4KB):
 *   Sector 0-3 (4KB each): Metadata (magic, write pointer, total count)
 *   Sector 4-103 (100 sectors x 4KB): Ring log data area
 *   Sector 104-127: Reserved (future: config backup, LVGL assets)
 *
 * Record format (16 bytes fixed, 256 records per 4KB sector):
 *   [0-1]   magic 0xDAL0 (Data Acquisition Log)
 *   [2-5]   timestamp (uint32, seconds since 2000-01-01)
 *   [6-7]   temperature (int16, x100)
 *   [8-9]   humidity (uint16, x100)
 *   [10-11] flags (uint16)
 *   [12-13] crc16 (over bytes 0-11)
 *   [14-15] sequence number (uint16, wrapping)
 *
 * Step 2 implementation target.
 */

#ifndef __FLASH_LOG_H
#define __FLASH_LOG_H

#include <stdint.h>

/*============================================================================
 * Constants
 *============================================================================*/
#define FLASH_LOG_RECORD_SIZE       16      /* bytes per record */
#define FLASH_LOG_SECTOR_SIZE       4096    /* W25Q64 sector = 4KB */
#define FLASH_LOG_RECORDS_PER_SEC   256     /* 4096 / 16 */
#define FLASH_LOG_DATA_SECTORS      100     /* sectors 4-103 */
#define FLASH_LOG_MAX_RECORDS       (FLASH_LOG_DATA_SECTORS * FLASH_LOG_RECORDS_PER_SEC)
#define FLASH_LOG_META_SECTOR       0       /* metadata in sector 0 */
#define FLASH_LOG_DATA_START_SEC    4       /* first data sector */

#define FLASH_LOG_MAGIC             0xDA10  /* "DAL" + version 0 */
#define FLASH_LOG_RECORD_MAGIC      0xDA10

/*============================================================================
 * Types
 *============================================================================*/
typedef struct {
    uint16_t magic;         /* 0xDA10 */
    uint32_t timestamp;     /* seconds since 2000-01-01 */
    int16_t  temperature;   /* x100 */
    uint16_t humidity;      /* x100 */
    uint16_t flags;
    uint16_t crc16;         /* CRC over bytes [0..11] */
    uint16_t seq;           /* sequence number (0-65535, wrapping) */
} __attribute__((packed)) FlashLogRecord_t;

typedef struct {
    uint32_t magic;             /* 0xDA10DA10 = valid metadata */
    uint32_t writeSector;       /* current write sector (4-103) */
    uint32_t writeOffset;       /* byte offset within sector (0-4095) */
    uint32_t totalRecords;      /* total records written since boot */
    uint32_t wrapCount;         /* how many times ring has wrapped */
    uint32_t crc32;             /* metadata integrity check */
} FlashLogMeta_t;

/*============================================================================
 * Public API
 *============================================================================*/

/**
 * @brief  Initialize flash log layer.
 *         Scans metadata sector, validates, recovers write pointer.
 *         If metadata invalid (first boot / corruption), format sector 0.
 * @return 0=OK, -1=flash error
 */
int8_t FlashLog_Init(void);

/**
 * @brief  Append one record to the ring log (buffered, not immediately written).
 *         Accumulates in RAM until page (256 bytes) is full, then programs.
 * @param  record: pointer to record data (crc and seq auto-filled)
 * @return 0=OK, -1=flash write error
 */
int8_t FlashLog_Append(const FlashLogRecord_t *record);

/**
 * @brief  Force flush any buffered records to flash.
 *         Call before power-off or after batch accumulation.
 */
int8_t FlashLog_Flush(void);

/**
 * @brief  Read record at absolute index (0 = oldest available).
 * @param  index: 0-based from oldest
 * @param  out: output record
 * @return 0=OK, -1=index out of range, -2=CRC mismatch (corrupt)
 */
int8_t FlashLog_ReadByIndex(uint32_t index, FlashLogRecord_t *out);

/**
 * @brief  Get total number of valid records currently stored.
 */
uint32_t FlashLog_GetCount(void);

/**
 * @brief  Erase all log data sectors (triggered by Modbus command 0x5A5A).
 *         Does NOT erase metadata sector (just resets counters).
 */
int8_t FlashLog_EraseAll(void);

#endif /* __FLASH_LOG_H */
