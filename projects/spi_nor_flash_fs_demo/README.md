# SPI NOR Flash 文件系统示例

## 简介

本示例基于 **HPM5100EVK** 开发板，演示了如何在 RT-Thread 上通过 SPI 接口驱动 NOR Flash，并使用 FAL（Flash Abstraction Layer）+ FatFs 文件系统对其进行挂载管理。

示例的核心功能包括：

- **LED 闪烁**：通过 RT-Thread 线程控制板载 LED 周期性闪烁
- **SPI NOR Flash 驱动**：基于 SFUD（Serial Flash Universal Driver）自动识别 Flash 型号（支持 SFDP 标准），使用 QSPI 四线模式通信
- **FAL 分区管理**：通过 FAL 将 Flash 划分为多个分区，便于应用层访问
- **文件系统挂载**：在 FAL 分区上创建块设备，挂载 elm FatFs 文件系统

---

## 功能说明

### 1. LED 闪烁线程

`main()` 函数中创建了一个 `led_th` 线程，以 500ms 为周期交替点亮/熄灭 LED0，作为系统正常运行的指示。

```c
rt_thread_t led_thread = rt_thread_create("led_th", thread_entry, &led_thread_arg, 1024, 1, 10);
rt_thread_startup(led_thread);
```

### 2. SPI NOR Flash 初始化

通过 `INIT_COMPONENT_EXPORT` 自动注册的初始化函数 `rt_hw_spi_flash_init()`：

1. **配置 CS 引脚**：将 PA11 设为输出模式
2. **挂载 SPI 设备**：通过 `rt_hw_spi_device_attach()` 将 Flash 设备 `qspi_flash` 挂载到 SPI 总线 `qspi0` 上，使用软件 CS 回调
3. **SFUD 探测 Flash**：调用 `rt_sfud_flash_probe()` 自动识别 Flash 芯片型号和参数

```c
static int rt_hw_spi_flash_init(void)
{
    rt_pin_mode(SPI_FLASH_CS_PIN, PIN_MODE_OUTPUT);
    rt_hw_spi_device_attach(SPI_FLASH_BUS_NAME, SPI_FLASH_DEVICE_NAME, spi_flash_cs_ctrl);
    rt_sfud_flash_probe(SPI_NOR_FLASH_DEV_NAME, SPI_FLASH_DEVICE_NAME);
    ...
}
INIT_COMPONENT_EXPORT(rt_hw_spi_flash_init);
```

### 3. FAL 分区 & 文件系统挂载

通过 `INIT_ENV_EXPORT` 自动注册的初始化函数 `filesystem_init()`：

1. **初始化 FAL**：调用 `fal_init()`
2. **创建块设备**：在 FAL 的 `fs` 分区上创建块设备
3. **挂载文件系统**：尝试挂载 elm FatFs；若首次使用则自动格式化

```c
static int filesystem_init(void)
{
    fal_init();
    struct rt_device *flash_dev = fal_blk_device_create("fs");
    if (dfs_mount("fs", "/", "elm", 0, 0) == 0)
        rt_kprintf("File system mounted to /\n");
    else {
        dfs_mkfs("elm", "fs");
        dfs_mount("fs", "/", "elm", 0, 0);
    }
    ...
}
INIT_ENV_EXPORT(filesystem_init);
```

### 4. FAL 分区表

分区表定义在 `board/fal_cfg.h` 中（`FAL_PART_TABLE`），本项目配置了两个 Flash 设备上的分区：

| 分区名 | Flash 设备 | 偏移 | 大小 | 用途 |
|--------|-----------|------|------|------|
| `app` | `norflash0`（内部 Flash） | 0 | 256 KB | 应用程序 |
| `easyflash` | `norflash0`（内部 Flash） | 256 KB | 256 KB | EasyFlash KV 存储 |
| `download` | `norflash0`（内部 Flash） | 512 KB | 256 KB | 固件下载区 |
| `flashdb` | `norflash0`（内部 Flash） | 768 KB | 256 KB | FlashDB 数据库 |
| `app` | `spi_norflash0`（SPI NOR Flash） | 0 | 6 MB | SPI Flash 应用区 |
| **`fs`** | `spi_norflash0`（SPI NOR Flash） | **6 MB** | **10 MB** | **文件系统分区** |

---

## Menuconfig 配置

运行 `scons --menuconfig` 进入配置界面，以下选项需要正确启用。

### 1. 启用 SPI 外设

```text
Hardware Drivers Config → On-chip Peripheral Drivers → Enable SPI
```

- `[*] Enable SPI` → **选中**
- `[*] Enable SPI0` → **选中**
  - `[*] Enable SPI0 DMA` → 可选（本示例未使用 DMA）
  - `Select SPI0 CS TYPE` → **Enable SPI0 software cs**（使用 GPIO 软件片选）
  - `Select SPI0 IO mode` → **Enable SPI0 quad IO mode**（四线 QSPI 模式）

### 2. 启用文件系统 (DFS)

```text
RT-Thread Components → DFS: device virtual file system
```

