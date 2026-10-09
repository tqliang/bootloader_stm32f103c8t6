#include "bootloader.h"
#include "flash_if.h"
#include "stm32f1xx_hal.h"

/* ========================================================================== */
/*                          CRC32 计算                                        */
/* ========================================================================== */

/* CRC32 查找表 (多项式 0x04C11DB7, 反射输入/输出) */
static const uint32_t crc32_table[256] = 
{
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F,
    0xE963A535, 0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988,
    0x09B64C2B, 0x7EB17CBD, 0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2,
    0xF3B97148, 0x84BE41DE, 0x1ADAD47D, 0x6DDDE4EB, 0xF4D4B551, 0x83D385C7,
    0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC, 0x14015C4F, 0x63066CD9,
    0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4, 0xA2677172,
    0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59,
    0x26D930AC, 0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423,
    0xCFBA9599, 0xB8BDA50F, 0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924,
    0x2F6F7C87, 0x58684C11, 0xC1611DAB, 0xB6662D3D, 0x76DC4190, 0x01DB7106,
    0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F, 0x9FBFE4A5, 0xE8B8D433,
    0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB, 0x086D3D2D,
    0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950,
    0x8BBEB8EA, 0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65,
    0x4DB26158, 0x3AB551CE, 0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7,
    0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A, 0x346ED9FC, 0xAD678846, 0xDA60B8D0,
    0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9, 0x5005713C, 0x270241AA,
    0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409, 0xCE61E49F,
    0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A,
    0xEAD54739, 0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84,
    0x0D6D6A3E, 0x7A6A5AA8, 0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1,
    0xF00F9344, 0x8708A3D2, 0x1E01F268, 0x6906C2FE, 0xF762575D, 0x806567CB,
    0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0, 0x10DA7A5A, 0x67DD4ACC,
    0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8, 0xA1D1937E,
    0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55,
    0x316E8EEF, 0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236,
    0xCC0C7795, 0xBB0B4703, 0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28,
    0x2BB45A92, 0x5CB30A04, 0xC2D7FFA7, 0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D,
    0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A, 0x9C0906A9, 0xEB0E363F,
    0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE, 0x0CB61B38,
    0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777,
    0x88085AE6, 0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69,
    0x616BFFD3, 0x166CCF45, 0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2,
    0xA7672661, 0xD06016F7, 0x4969474D, 0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC,
    0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5, 0x47B2CF7F, 0x30B5FFE9,
    0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605, 0xCDD70693,
    0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D
};

