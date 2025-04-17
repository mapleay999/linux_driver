//chrdev_platform_device.c文件：描述硬件信息
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include "stm32mp157d.h"

/*
0. 驱动和设备分离程度的思考及linux支持的注入接口：
   0.1 驱动（操作函数）和设备（硬件信息，以C语言中的各种数据结构体现）的划分程度，完全自定义，
       标准就是合理且够用，有些不需要给用户空间暴露出的操纵接口的硬件参数，可以写死在驱动中。
       这里的实例是操纵led灯，所以干脆极端一点，将所有硬件信息都注入内核，供调用，反正工作量
       不大。
   0.2 既然分离后需要分别注入内核，那么就要入乡随俗，遵循linux内相关子系统的框架规则，注入。
       入乡随俗的方式有两种：传统的驱动c文件+设备c文件方式；推荐的驱动c文件+设备树方式。
       本次实例，采用传统的驱动c文件+设备c文件，后续会推进到第二种使用设备树的方式。
       
1. 向内核注入设备的硬件信息：resource 硬件信息的挂接 + platform_data 硬件信息的挂接，一共两种。
   思考：哪些信息需要挂接到 struct platform_device 的 struct resource ？ 
         哪些信息需要挂接到 struct platform_device 的 struct device 的 (void*) platform_data？
   实例是控制led灯，似乎自己可以任意程度挂接到这俩货那里。
   这里为了符合操作逻辑，也为了全面体验这俩货，所以计划这样：
       GPIO输出高低电平挂接在resource上；
       GPIO的工作配置模式挂接在platform_data上；
       
    //1. 寄存器地址映射
    MPU_AHB4_PERIPH_RCC_PI = ioremap(RCC_MP_AHB4ENSETR, 4);  //2. 使能 PI 时钟:platform_data
    GPIOI_MODER_PI         = ioremap(GPIOI_MODER, 4);        //3. 设置 PI0 通用的输出模式:         platform_data
    GPIOI_OTYPER_PI        = ioremap(GPIOI_OTYPER, 4);       //4. 设置 PI0 为推挽模式:platform_data
    GPIOI_OSPEEDR_PI       = ioremap(GPIOI_OTYPER, 4);       //5. 设置 PI0 为高速:platform_data
    GPIOI_PUPDR_PI         = ioremap(GPIOI_PUPDR, 4);        //6. 设置 PI0 为上拉:platform_data
    GPIOI_BSRR_PI          = ioremap(GPIOI_BSRR, 4);         //7. 开关 LED: resource
*/
/************************ platform_data: 开始 ***************************************/
//此部分针对于模式控制引脚，也就是复用引脚
struct led_pin {  //复用引脚的定义 pin 是复用 multiple pin 的意思
    char *name;
    char *mode;
    unsigned long addr;
};

struct led_platform_data { // ppin: pointer_pin
    struct led_pin *ppin;  //记录所有复用模式引脚的数组首地址
    int pin_cnt;           //记录所有复用模式引脚的数量，也就是数组长度
};

static struct led_pin arr_pin[] = { //复用引脚的声明 arr_pin: array_pin
    {
        .name = "MPU_AHB4_PERIPH_RCC_PI",
        .mode = "RCC_ON",
        .addr = RCC_MP_AHB4ENSETR
    },

    {
        .name = "GPIOI_MODER_PI",
        .mode = "GENERAL_OUTPUT",
        .addr = GPIOI_MODER
    },
    
    {
        .name = "GPIOI_OTYPER_PI",
        .mode = "PUSH_PULL",
        .addr = GPIOI_OTYPER
    },
    
    {
        .name = "GPIOI_OSPEEDR_PI",
        .mode = "HIGH_SPEED",
        .addr = GPIOI_OTYPER
    },
    
    {
        .name = "GPIOI_PUPDR_PI",
        .mode = "UP_PULL",
        .addr = GPIOI_PUPDR
    }

};

static struct led_platform_data led_platform_info = {
    .ppin     = arr_pin,             //复用引脚做在的数组首地址
    .pin_cnt  = ARRAY_SIZE(arr_pin)  //复用引脚的数量
};
/************************ platform_data: 结束 ***************************************/
/************************ resource data: 开始 ***************************************/
//此部分针对输入输出引脚：直接使用内核定义的标注数据结构：struct resource

static struct resource led_resource[] = { //只有一个led_pin，编号暂时随便写为11号
    [0] = { // GPIO控制寄存器地址
        .start = GPIOI_BSRR, 
        .end   = GPIOI_BSRR + 0,
        .flags = IORESOURCE_MEM
    },
    [1] = { // GPIO引脚号为11号
        .start = 11, 
        .end   = 11,
        .flags = IORESOURCE_IRQ
    }
};
/************************ resource data: 结束 ***************************************/


static void led_release(struct device *dev)
{
    printk(KERN_INFO "平台设备驱动框架:paltform_device:led_release：正在被调用！\n");
}


static struct platform_device chrdev_platform_dev = {
        .name          = "mapleay-pltfm-led",
        .id            = -1,
        .num_resources = ARRAY_SIZE(led_resource),
        .resource      = led_resource,                 //注入 resource data：注入标准信息：IO/IRQ/MEM
        .dev           = {
                           .platform_data = &led_platform_info, //注入 platform_data：注入个性化板级信息
                           .release = led_release
         },
};

static int __init chrdev_dev_init(void)
{ 
    return platform_device_register(&chrdev_platform_dev);
}

static void __exit chrdev_dev_exit(void)
{
    platform_device_unregister(&chrdev_platform_dev);
}

module_init(chrdev_dev_init);
module_exit(chrdev_dev_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mapleay");
MODULE_DESCRIPTION("the device partition of a demo of platform device driver model.");
