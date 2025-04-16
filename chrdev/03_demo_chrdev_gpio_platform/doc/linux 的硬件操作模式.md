## gpio_set_value 这个函数是linux内核的吗

是的，`gpio_set_value` 是 Linux 内核中的一个函数，属于内核 **GPIO 子系统**的 API。它用于在内核空间（如驱动或模块）中设置 GPIO 引脚的电平状态（高/低）。

---

### 关键细节：
1. **所属头文件**：
   - 旧版接口：`#include <linux/gpio.h>`
   - 新版推荐接口（基于描述符）：`#include <linux/gpiod.h>`（使用 `gpiod_set_value()` 替代）

2. **功能**：
   - 设置指定 GPIO 引脚的电平值（0 或 1），需确保引脚已配置为输出模式（例如通过 `gpio_direction_output` 或 `gpiod_direction_output`）。

3. **过时性**：
   - `gpio_set_value` 属于旧的基于整数的 GPIO 接口（通过 GPIO 编号操作），在较新内核（4.8+）中已逐渐被 **gpiod 系列函数**（基于描述符）取代，例如：
     ```c
     void gpiod_set_value(struct gpio_desc *desc, int value);
     ```

4. **用户空间 vs 内核空间**：
   - **内核模块/驱动**：可以直接调用 `gpio_set_value` 或 `gpiod_set_value`。
   - **用户空间程序**：无法直接调用内核函数，需通过 sysfs、字符设备（如 `/dev/gpiochip*`）或库（如 `libgpiod`）操作 GPIO。

---

### 示例代码（旧接口）：
```c
#include <linux/gpio.h>

int gpio = 123; // GPIO 编号
gpio_request(gpio, "example");
gpio_direction_output(gpio, 0);
gpio_set_value(gpio, 1); // 输出高电平
```

### 推荐的新接口（gpiod）：
```c
#include <linux/gpiod.h>

struct gpio_desc *desc = gpiod_get(dev, "example", GPIOD_OUT_LOW);
gpiod_set_value(desc, 1); // 输出高电平
```

---

### 注意事项：
- **权限问题**：内核中操作 GPIO 需确保引脚未被其他驱动占用。
- **兼容性**：新代码建议使用 `gpiod_*` 接口以适配未来的内核版本。

如果您的代码运行在用户空间，请改用 `libgpiod` 库或 sysfs 文件系统操作 GPIO。



## gpiod_set_value 这个内核版本 与在内核中直接操作寄存器有什么区别？

在内核中使用 `gpiod_set_value` 和直接操作寄存器控制 GPIO 引脚，有本质区别，主要体现在 **抽象层级、安全性、可移植性、维护成本** 和 **适用场景** 上。以下是详细对比：

---

### 1. **抽象层级与硬件依赖**
#### (1) `gpiod_set_value`（内核 GPIO 子系统）
- **抽象层**：通过 Linux 内核提供的 **GPIO 子系统** 操作 GPIO，隐藏硬件细节。
- **硬件无关性**：  
  开发者无需关心具体芯片的寄存器地址或位定义，只需通过标准 API（如 `gpiod_set_value`）设置电平，内核会自动处理底层硬件差异。
- **示例**：
  ```c
  struct gpio_desc *led = gpiod_get(dev, "led", GPIOD_OUT_LOW);
  gpiod_set_value(led, 1);  // 输出高电平，内核自动翻译为底层寄存器操作
  ```

#### (2) 直接操作寄存器
- **直接访问硬件**：  
  开发者需要手动计算 GPIO 对应的寄存器物理地址，通过读写内存映射（如 `ioremap`）直接修改寄存器值。
- **硬件强依赖**：  
  代码与具体芯片绑定，不同平台（如树莓派、STM32）的寄存器地址和位定义完全不同，代码不可移植。
- **示例**：
  ```c
  void __iomem *gpio_reg = ioremap(0xFE200000, 4);  // 假设树莓派 GPIO 基地址
  u32 val = ioread32(gpio_reg);
  val |= (1 << 17);  // 假设 GPIO17 对应寄存器第17位
  iowrite32(val, gpio_reg);
  ```

---

### 2. **安全性与稳定性**
#### (1) `gpiod_set_value`
- **内核管理**：  
  内核确保 GPIO 的请求、配置和释放是线程安全的，避免多个驱动或模块同时操作同一 GPIO 导致的冲突。
- **错误检查**：  
  内核会验证 GPIO 是否已正确初始化（如方向是否为输出），防止非法操作。
- **电源管理**：  
  与内核电源管理子系统集成，休眠时自动关闭 GPIO，避免意外功耗。

#### (2) 直接操作寄存器
- **绕过内核保护**：  
  直接修改寄存器可能破坏内核状态（如其他驱动正在使用该 GPIO），导致系统崩溃或硬件损坏。
- **无错误检查**：  
  若错误配置寄存器（如将输入引脚设为输出），可能引发短路或器件损坏。
- **并发风险**：  
  需自行处理多线程/中断环境下的竞态条件（如未使用锁导致数据不一致）。

---

### 3. **性能与实时性**
#### (1) `gpiod_set_value`
- **性能开销**：  
  需要经过内核抽象层（如 GPIO 子系统、设备树解析），会有微秒级延迟。
