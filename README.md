# STM32F103C8T6 Bootloader

基于 STM32CubeMX + HAL 库的双 App 分区 Bootloader，支持固件升级和分区切换。

## 硬件平台

- **MCU**: STM32F103C8T6 (Cortex-M3, 64KB Flash, 20KB RAM)
- **时钟**: HSE 8MHz, PLL x9 = 72MHz
- **工具链**: GCC ARM (arm-none-eabi-) + CMake + Ninja

## Flash 分区

```
0x08010000 ┌──────────────────────┐  64KB
           │   App B (备份区)     │  24KB  (页 40 ~ 63)
0x0800A000 ├──────────────────────┤
           │   App A (主运行区)   │  24KB  (页 16 ~ 39)
0x08004000 ├──────────────────────┤
           │   分区信息表          │  1KB   (页 15, 0x08003C00)
0x08003C00 ├──────────────────────┤
           │   Bootloader 代码    │  15KB  (页 0 ~ 14)
0x08000000 └──────────────────────┘
```

| 分区 | 起始地址 | 大小 | 说明 |
|------|----------|------|------|
| Bootloader | `0x08000000` | 16KB | 固件升级 + App 跳转 |
| 分区信息 | `0x08003C00` | 1KB | 存储 `partition_info_t` 结构体 |
| App A | `0x08004000` | 24KB | 主运行区 |
| App B | `0x0800A000` | 24KB | 备份区 (升级目标) |

## 编译

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -G "Ninja"
cmake --build build
```

输出文件: `build/bootloader_stm32f103c8t6.elf` / `.hex` / `.bin`

## 烧录

使用 ST-Link / J-Link / 串口 ISP 将 Bootloader 烧录到 `0x08000000`:

```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
        -c "program build/bootloader_stm32f103c8t6.elf verify reset exit"
```

## 启动流程

```
上电 / 复位
    │
    ▼
Bootloader 启动
    │
    ├── GPIO 触发升级? ──YES──▶ 升级模式 (版本检测 + 激活)
    ├── update_pending? ──YES──▶ 升级模式
    │
    └── 正常启动
         │
         ├── 步骤1: 回滚检测
         │   ├── 活跃 App 有效 ──▶ 继续
         │   └── 活跃 App 损坏 ──▶ 自动切回备用 App (回滚)
         │
         ├── 步骤2: 自动升级检测
         │   检测另一分区版本 > 当前版本 → CRC32 校验 → 激活
         │
         └── 步骤3: 跳转活跃 App
```

## 升级流程 (UpdateMode 详细)

```
进入升级模式 (GPIO 触发 或 update_pending)
    │
    ├── 读取分区信息
    │
    ├── 确定活跃分区 (App A 或 App B)
    │
    ├── 检测另一分区
    │   ├── 向量表合法? ──YES──▶ 读版本号
    │   └── 向量表非法 ──▶ 无新固件 → 慢闪等待烧录
    │
    ├── 版本比较
    │   ├── 新版本 > 旧版本 ──YES──▶ CRC32 校验全分区
    │   └── 新版本 ≤ 旧版本 ──▶ 无需升级 → 慢闪等待
    │
    ├── CRC32 校验
    │   ├── 通过 ──▶ 激活新分区 (写 partition_info)
    │   └── 失败 ──▶ 升级失败 → 慢闪等待 (旧分区不受影响)
    │
    └── 跳转新固件 (快闪 3 次)