uint32_t CRC32_Calculate(const uint8_t *data, uint32_t len) 
{
    uint32_t crc = 0xFFFFFFFF;
    uint32_t i;
    for (i = 0; i < len; i++) 
    {
        crc = crc32_table[((crc ^ data[i]) & 0xFF)] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

/* ========================================================================== */
/*                      分区信息 读取 / 写入                                   */
/* ========================================================================== */

int Bootloader_ReadPartitionInfo(partition_info_t *info) 
{
    Flash_Read(PARTITION_INFO_ADDR, (uint8_t *)info, sizeof(partition_info_t));

    if (info->magic != PARTITION_MAGIC) // 校验分区信息是否完整
    {
        return -1;
    }
    return 0;
}

int Bootloader_WritePartitionInfo(const partition_info_t *info) // 写入分区信息
{
    if (Flash_ErasePage(PARTITION_INFO_ADDR) != 0) 
    {
        return -1;
    }

    /* 按 32 位写入整个结构体 */
    const uint32_t *src = (const uint32_t *)info;
    uint32_t word_count = sizeof(partition_info_t) / 4;
    uint32_t i;

    Flash_Unlock();
    for (i = 0; i < word_count; i++) // 按 32 位写入每个字
    {
        if (Flash_WriteWord(PARTITION_INFO_ADDR + i * 4, src[i]) != 0) // 写入每个字
        {
            Flash_Lock();
            return -1;
        }
    }
    Flash_Lock();
    return 0;
}

/* ========================================================================== */
/*                         固件校验与激活                                      */
/* ========================================================================== */

int Bootloader_VerifyAndActivate(partition_info_t *info) // 校验并激活
{
    uint32_t app_addr;
    uint32_t expected_size;
    uint32_t expected_crc;
    uint32_t calculated_crc;
    uint32_t *vector_table;

    if (!info->update_pending) 
    {
        return -1;
    }

    /* 确定目标分区的起始地址 */
    if (info->update_target == PARTITION_APP_A) 
    {
        app_addr = APP_A_START;
    } 
    else 
    {
        app_addr = APP_B_START;
    }

    expected_size = info->update_size;
    expected_crc  = info->update_crc;

    /* 检查向量表是否合法 */
    vector_table = (uint32_t *)app_addr;
    /* SP 应在 RAM 范围内 (0x20000000 ~ 0x20005000) */
    if ((vector_table[0] & 0x2FFE0000) != 0x20000000) 
    {
        return -1;
    }
    /* PC (Reset_Handler) 应在 Flash 范围内 */
    if ((vector_table[1] & 0x08000000) != 0x08000000) 
    {
        return -1;
    }
    /* 向量表内不应全是 0xFFFFFFFF */
    if (vector_table[0] == 0xFFFFFFFF && vector_table[1] == 0xFFFFFFFF) 
    {
        return -1;
    }

    /* CRC32 校验 */
    calculated_crc = CRC32_Calculate((const uint8_t *)app_addr, expected_size);

    if (calculated_crc != expected_crc) 
    {
        return -1;
    }

    /* 校验通过, 切换激活分区 */
    info->active_partition = info->update_target;
    info->update_pending   = 0;
    info->update_size      = 0;
    info->update_crc       = 0;

    /* 更新对应分区的大小和 CRC */
    if (info->update_target == PARTITION_APP_A) 
    {
        info->app_a_size = expected_size;
        info->app_a_crc  = expected_crc;
    } 
    else 
    {
        info->app_b_size = expected_size;
        info->app_b_crc  = expected_crc;
    }

    if (Bootloader_WritePartitionInfo(info) != 0) 
    {
        return -1;
    }

    return 0;
}

/* ========================================================================== */
/*                     固件有效性校验                                          */
/* ========================================================================== */

int Bootloader_ValidateApp(uint32_t app_addr, uint32_t *out_version)
{
    uint32_t sp = *(volatile uint32_t *)(app_addr);
    uint32_t pc = *(volatile uint32_t *)(app_addr + 4);

    /* 向量表全空? */
    if (sp == 0xFFFFFFFF && pc == 0xFFFFFFFF) return -1;

    /* SP 必须在 RAM 范围内 */
    if ((sp & 0x2FFE0000) != 0x20000000) return -1;

    /* PC 必须在 Flash 范围内 (含 Thumb 位检查) */
    if ((pc & 0x08000000) != 0x08000000) return -1;

    /* 读出版本号 */
    if (out_version != NULL) 
    {
        *out_version = *(volatile uint32_t *)(app_addr + APP_VERSION_OFFSET);
    }

    return 0;
}

/* ========================================================================== */
/*                 版本比较 + 升级激活                                         */
/* ========================================================================== */

int Bootloader_CheckAndUpgrade(partition_info_t *info)
{
    uint32_t inactive_addr;
    uint32_t active_version, inactive_version;
    uint32_t inactive_crc;

    if (info->active_partition == PARTITION_APP_A) 
    {
        inactive_addr = APP_B_START;
        active_version = info->app_a_version;
    } 
    else 
    {
        inactive_addr = APP_A_START;
        active_version = info->app_b_version;
    }

    /* 校验非活跃分区是否有有效固件 */
    if (Bootloader_ValidateApp(inactive_addr, &inactive_version) != 0) 
    {
        return -1;  /* 对端分区无有效固件 */
    }

    /* 版本号比较: 必须严格大于才升级 */
    if (inactive_version <= active_version) 
    {
        return -1;  /* 版本未更新, 无需升级 */
    }

    /* CRC32 校验非活跃分区整个固件 */
    inactive_crc = CRC32_Calculate((const uint8_t *)inactive_addr, APP_A_SIZE);

    /* 切换激活分区 */
    uint8_t new_active = (info->active_partition == PARTITION_APP_A)
                       ? PARTITION_APP_B : PARTITION_APP_A;

    /* 更新分区信息 */
    if (new_active == PARTITION_APP_A) 
    {
        info->app_a_version = inactive_version;
        info->app_a_crc     = inactive_crc;
        info->app_a_size    = APP_A_SIZE;
    } 
    else 
    {
        info->app_b_version = inactive_version;
        info->app_b_crc     = inactive_crc;
        info->app_b_size    = APP_B_SIZE;
    }

    info->active_partition = new_active; // 切换激活分区
    info->update_pending   = 0;// 清除升级标志位
    info->update_target    = 0;// 清除升级目标
    info->update_size      = 0;// 清除升级大小
    info->update_crc       = 0;// 清除升级 CRC

    if (Bootloader_WritePartitionInfo(info) != 0) 
    {
        return -1;
    }

    return 0;
}

/* ========================================================================== */
/*                    回滚: 活跃分区坏了 → 切回备用                            */
/* ========================================================================== */

int Bootloader_TryRollback(partition_info_t *info, uint32_t *out_addr)
{
    uint32_t active_addr, backup_addr;
    int active_ok, backup_ok;

    if (info->active_partition == PARTITION_APP_A) 
    {
        active_addr = APP_A_START;
        backup_addr = APP_B_START;
    } 
    else 
    {
        active_addr = APP_B_START;
        backup_addr = APP_A_START;
    }

    active_ok = (Bootloader_ValidateApp(active_addr, NULL) == 0);

    if (active_ok) // 活跃分区有效
    {
        *out_addr = active_addr;
        return 0;
    }

    /* 活跃分区坏了, 试备用分区 */
    backup_ok = (Bootloader_ValidateApp(backup_addr, NULL) == 0);

    if (backup_ok) // 备用分区有效
    {
        /* 回滚: 切到备用分区 */
        info->active_partition = (info->active_partition == PARTITION_APP_A)
                               ? PARTITION_APP_B : PARTITION_APP_A;
        info->update_pending = 0;
        Bootloader_WritePartitionInfo(info);
        *out_addr = backup_addr;
        return 0;
    }

    /* 两个分区都坏了 */
    return -1;
}

/* ========================================================================== */
/*                         App 跳转                                           */
/* ========================================================================== */

void Bootloader_JumpToApp(uint32_t app_addr) 
{
    uint32_t sp;
    uint32_t pc;
    void (*app_entry)(void);

    /* 读取向量表: 第 0 字 = 初始 SP, 第 1 字 = Reset_Handler */
    sp = *(volatile uint32_t *)(app_addr);
    pc = *(volatile uint32_t *)(app_addr + 4);

    /* 校验 SP: 必须在 RAM 范围内 */
    if ((sp & 0x2FFE0000) != 0x20000000) 
    {
        return;
    }

    /* 校验 PC: Reset_Handler 地址最低位必须为 1 (Thumb 模式) */
    if ((pc & 0x08000000) != 0x08000000) 
    {
        return;
    }

    app_entry = (void (*)(void))pc;

    /* ---- 以下为跳转前清理, 防止对 App 造成干扰 ---- */

    /* 关闭全局中断 */
    __disable_irq();

    /* 关闭 SysTick */
    SysTick->CTRL  = 0;
    SysTick->LOAD  = 0;
    SysTick->VAL   = 0;

    /* 清除中断使能和挂起寄存器 (NVIC 总共 8 个 32 位寄存器, 覆盖 240 个 IRQ) */
    {
        uint32_t i;
        for (i = 0; i < 8; i++) 
        {
            NVIC->ICER[i] = 0xFFFFFFFF;
            NVIC->ICPR[i] = 0xFFFFFFFF;
        }
    }

    /* 恢复默认的时钟配置 (关闭 PLL, 切换回 HSI, 降低功耗) */
    HAL_RCC_DeInit();

    /* 清除 PendSV */
    SCB->ICSR = SCB_ICSR_PENDSVCLR_Msk;

    /* 设置主堆栈指针 */
    __set_MSP(sp);// 设置主堆栈指针为向量表中的 SP
    __set_PSP(sp);// 设置进程堆栈指针为向量表中的 SP

    /* 跳转到 App */
    app_entry();

    /* 永远不会到达这里 */
}