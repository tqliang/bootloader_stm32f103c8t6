#include "flash_if.h"
#include "stm32f1xx_hal.h"

/* ========================================================================== */
/*                          Flash 解锁 / 锁定                                  */
/* ========================================================================== */

void Flash_Unlock(void) 
{
    HAL_FLASH_Unlock();
}

void Flash_Lock(void) 
{
    HAL_FLASH_Lock();
}

/* ========================================================================== */
/*                          Flash 擦除                                        */
/* ========================================================================== */

int Flash_ErasePage(uint32_t page_addr) 
{
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error;

    erase_init.TypeErase   = FLASH_TYPEERASE_PAGES;
    erase_init.PageAddress = page_addr;
    erase_init.NbPages     = 1;

    if (HAL_FLASHEx_Erase(&erase_init, &page_error) != HAL_OK) {
        return -1;
    }
    return 0;
}

int Flash_EraseRange(uint32_t start_addr, uint32_t size) 
{
    uint32_t end_addr = start_addr + size;
    uint32_t page_mask = ~(FLASH_PAGE_SIZE - 1);

    start_addr &= page_mask;

    while (start_addr < end_addr) 
    {
        if (Flash_ErasePage(start_addr) != 0) 
        {
            return -1;
        }
        start_addr += FLASH_PAGE_SIZE;
    }
    return 0;
}

/* ========================================================================== */
/*                          Flash 写入                                        */
/* ========================================================================== */

int Flash_WriteWord(uint32_t addr, uint32_t data) 
{
    uint16_t low  = (uint16_t)(data & 0xFFFF);
    uint16_t high = (uint16_t)((data >> 16) & 0xFFFF);

    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr, low) != HAL_OK) 
    {
        return -1;
    }
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + 2, high) != HAL_OK) 
    {
        return -1;
    }
    return 0;
}

int Flash_Write(uint32_t addr, const uint8_t *buf, uint32_t len) 
{
    uint32_t aligned_len;
    uint32_t i;
    uint32_t word;
    uint16_t single;

    /* 先按 32 位对齐写入 */
    aligned_len = len & ~0x3U;
    for (i = 0; i < aligned_len; i += 4) 
    {
        word = ((uint32_t)buf[i]) | ((uint32_t)buf[i + 1] << 8) | ((uint32_t)buf[i + 2] << 16) | ((uint32_t)buf[i + 3] << 24);

        if (Flash_WriteWord(addr + i, word) != 0) 
        {
            return -1;
        }
    }

    /* 处理剩余的未对齐字节 (1~3 字节) */
    if (i < len) {
        single = 0xFFFF;
        if (len - i >= 2) 
        {
            single = (uint16_t)buf[i] | ((uint16_t)buf[i + 1] << 8);
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i, single) != HAL_OK) 
            {
                return -1;
            }
            i += 2;
        }
        /* 最后 1 字节 */
        if (i < len) 
        {
            single = buf[i];
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i, single) != HAL_OK) 
            {
                return -1;
            }
        }
    }

    return 0;
}

/* ========================================================================== */
/*                          Flash 读取                                        */
/* ========================================================================== */

void Flash_Read(uint32_t addr, uint8_t *buf, uint32_t len) 
{
    uint32_t i;
    for (i = 0; i < len; i++) 
    {
        buf[i] = *((volatile uint8_t *)(addr + i));
    }
}