- `[*] Using DFS` → **选中**
- `[*] Using posix API` → 选中（推荐）
- `[*] Using working directory` → 选中
- `[*] Using elm-chan's FatFs` → **选中**
  - `Long file name (LFN) type` → 3（支持长文件名）
  - `Max LFN working buffer length` → 255
  - `Number of volumes` → 2
  - `Maximum sector size` → 4096

### 3. 启用 FAL

```text
RT-Thread Components → FAL: Flash Abstraction Layer
```

- `[*] Using FAL` → **选中**
- `[*] Using debug mode` → 可选（调试用）
- `[*] Using partition table defined in 'fal_cfg.h'` → **选中**

### 4. 启用 SFUD

```text
RT-Thread Components → Device Drivers → Using SFUD (Serial Flash Universal Driver)
```

- `[*] Using SFUD` → **选中**
- `[*] Using SFDP mode` → 选中（自动读取 Flash 参数）
- `[*] Using flash information table` → 选中
- `[*] Using QSPI mode` → **选中**
- `SPI max frequency` → 40000000（40 MHz）

### 5. 启用 QSPI

```text
RT-Thread Components → Device Drivers → Using QSPI
```

- `[*] Using QSPI` → **选中**

### 6. 启用 SPI 驱动框架

```text
RT-Thread Components → Device Drivers → Using SPI Bus/Device driver
```

- `[*] Using SPI Bus/Device driver` → **选中**


## 运行效果

启动后，串口控制台输出类似以下信息：

```console
 \ | /
- RT -     Thread Operating System
 / | \     5.2.2 build Jul  2 2026 16:50:21
 2006 - 2024 Copyright by RT-Thread team
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud.c:870) The flash device manufacturer ID is 0xC2, memory type ID is 0x20, capac
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:131) Check SFDP header is OK. The reversion is V1.6, NPN is 2.
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:173) Check JEDEC basic flash parameter header is OK. The table id is 0,
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:203) JEDEC basic flash parameter table info:
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:204) MSB-LSB  3    2    1    0
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:206) [0001] 0xFF 0xF1 0x20 0xE5
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:206) [0002] 0x07 0xFF 0xFF 0xFF
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:206) [0003] 0x6B 0x08 0xEB 0x44
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:206) [0004] 0xBB 0x04 0x3B 0x08
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:206) [0005] 0xFF 0xFF 0xFF 0xFE
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:206) [0006] 0xFF 0x00 0xFF 0xFF
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:206) [0007] 0xEB 0x44 0xFF 0xFF
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:206) [0008] 0x52 0x0F 0x20 0x0C
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:206) [0009] 0xFF 0x00 0xD8 0x10
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:215) 4 KB Erase is supported throughout the device. Command is 0x20.
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:234) Write granularity is 64 bytes or larger.
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:245) Target flash status register is non-volatile.
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:271) 3-Byte only addressing.
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:305) Capacity is 16777216 Bytes.
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:311) Flash device supports 4KB block erase. Command is 0x20.
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:311) Flash device supports 32KB block erase. Command is 0x52.
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud_sfdp.c:311) Flash device supports 64KB block erase. Command is 0xD8.
[I/SFUD] Found a Macronix flash chip. Size is 16777216 bytes.
[D/SFUD] (rt-thread\components\drivers\spi\sfud\src\sfud.c:849) Flash device reset success.
[I/SFUD] spi_norflash0 flash device initialized successfully.
[I/SFUD] Probe SPI flash spi_norflash0 by SPI device qspi_flash success.
SPI Flash initialized successfully!
[D/FAL] Flash device |                norflash0 | addr: 0x80000000 | len: 0x00100000 | blk_size: 0x00001000 |initialized finish.
[D/FAL] Flash device |            spi_norflash0 | addr: 0x00000000 | len: 0x01000000 | blk_size: 0x00001000 |initialized finish.
[I/FAL] ==================== FAL partition table ====================
[I/FAL] | name      | flash_dev     |   offset   |    length  |
[I/FAL] -------------------------------------------------------------
[I/FAL] | app       | norflash0     | 0x00000000 | 0x00040000 |
[I/FAL] | easyflash | norflash0     | 0x00040000 | 0x00040000 |
[I/FAL] | download  | norflash0     | 0x00080000 | 0x00040000 |
[I/FAL] | flashdb   | norflash0     | 0x000c0000 | 0x00040000 |
[I/FAL] | app       | spi_norflash0 | 0x00000000 | 0x00600000 |
[I/FAL] | fs        | spi_norflash0 | 0x00600000 | 0x00a00000 |
[I/FAL] =============================================================
[I/FAL] RT-Thread Flash Abstraction Layer initialize success.
[I/FAL] The FAL block device (fs) created successfully
File system mounted to /
```

> 如果 Flash 是首次使用或分区未格式化，会先自动执行 `dfs_mkfs` 格式化，然后挂载。
> 表中两个 `app` 分区分别位于内部 Flash（norflash0）与 SPI NOR Flash（spi_norflash0）上，见 `fal_cfg.h` 中的分区表定义。


## 文件系统测试

在 msh 终端中可执行以下命令验证文件系统：

```bash
# 查看根目录
ls /

# 创建目录
mkdir /data

# 创建文件并写入内容
echo "Hello SPI Flash!" test.txt

# 读取文件
cat test.txt

# 查看磁盘使用情况
df
```