```

## 断电保护机制

升级过程中任何时刻断电都**不会变砖**:

| 断电时机 | 影响 |
|----------|------|
| 写入另一分区时 | 被写入的分区有脏数据 → CRC32 校验失败 → 自动回滚旧 App |
| 激活前 (写 partition_info 之前) | `active_partition` 未变 → 重启后仍在旧 App |
| 激活后 (写 partition_info 之后) | 新 App 已激活, 重启后尝试启动新 App |
| 新 App 启动后崩溃 | 下次上电 → `TryRollback()` 检测到活跃 App 损坏 → 自动切回备用 App |

> **关键**: 升级先写非活跃分区, 校验通过后才修改 `active_partition`。旧固件始终完好保留, 直到新固件确认可用。

## LED 指示灯

| 看到什么 | 含义 |
|----------|------|
| 上电亮 100ms → 灭 → 快闪 3 次 (50ms) → App 运行 | ✅ 正常跳转成功 |
| 上电亮 100ms → 灭 → 亮 200ms → 慢闪 (500ms) | 🔧 升级模式, 等待新固件 |
| 上电亮 100ms → 灭 → 快闪 3 次 (100ms) → 快闪 (150ms) | ❌ 无有效 App 或回滚失败 |

## 升级触发方式

| 方式 | 条件 | 说明 |
|------|------|------|
| GPIO 引脚 | PA0 高电平 | 上电时按住按键强制进入升级模式 |
| 分区标记 | `update_pending == 1` | 上次升级未完成, 继续升级流程 |

> 分区信息无效时**不再**直接进升级模式, 而是自动检测 App 是否存在 (见启动流程)。

## 首次使用指南

**烧录 Bootloader + App 后无需任何额外操作**, Bootloader 会自动完成初始化:

1. 烧录 Bootloader 到 `0x08000000`
2. 烧录 App 到 `0x08004000`
3. 上电 → Bootloader 检测到 App A 向量表合法 → 自动创建分区信息 → 跳转 App A

之后每次上电, Bootloader 读取分区信息直接跳转, 耗时极短 (< 1ms)。

> App 中**不需要**硬编码任何分区信息, `partition_info_t` 由 Bootloader 全权管理。

## 文件结构

```
bootloader_stm32f103c8t6/
├── STM32F103XX_FLASH.ld        # 链接脚本 (FLASH=16K)
├── CMakeLists.txt              # 根 CMake
├── startup_stm32f103xb.s       # 启动文件
├── cmake/
│   ├── gcc-arm-none-eabi.cmake # 工具链配置
│   └── stm32cubemx/
│       └── CMakeLists.txt      # HAL 库源码编译
├── Core/
│   ├── Inc/
│   │   ├── main.h              # 主头文件
│   │   ├── bootloader_config.h # ★ 分区定义 & 分区信息结构体
│   │   ├── bootloader.h        # ★ Bootloader 核心接口
│   │   ├── flash_if.h          # ★ Flash 操作接口
│   │   ├── stm32f1xx_hal_conf.h
│   │   └── stm32f1xx_it.h
│   └── Src/
│       ├── main.c              # ★ Bootloader 主逻辑
│       ├── bootloader.c        # ★ 跳转 + 分区管理 + CRC32
│       ├── flash_if.c          # ★ Flash 擦/写/读
│       ├── stm32f1xx_it.c
│       ├── stm32f1xx_hal_msp.c
│       ├── system_stm32f1xx.c
│       ├── syscalls.c
│       └── sysmem.c
└── Drivers/                    # STM32 HAL 库 (CMSIS + HAL)
```

## 核心 API

### Bootloader 接口 ([bootloader.h](Core/Inc/bootloader.h))

| 函数 | 功能 |
|------|------|
| `Bootloader_JumpToApp(app_addr)` | 跳转到指定 App (关中断→关 SysTick→清 NVIC→设 MSP→跳转) |
| `Bootloader_ReadPartitionInfo(info)` | 从 Flash 读取分区信息 (magic 不匹配返回 -1) |
| `Bootloader_WritePartitionInfo(info)` | 保存分区信息到 Flash (先擦除再写入) |
| `Bootloader_ValidateApp(addr, &ver)` | 校验 App 向量表有效性 + 读出版本号 |
| `Bootloader_CheckAndUpgrade(info)` | 检测另一分区版本是否更高, 是则 CRC32 校验并激活 |
| `Bootloader_TryRollback(info, &addr)` | 活跃分区损坏时自动切回备用分区 |
| `Bootloader_VerifyAndActivate(info)` | CRC32 校验新固件, 通过则切换 active_partition |
| `CRC32_Calculate(data, len)` | 标准 CRC32 计算 (多项式 0x04C11DB7) |

### Flash 接口 ([flash_if.h](Core/Inc/flash_if.h))

| 函数 | 功能 |
|------|------|
| `Flash_Unlock()` / `Flash_Lock()` | 解锁/锁定 Flash 控制器 |
| `Flash_ErasePage(addr)` | 擦除 1 页 (1KB) |
| `Flash_EraseRange(start, size)` | 擦除多页 (自动对齐页边界) |
| `Flash_WriteWord(addr, data)` | 写入 32 位 (自动拆为 2 次半字, 适配 STM32F1 限制) |
| `Flash_Write(addr, buf, len)` | 批量写入 (自动处理对齐) |
| `Flash_Read(addr, buf, len)` | 从 Flash 读取数据 (直接内存访问) |

## 分区信息结构体

```c
typedef struct {
    uint32_t magic;              // 魔数 0xAA55AA55 (判断数据有效性)
    uint8_t  active_partition;   // 当前激活: 0=AppA, 1=AppB
    uint8_t  reserved1[3];       // 对齐保留

    uint32_t app_a_crc;          // App A CRC32
    uint32_t app_a_size;         // App A 大小 (字节)
    uint32_t app_a_version;      // App A 版本号

    uint32_t app_b_crc;          // App B CRC32
    uint32_t app_b_size;         // App B 大小 (字节)
    uint32_t app_b_version;      // App B 版本号

    uint8_t  update_pending;     // 1 = 有待升级固件
    uint8_t  update_target;      // 升级目标: 0=AppA, 1=AppB
    uint8_t  reserved2[2];       // 对齐保留
    uint32_t update_size;        // 新固件大小
    uint32_t update_crc;         // 新固件 CRC32
} partition_info_t;
```

## App 工程适配

App 是独立工程, 不在本仓库, 需要做以下三处修改才能被 Bootloader 正常启动:

### 1. 链接脚本: Flash 起始地址偏移 16KB

将 App 工程的链接脚本中 Flash 段改为:

```ld
FLASH (rx) : ORIGIN = 0x08004000, LENGTH = 24K
```

> 如果 App 只需要 App A 分区 (不带双分区), `LENGTH = 24K` 即可。
> 如果 App A/B 都会用到, 可改为 `LENGTH = 48K` 覆盖两个区。

### 2. 中断向量表重映射 (VTOR)

STM32 HAL 库已内置了 VTOR 配置机制, 只需改两处:

**① 启用宏定义**

在 App 的 `CMakeLists.txt` 中添加编译定义:

```cmake
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE USER_VECT_TAB_ADDRESS)
```

或直接在 `stm32f1xx_hal_conf.h` 中:

```c
#define USER_VECT_TAB_ADDRESS
```

**② 修改偏移量**

在 App 的 `system_stm32f1xx.c` 中找到:

```c
#define VECT_TAB_OFFSET  0x00000000U
```

改为:

```c
#define VECT_TAB_OFFSET  0x00004000U   /* App A 偏移 16KB */
```

**原理**: HAL 的 `SystemInit()` 内有以下代码, 定义上述宏后会自动生效:

```c
#if defined(USER_VECT_TAB_ADDRESS)
  SCB->VTOR = VECT_TAB_BASE_ADDRESS | VECT_TAB_OFFSET;
  // = FLASH_BASE | 0x4000 = 0x08004000
