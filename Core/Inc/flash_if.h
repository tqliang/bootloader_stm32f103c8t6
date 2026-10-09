#ifndef __FLASH_IF_H
#define __FLASH_IF_H

#include <stdint.h>

/* ========================================================================== */
/*                          Flash 操作接口                                     */
/* ========================================================================== */

/**
  * @brief  解锁 Flash 控制器
  */
void Flash_Unlock(void);

/**
  * @brief  锁定 Flash 控制器
  */
void Flash_Lock(void);

/**
  * @brief  擦除指定页 (1KB)
  * @param  page_addr: 页起始地址 (必须 1KB 对齐)
  * @retval 0=成功, 非0=失败
  */
int Flash_ErasePage(uint32_t page_addr);

/**
  * @brief  擦除指定地址范围的所有页
  * @param  start_addr: 起始地址 (自动对齐到页边界)
  * @param  size: 擦除大小 (字节)
  * @retval 0=成功, 非0=失败
  */
int Flash_EraseRange(uint32_t start_addr, uint32_t size);

/**
  * @brief  写入 32 位数据到 Flash (自动拆为 2 次半字写入)
  * @param  addr: 目标地址 (必须 2 字节对齐)
  * @param  data: 32 位数据
  * @retval 0=成功, 非0=失败
  */
int Flash_WriteWord(uint32_t addr, uint32_t data);

/**
  * @brief  批量写入数据到 Flash
  * @param  addr: 目标起始地址 (必须 2 字节对齐)
  * @param  buf:  数据缓冲区
  * @param  len:  数据长度 (字节)
  * @retval 0=成功, 非0=失败
  */
int Flash_Write(uint32_t addr, const uint8_t *buf, uint32_t len);

/**
  * @brief  从 Flash 读取数据
  * @param  addr: 源地址
  * @param  buf:  接收缓冲区
  * @param  len:  读取长度 (字节)
  */
void Flash_Read(uint32_t addr, uint8_t *buf, uint32_t len);

#endif /* __FLASH_IF_H */