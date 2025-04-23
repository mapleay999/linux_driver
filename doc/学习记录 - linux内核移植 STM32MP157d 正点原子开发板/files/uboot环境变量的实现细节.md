

在 U-Boot 中，将环境变量写入 MMC（包括 eMMC 或 SD 卡）的过程涉及以下关键步骤和协议实现：

---

### 一、MMC 写入的基本原理
MMC（MultiMediaCard）设备的读写操作基于 **MMC/SD 协议**，通过发送**标准命令**和**数据块**完成。U-Boot 的 MMC 驱动通过以下方式实现写入：
1. **协议层**：遵循 MMC/SD 规范，使用块写入命令（如 `CMD24` 写单块，`CMD25` 写多块）。
2. **硬件控制器**：依赖 SoC 的 MMC 控制器（如 STM32 的 SDMMC、i.MX 的 USDHC）将协议命令转换为物理信号（时钟、数据线）。
3. **Flash 管理**：MMC 设备内部集成 Flash 控制器，负责将逻辑块地址（LBA）映射到物理 NAND Flash 单元，并处理擦除、编程等操作。

---

### 二、U-Boot 中环境变量写入 MMC 的流程
以下为 U-Boot 将环境变量写入 MMC 的具体实现步骤：

#### 1. **配置环境变量存储位置**
在 U-Boot 的配置文件（如 `include/configs/<board>.h` 或 Kconfig）中定义：
```c
#define CONFIG_ENV_IS_IN_MMC       // 指定环境变量存储在 MMC
#define CONFIG_SYS_MMC_ENV_DEV     0     // MMC 设备号（如 mmc0）
#define CONFIG_SYS_MMC_ENV_PART    0     // 分区号（通常为 0 或保留分区）
#define CONFIG_ENV_OFFSET          (64 * 1024)  // 环境变量起始偏移（64KB）
#define CONFIG_ENV_SIZE            (8 * 1024)   // 环境变量大小（8KB）
```

#### 2. **环境变量保存操作**
用户执行 `saveenv` 或 `env save` 时，U-Boot 调用 MMC 驱动写入数据：
```c
// 代码路径：env/mmc.c
static int mmc_saveenv(void)
{
    struct mmc *mmc = find_mmc_device(CONFIG_SYS_MMC_ENV_DEV);
    // 将环境变量数据写入 MMC 的指定偏移
    return mmc->block_dev.block_write(&mmc->block_dev, 
        CONFIG_SYS_MMC_ENV_PART, 
        CONFIG_ENV_OFFSET / mmc->write_bl_len,  // 转换为块地址
        CONFIG_ENV_SIZE / mmc->write_bl_len, 
        (void *)env_ptr);
}
```

#### 3. **MMC 驱动层实现
MMC 驱动的核心写入函数通过以下步骤发送协议命令：
- **步骤 1**：发送 `CMD24`（写单块）或 `CMD25`（写多块）命令，指定写入的 LBA。
- **步骤 2**：通过数据线（DAT0-DAT3）传输数据块（通常为 512 字节/块）。
- **步骤 3**：等待 MMC 设备响应 `CMD13`（检查操作状态），确认写入成功。

#### 4. **硬件控制器操作
SoC 的 MMC 控制器将协议命令转换为电气信号：
- **时钟信号**（CLK）：同步数据传输。
- **数据线**（DAT0-DAT3）：传输实际数据。
- **命令线**（CMD）：发送命令字。

---

### 三、关键协议命令详解
| 命令      | 说明                               |
| --------- | ---------------------------------- |
| **CMD0**  | 复位 MMC 设备到空闲状态。          |
| **CMD16** | 设置块长度（例如 512 字节）。      |
| **CMD24** | 写入单个数据块到指定 LBA。         |
| **CMD25** | 写入多个连续数据块（多块写入）。   |
| **CMD13** | 获取操作状态（检查写入是否成功）。 |

---

### 四、与 SPI/I2C 的区别
- **MMC 协议**：  
  基于专用总线（CLK, CMD, DAT0-DAT3），支持高速并行数据传输（如 HS200、HS400 模式）。
- **SPI 模式**：  
  某些 MMC 设备支持 SPI 接口作为兼容模式，但性能较低（仅使用 MOSI/MISO 单线传输）。
- **I2C**：  
  完全不同的协议，不用于 MMC 设备通信。

---

### 五、示例：STM32 MMC 写入时序
以 STM32 的 SDMMC 控制器为例：
1. 初始化 SDMMC 时钟和 GPIO。
2. 发送 `CMD0` 复位设备。
3. 发送 `CMD8` 检查电压兼容性。
4. 发送 `ACMD41` 初始化设备进入传输状态。
5. 发送 `CMD16` 设置块大小。
6. 发送 `CMD24` + LBA，随后传输数据块。
7. 发送 `CMD13` 确认写入成功。

---

### 六、调试与验证
1. **查看 MMC 设备信息**：
   ```bash
   => mmc info
   ```
   输出示例：
   ```
   Device: mmc@48040000
   Manufacturer ID: 15
   OEM: 100
   Name: 8GTF4 
   Bus Speed: 52000000
   Mode: HS200 (200MHz)
   Rd Block Len: 512
   ```

2. **手动读写测试**：
   ```bash
   => mmc write ${loadaddr} 0x800 0x10  # 将内存地址数据写入 LBA 0x800，写入 16 块
   => mmc read ${loadaddr} 0x800 0x10    # 读取验证
   ```

---

### 总结
U-Boot 通过 **MMC 协议命令**（如 CMD24/CMD25）将环境变量写入 MMC 设备的指定偏移位置，依赖 SoC 的 MMC 控制器实现物理层通信。整个过程与 SPI/I2C 无关，而是基于 MMC 标准协议的高速块设备操作。