#endif
```

### 3. 固化版本号 (用于升级检测)

Bootloader 通过读取 App 固件偏移 `0x10C` 处的 `uint32_t` 来获取版本号。
App 必须在链接时在该位置放置版本号:

**方法一: 在代码中定义常量 (推荐)**

```c
/* 放在 main.c 或单独的 version.c 中 */
const uint32_t __attribute__((section(".app_version"))) app_version = 0x01000000;
/* 含义: 主版本=1, 次版本=0, 修订=0, 构建=0                       */
```

并在 App 链接脚本中添加:

```ld
.app_version 0x0800410C : {
    KEEP(*(.app_version))
} > FLASH
```

**方法二: 在链接脚本中直接定义符号**

```ld
_app_version = 0x01000000;
```

无论哪种方式, Bootloader 会读取 `App地址 + 0x10C` 处的 `uint32_t` 作为版本号。
版本号格式自由, 只要保证 **数值越大 = 版本越新** 即可。

### 4. 无需其他操作

- **中断**: VTOR 重映射后, App 可正常使用所有中断, 无需额外配置
- **分区信息**: App 不需要硬编码任何 `partition_info_t`, 由 Bootloader 全权管理
- **独立性**: App 不感知 Bootloader 的存在, 正常运行即可

### 首次烧录流程

```bash
# 1. 烧录 Bootloader 到 0x08000000
# 2. 烧录 App 到 0x08004000
# 3. 上电 → Bootloader 自动检测 App A → 创建分区信息 → 跳转
```

### 快速验证

烧录后如果 App 没运行, 检查:

| 现象 | 可能原因 | 检查 |
|------|----------|------|
| LED 慢闪 (500ms) | App 未烧录或烧错地址 / 无新固件 | 确认烧录地址为 `0x08004000` |
| LED 慢闪 + 按键未按下 | GPIO 浮空误触发 | 确认 PA0 有下拉电阻或在 CubeMX 中启用内部下拉 |
| LED 快闪 (150ms) | 两个分区都无效 | 确认 App 烧录地址正确 |
| HardFault | VTOR 未设置 | 确认 `VECT_TAB_OFFSET = 0x4000` 且 `USER_VECT_TAB_ADDRESS` 已定义 |
| App 正常运行但中断不响应 | 链接脚本未改 | 确认链接脚本 `ORIGIN = 0x08004000` |
| 不自动升级 | 版本号未固化 / 新版版本号 ≤ 旧版 | 确认 `APP_VERSION_OFFSET` (0x10C) 处有正确版本号 |

## 待实现

- [ ] 升级协议 (UART Ymodem / 自定义协议 / CAN) — 框架入口在 `Bootloader_UpdateMode()`
- [ ] 看门狗 (IWDG) 支持

## License

基于 STM32CubeMX 生成, 遵循 STMicroelectronics 相关许可。