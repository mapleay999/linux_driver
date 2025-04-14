#include "stm32mp157d.h"
/*
内核类型 u32：直接使用，无需额外定义，但需包含 <linux/types.h>。
避免在内核代码中使用用户空间的 uint32_t，优先使用 u32。
跨平台兼容性：u32 明确表示 32 位无符号整数，无论底层架构是 32 位还是 64 位。
*/
#include <linux/types.h>
/*
在 Linux 内核驱动开发中，writel 和 readl 是用于 访问内存映射 I/O 寄存器 的核心函数。

writel(value, addr)作用：向内存映射的寄存器地址 addr 写入一个 32 位的值 value。
readl(addr)作用：从内存映射的寄存器地址 addr 读取一个 32 位的值。

关键特性:
    原子性：
    writel 和 readl 保证对寄存器的访问是 原子操作（不可中断），避免并发问题。
    内存屏障：
    这些函数隐式包含 内存屏障（Memory Barrier），确保读写操作的顺序性，防止编译器或 CPU 乱序优化。
    字节序处理：
    自动处理 小端/大端字节序，确保数据按设备期望的格式写入或读取。

使用前提
在使用 writel 和 readl 前，必须 将物理地址映射到内核虚拟地址空间，通常通过 ioremap() 实现。

总结
writel 和 readl 是 Linux 内核中访问硬件寄存器的标准接口。
使用步骤：
    1. 包含 <linux/io.h>。
    2. 通过 ioremap() 映射物理地址到虚拟地址。
    3. 使用 readl 和 writel 操作寄存器。
    4. 退出时调用 iounmap() 释放资源。
通过合理使用这些函数，可以安全高效地控制硬件外设。
*/
#include <linux/io.h>


/* 
__iomem：内核中用于标识 I/O 内存的修饰符，
提醒编译器这些指针指向的是设备寄存器而非普通内存。 
*/
static void __iomem *MPU_AHB4_PERIPH_RCC_PI; 
static void __iomem *GPIOI_MODER_PI; 
static void __iomem *GPIOI_OTYPER_PI; 
static void __iomem *GPIOI_OSPEEDR_PI; 
static void __iomem *GPIOI_PUPDR_PI; 
static void __iomem *GPIOI_BSRR_PI; 


/* 初始化 LED */ 
void led_init(void)
{
    u32 val = 0;

    /* 1、寄存器地址映射 */ 
    MPU_AHB4_PERIPH_RCC_PI = ioremap(RCC_MP_AHB4ENSETR, 4); 
    GPIOI_MODER_PI         = ioremap(GPIOI_MODER, 4); 
    GPIOI_OTYPER_PI        = ioremap(GPIOI_OTYPER, 4); 
    GPIOI_OSPEEDR_PI       = ioremap(GPIOI_OTYPER, 4); 
    GPIOI_PUPDR_PI         = ioremap(GPIOI_PUPDR, 4); 
    GPIOI_BSRR_PI          = ioremap(GPIOI_BSRR, 4); 
    
    /* 2、使能 PI 时钟 */ 
    val = readl(MPU_AHB4_PERIPH_RCC_PI); 
    val &= ~(0X1 << 8);                 /* 清除以前的设置   */ 
    val |= (0X1 << 8);                  /* 设置新值      */ 
    writel(val, MPU_AHB4_PERIPH_RCC_PI); 
    
    /* 3、设置 PI0 通用的输出模式。*/ 
    val = readl(GPIOI_MODER_PI); 
    val &= ~(0X3 << 0);                 /* bit0:1 清零    */ 
    val |= (0X1 << 0);                  /* bit0:1 设置 01   */ 
    writel(val, GPIOI_MODER_PI); 
    
    /* 3、设置 PI0 为推挽模式。*/ 
    val = readl(GPIOI_OTYPER_PI); 
    val &= ~(0X1 << 0);                 /* bit0 清零，设置为上拉*/ 
    writel(val, GPIOI_OTYPER_PI); 
    /* 4、设置 PI0 为高速。*/ 
    val = readl(GPIOI_OSPEEDR_PI); 
    val &= ~(0X3 << 0);                 /* bit0:1 清零     */ 
    val |= (0x2 << 0);                  /* bit0:1 设置为 10   */ 
    writel(val, GPIOI_OSPEEDR_PI); 
    
    /* 5、设置 PI0 为上拉。*/ 
    val = readl(GPIOI_PUPDR_PI); 
    val &= ~(0X3 << 0);                 /* bit0:1 清零     */ 
    val |= (0x1 << 0);                  /*bit0:1 设置为 01    */ 
    writel(val,GPIOI_PUPDR_PI); 
    
    /* 6、默认关闭 LED */ 
    val = readl(GPIOI_BSRR_PI); 
    val |= (0x1 << 0); 
    writel(val, GPIOI_BSRR_PI); 
}

/* 
 * @description : LED 打开/关闭
 * @param - sta : 关闭LED:LEDON(0)；打开LED:LEDOFF(1)
 * @return      : 无
 */ 
void led_switch(u8 sta)
{ 
    u32 val = 0;
    if(sta == LEDON)
    {
        val = readl(GPIOI_BSRR_PI);
        val |= (1 << 16);
        writel(val, GPIOI_BSRR_PI);
    }else if(sta == LEDOFF)
    {
        val = readl(GPIOI_BSRR_PI);
        val|= (1 << 0);
        writel(val, GPIOI_BSRR_PI);
    }
}

/* 
 * @description :重置硬件资源
 * @return      : 无
 */ 
void led_deinit(void)
{
    /* 解除映射 */ 
    iounmap(MPU_AHB4_PERIPH_RCC_PI); 
    iounmap(GPIOI_MODER_PI); 
    iounmap(GPIOI_OTYPER_PI); 
    iounmap(GPIOI_OSPEEDR_PI); 
    iounmap(GPIOI_PUPDR_PI); 
    iounmap(GPIOI_BSRR_PI); 
    
    /* 应该还有其他硬件资源需要重置
     * 但是这里只是演示，无需太严格
     * 所以，只写这些，其他省略。
    */
    
}