- **适用场景**：  
  适用于大多数通用场景（如控制 LED、按键），但对实时性要求极高（如高速 PWM）可能不足。

#### (2) 直接操作寄存器
- **极致性能**：  
  直接读写寄存器通常只需几条 CPU 指令，延迟在纳秒级。
- **适用场景**：  
  实时性要求极高的场景（如电机控制、高速通信），但需牺牲可移植性和安全性。

---

### 4. **可移植性与维护成本**
| **对比项**         | `gpiod_set_value`                  | 直接操作寄存器           |
| ------------------ | ---------------------------------- | ------------------------ |
| **跨平台支持**     | ✅ 同一代码适配不同硬件             | ❌ 每换一个芯片需重写代码 |
| **内核版本兼容性** | ✅ 内核维护 API 兼容性              | ❌ 依赖具体寄存器布局     |
| **代码可读性**     | ✅ 语义清晰（如 `gpiod_set_value`） | ❌ 需注释寄存器位定义     |
| **维护成本**       | ✅ 低                               | ❌ 高（需跟踪硬件更新）   |

---

### 5. **何时选择哪种方式？
#### (1) **优先使用 `gpiod_set_value`**
- 开发通用内核驱动（如 LED、按键驱动）。
- 需要代码跨平台移植（如同时支持树莓派和 i.MX6）。
- 注重安全性和长期维护性。

#### (2) **谨慎选择直接操作寄存器**
- 实时性要求极高（如生成 100MHz 的 PWM 信号）。
- 内核 GPIO 子系统无法满足需求（如操作特殊功能寄存器）。
- 对目标硬件寄存器有完全掌控（如裸机或定制内核）。

---

### 总结
| **特性**     | `gpiod_set_value` | 直接操作寄存器         |
| ------------ | ----------------- | ---------------------- |
| **抽象层级** | 高（硬件无关）    | 低（直接操作硬件）     |
| **安全性**   | 高（内核保护）    | 低（需自行处理冲突）   |
| **性能**     | 微秒级延迟        | 纳秒级延迟             |
| **可移植性** | 高                | 无                     |
| **适用场景** | 通用驱动、跨平台  | 实时控制、硬件深度优化 |

**建议**：优先使用内核 API（`gpiod_set_value`），仅在极端性能需求或特殊硬件场景下直接操作寄存器。



## tarena 关于linux gpio 的讲解

1.6.linux内核提供的GPIO操作库函数
1.问：何为GPIO操作
  答：输入或者输出操作
      回顾ARM裸板GPIO输出开关灯操作代码：
	  //四选一:选择为GPIO功能
	  GPIOCALTFN0 &= ~(3 << 24);
	  GPIOCALTFN0 |= (1 << 24);
	  //二选一:选择为输出功能
 	 GPIOCOUTENB |= (1 << 12);

  	//输出1或者0
 	 GPIOCOUT |= (1 << 12);
  	GPIOCOUT &= ~(1 << 12);

  找茬：此代码可读性非常差劲,还必须研究手册,位运算
  办法：在linux内核中,内核提供了相关的GPIO操作函数
		简化GPIO操作的流程

2.内核提供的GPIO操作函数如下： 
	int gpio_request(unsigned gpio, const char *label)
      功能：在linux内核中,处理器的任何GPIO硬件对于linux内核来说
			都是一种宝贵的资源,内核程序要想访问操作某个GPIO
			硬件,必须利用此函数先向内核申请这个GPIO资源
			类似：malloc
	  参数：
	  gpio:在linux内核中,内核给每个GPIO硬件都指定分配唯一的一个
		   软件编号,简称GPIO编号,类似GPIO硬件的身份证号  
		   GPIO硬件对应的软件编号定义：
		   例如：
				GPIO硬件 				GPIO编号
				GPIOC12				PAD_GPIO_C + 12
				GPIOB26				PAD_GPIO_B + 26
				...					...
	  label：随意指定一个标签名称即可,表示申请的GPIO硬件的标识
	  返回值：不用记,只需参考内核大神如何使用判断此函数的返回值即可
		  照猫画虎
          头文件：只需将大神调用此函数使用的头文件全盘拷贝即可    
2.2.void gpio_free(unsigned gpio)
	功能：如果内核程序不再使用某个GPIO硬件,记得要释放资源   
2.3.gpio_direction_output(unsigned gpio, int value);
        功能：配置GPIO为输出功能同时输出value(1高/0低)	
2.4.gpio_direction_input(unsigned gpio);
        功能：配置GPIO为输入功能	
2.5.gpio_set_value(unsigned gpio, int value);
        设置GPIO的输出值为value(1/0)
	此函数使用的前提是提前配置为输出功能	
2.6.int gpio_get_value(unsigned gpio);
        获取GPIO的电平状态,返回值保存状态
	此函数对输入还是输出无要求

案例：编写内核程序,要求加载驱动开灯,卸载驱动关灯
上位机执行：
mkdir /opt/drivers/day02/1.0 -p 
cd /opt/drivers/day02/1.0
vim led_drv.c 
vim Makefile
make
cp led_drv.ko /opt/rootfs/home/drivers/

下位机测试：
cd /home/drivers/
insmod led_drv.ko 
rmmod led_drv