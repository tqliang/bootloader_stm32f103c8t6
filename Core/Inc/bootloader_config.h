#ifndef __BOOTLOADER_CONFIG_H
#define __BOOTLOADER_CONFIG_H

#include <stdint.h>

/* ========================================================================== */
/*                          Flash 分区定义                                     */
/* ========================================================================== */

/* STM32F103C8T6 Flash 总容量: 64KB, 每页 1KB, 共 64 页 */
/* Bootloader 占用前 16KB, App A 和 App B 各占 24KB      */

/* FLASH_BASE 和 FLASH_PAGE_SIZE 已在 HAL 库 (stm32f103xb.h) 中定义 */

/* Bootloader 区: 0x08000000 ~ 0x08003FFF (16KB, 0~15 页) */
#define BOOTLOADER_START        FLASH_BASE
#define BOOTLOADER_SIZE         (16 * 1024)

/* App A 区: 0x08004000 ~ 0x08009FFF (24KB, 16~39 页) */
#define APP_A_START             0x08004000U
#define APP_A_SIZE              (24 * 1024)

/* App B 区: 0x0800A000 ~ 0x0800FFFF (24KB, 40~63 页) */
#define APP_B_START             0x0800A000U
#define APP_B_SIZE              (24 * 1024)

/* ========================================================================== */
/*                        分区信息存储位置                                      */
/* ========================================================================== */

/* 分区信息表存放在 Bootloader 最后一页 (0x08003C00), 前面是代码区 */
#define PARTITION_INFO_ADDR     0x08003C00U
#define PARTITION_INFO_PAGE     15              /* 第 15 页 (0-based) */

/* ========================================================================== */
/*                         分区信息结构体                                      */
/* ========================================================================== */

/* App 版本号在固件内的偏移地址 (紧跟向量表之后) */
#define APP_VERSION_OFFSET       0x10C           /* 版本号 uint32_t, 紧贴向量表之后, 偏移 268 字节 */

#define PARTITION_MAGIC         0xAA55AA55U     /* 魔数: 判断数据有效性 */
#define PARTITION_APP_A         0               /* App A 索引 */
#define PARTITION_APP_B         1               /* App B 索引 */

typedef struct {
    uint32_t magic;              /* 魔数 PARTITION_MAGIC */
    uint8_t  active_partition;   /* 当前激活分区: 0=AppA, 1=AppB */
    uint8_t  reserved1[3];       /* 对齐保留 */

    /* App A 信息 */
    uint32_t app_a_crc;          /* App A 固件 CRC32 */
    uint32_t app_a_size;         /* App A 固件大小 (字节) */
    uint32_t app_a_version;      /* App A 版本号 */

    /* App B 信息 */
    uint32_t app_b_crc;          /* App B 固件 CRC32 */
    uint32_t app_b_size;         /* App B 固件大小 (字节) */
    uint32_t app_b_version;      /* App B 版本号 */

    /* 待升级固件信息 */
    uint8_t  update_pending;     /* 1 = 有待升级固件 */
    uint8_t  update_target;      /* 升级目标分区: 0=AppA, 1=AppB */
    uint8_t  reserved2[2];       /* 对齐保留 */
    uint32_t update_size;        /* 新固件大小 */
    uint32_t update_crc;         /* 新固件 CRC32 */
} partition_info_t;

#endif /* __BOOTLOADER_CONFIG_H */