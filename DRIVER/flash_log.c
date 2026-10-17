/**
 * @file    flash_log.c
 * @brief   W25Q64 Ring Log Implementation
 * TODO: Step 2 - implement ring buffer with page-level writes.
 *
 * Depends on: BSP W25Q64 driver (atomic w25qxx.c from Zhengdian Atom)
 *   W25QXX_Read(buf, addr, len)
 *   W25QXX_Write(buf, addr, len)   - page program (max 256 bytes)
 *   W25QXX_Erase_Sector(sector)   - 4KB erase
 */

#include "flash_log.h"
#include <string.h>

/* TODO Step 2: #include "./BSP/W25QXX/w25qxx.h" */

/* Private: RAM page buffer (256 bytes = 16 records = 1 page program) */
static FlashLogRecord_t s_pageBuf[16];
static uint8_t s_pageCount = 0;
static FlashLogMeta_t s_meta;
static uint16_t s_nextSeq = 0;

int8_t FlashLog_Init(void)
{
    /* TODO Step 2:
     * 1. Read sector 0 first 32 bytes -> FlashLogMeta_t
     * 2. Validate magic (0xDA10DA10) and CRC32
     * 3. If invalid: format (erase sector 0, write fresh metadata)
     * 4. If valid: restore writeSector, writeOffset, totalRecords
     * 5. Scan last written sector to find actual next free slot
     *    (in case of power loss between page write and metadata update)
     * 6. Restore s_nextSeq from last valid record
     */
    memset(&s_meta, 0, sizeof(s_meta));
    s_meta.magic = 0xDA10DA10;
    s_meta.writeSector = FLASH_LOG_DATA_START_SEC;
    return 0;
}

int8_t FlashLog_Append(const FlashLogRecord_t *record)
{
    /* TODO Step 2:
     * 1. Copy record into s_pageBuf[s_pageCount]
     * 2. Fill in seq = s_nextSeq++, compute CRC16
     * 3. s_pageCount++
     * 4. If s_pageCount >= 16: call FlashLog_Flush()
     */
    (void)record;
    return 0;
}

int8_t FlashLog_Flush(void)
{
    /* TODO Step 2:
     * 1. If s_pageCount == 0, return immediately
     * 2. Check if current sector is full (writeOffset >= 4096)
     *    If yes: advance to next sector, erase it
     *    If wrapping around: erase next sector before writing
     * 3. W25QXX_Write((uint8_t*)s_pageBuf, flashAddr, s_pageCount * 16)
     * 4. Update writeOffset += s_pageCount * 16
     * 5. Update totalRecords += s_pageCount
     * 6. Write updated metadata to sector 0 (erase + program)
     *    NOTE: metadata update is the atomicity bottleneck.
     *    If power lost here, recovery scan handles it.
     * 7. s_pageCount = 0
     */
    s_pageCount = 0;
    return 0;
}

int8_t FlashLog_ReadByIndex(uint32_t index, FlashLogRecord_t *out)
{
    /* TODO Step 2:
     * 1. Validate index < FlashLog_GetCount()
     * 2. Calculate physical address:
     *    oldest record position = (writePos - count) mod maxRecords
     *    target = oldest + index
     *    sector = DATA_START + (target / RECORDS_PER_SEC)
     *    offset = (target % RECORDS_PER_SEC) * RECORD_SIZE
     * 3. W25QXX_Read((uint8_t*)out, addr, 16)
     * 4. Verify CRC16
     */
    (void)index; (void)out;
    return -1;
}

uint32_t FlashLog_GetCount(void)
{
    /* TODO Step 2: return s_meta.totalRecords (capped at MAX_RECORDS) */
    return 0;
}

int8_t FlashLog_EraseAll(void)
{
    /* TODO Step 2:
     * 1. Erase sectors 4-103 (100 sectors, ~40 seconds total!)
     *    Consider doing this in background task with watchdog kicks.
     * 2. Reset metadata (totalRecords=0, writeSector=4, writeOffset=0)
     * 3. Write fresh metadata to sector 0
     */
    return 0;
}
