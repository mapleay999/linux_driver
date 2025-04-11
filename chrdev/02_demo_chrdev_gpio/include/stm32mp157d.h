
#ifndef __STM32MP157d_H__
#define __STM32MP157d_H__

#include <linux/io.h>

#define LEDOFF  0  /* 关灯 */
#define LEDON   1  /* 开灯 */


/* 寄存器物理地址 */ 
#define PERIPH_BASE              (0x40000000) 
#define MPU_AHB4_PERIPH_BASE    (PERIPH_BASE + 0x10000000) 
#define RCC_BASE                  (MPU_AHB4_PERIPH_BASE + 0x0000)  
#define RCC_MP_AHB4ENSETR       (RCC_BASE + 0XA28) 
#define GPIOI_BASE                (MPU_AHB4_PERIPH_BASE + 0xA000)  
#define GPIOI_MODER              (GPIOI_BASE + 0x0000)    
#define GPIOI_OTYPER             (GPIOI_BASE + 0x0004)  
#define GPIOI_OSPEEDR            (GPIOI_BASE + 0x0008)    
#define GPIOI_PUPDR               (GPIOI_BASE + 0x000C)    
#define GPIOI_BSRR               (GPIOI_BASE + 0x0018)    

void led_init(void); //需写出，否则其他c文件调用，提示非显性警告。
void led_switch(u8 sta);
void led_deinit(void);

#endif

/*
1. 关于 __iomem 之类的标记：
__iomem 的作用：
向开发者表明该指针指向的是 设备寄存器或内存映射的 I/O 区域（而非普通内存）。
帮助编译器进行类型检查（例如，避免直接解引用这类指针）。
使用场景：
static void __iomem *MPU_AHB4_PERIPH_RCC_PI;  // 指向 RCC 寄存器的虚拟地址
这类指针通常通过 ioremap() 函数映射物理地址获得。
头文件依赖：
linux/io.h 提供了以下关键函数和宏：
ioremap()：物理地址到内核虚拟地址的映射。
iounmap()：取消地址映射。
readl()/writel()：读写寄存器值的函数。
*/