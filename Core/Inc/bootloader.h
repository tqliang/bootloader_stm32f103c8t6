#ifndef __BOOTLOADER_H
#define __BOOTLOADER_H

#include <stdint.h>
#include "bootloader_config.h"

/* ========================================================================== */
/*                         Bootloader 核心接口                                 */
/* ========================================================================== */

/**
  * @brief  跳转到指定地址的 App
  * @param  app_addr: App 的起始地址 (向量表地址)
  * @note   此函数不会返回
  */
void Bootloader_JumpToApp(uint32_t app_addr);

/**
  * @brief  初始化 Bootloader, 读取分区信息
  * @param  info: 分区信息结构体指针 (输出)
  * @retval 0=分区信息有效, -1=无效 (首次启动)
  */
int Bootloader_ReadPartitionInfo(partition_info_t *info);

/**
  * @brief  保存分区信息到 Flash
  * @param  info: 分区信息结构体指针
  * @retval 0=成功, 非0=失败
  */
int Bootloader_WritePartitionInfo(const partition_info_t *info);

/**
  * @brief  校验并激活新固件
  * @note   对 update_target 分区做 CRC32 校验,
  *         校验通过则更新 active_partition 并清除 update_pending
  * @retval 0=成功, -1=CRC校验失败
  */
int Bootloader_VerifyAndActivate(partition_info_t *info);

/**
  * @brief  校验 App 固件是否有效
  * @param  app_addr:    App 起始地址
  * @param  out_version: 输出版本号 (可为 NULL)
  * @retval 0=有效, -1=无效 (向量表非法或全空)
  */
int Bootloader_ValidateApp(uint32_t app_addr, uint32_t *out_version);

/**
  * @brief  检测另一分区是否有更高版本的新固件, 有则校验并激活
  * @param  info: 分区信息 (输入/输出)
  * @note   比较 active 和非 active 分区的版本号,
  *         若非 active 版本更高, CRC32 校验通过后切换激活分区
  * @retval 0=升级成功, -1=无需升级或升级失败
  */
int Bootloader_CheckAndUpgrade(partition_info_t *info);

/**
  * @brief  尝试回滚: 当前活跃分区无效时, 自动切换到备用分区
  * @param  info:     分区信息 (输入/输出)
  * @param  out_addr: 最终要跳转的地址 (输出)
  * @retval 0=成功 (out_addr 有效), -1=两个分区均无效
  */
int Bootloader_TryRollback(partition_info_t *info, uint32_t *out_addr);

/**
  * @brief  CRC32 计算 (标准 CRC32, 多项式 0x04C11DB7)
  * @param  data: 数据指针
  * @param  len:  数据长度
  * @retval CRC32 值
  */
uint32_t CRC32_Calculate(const uint8_t *data, uint32_t len);

#endif /* __BOOTLOADER_H */