/* UTF-8编码 Unix(LF) */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>  /* 用户空间数据拷贝 */
#include <linux/slab.h>     /* 内核内存分配    */
#include <linux/ioctl.h>    /* ioctl相关定义   */
#include <linux/string.h>   /* 内核的 strlen() */
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/string.h>  /* 用内核自己的strcmp函数 */
#include "chrdev_ioctl.h"
#include "chrdev.h"         /* 自定义内核字符设备驱动框架信息 */
#include "stm32mp157d.h"
#include <linux/mod_devicetable.h> /* struct platform_device_id */
#include <linux/io.h>
#include <linux/of.h>          /* device-tree */
#include <linux/of_address.h>  /* device-tree */

static chrdev_t chrdev; //字符设备对象结构体（自定义的）
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

/*************************************实际受控的硬件（GPIO）驱动代码：开始***************************************************/
/* 初始化 LED */ 
void led_init(void)
{
    u32 val = 0;

    /* 1、寄存器地址映射 */ 
    // 这部分代码，转移到了平台设备驱动模型中，前面已经解析出硬件信息，并做了这部分的映射。
    
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

/*************************************实际受控的硬件（GPIO）驱动代码：结束***************************************************/

static int dev_open(struct inode *inode, struct file *filp) {
    filp->private_data = &chrdev.dev_data;
    printk(KERN_INFO "内核 chrdev_open：设备已被 pid %d 打开！\n", current->pid);
    return 0;
}

/* llseek 函数跟字符设备驱动的数据，可以无任何直接关联，只通过 offset 间接关联。 */
/* loff_t 类型是 signed long long 有符号数值 */
loff_t dev_llseek (struct file *filp, loff_t offset, int whence){

    struct cdev_private_data_t *data = filp->private_data;
    loff_t new_pos = 0;
    /* 对进程的 f_ops 进行校验，防止意外 */
    if ((filp->f_pos < 0) || (filp->f_pos > data->buf_size)) {
        return -EINVAL;
    }

    switch (whence) {
        case SEEK_SET:
                if ((offset < 0) || (offset > data->buf_size)) {
                    printk(KERN_INFO "内核 dev_llseek:SEEK_SET:文件偏移失败！！\n");
                    return -EINVAL;
                }
                filp->f_pos = offset;
                break;

        case SEEK_CUR:
                new_pos = filp->f_pos + offset;
                if ((new_pos < 0) || (new_pos > data->buf_size)){
                    printk(KERN_INFO "内核 dev_llseek:SEEK_CUR:文件偏移失败！！\n");
                    return -EINVAL;
                }
                filp->f_pos = new_pos;
                break;

        case SEEK_END:
                new_pos = data->buf_size + offset;
                if ((new_pos < 0) || (new_pos > data->buf_size)){
                    printk(KERN_INFO "内核 dev_llseek:SEEK_END:文件偏移失败！！\n");
                    return -EINVAL;
                }
                filp->f_pos = new_pos;
                break;

        default: return -EINVAL; /* 禁止隐式偏移 */
    }
    return filp->f_pos;
}

/* 提示：read/write处理风格都是：二进制安全型！所以使用char类型代表单个字节，所有以单个字节的操作都是安全且兼容性强的 */
static ssize_t dev_read(struct file *filp, char __user *buf, size_t len_to_meet, loff_t *off) {

    struct cdev_private_data_t *data = filp->private_data;
    size_t cnt_read = min_t(size_t, len_to_meet, data->data_len - *off); //min截短

    if (cnt_read == 0) {
        printk(KERN_INFO "内核 chrdev_read：内核数据早已读出完毕！无法继续读出！\n");
        return 0;
    }

    if (copy_to_user(buf, data->buffer + *off, cnt_read)) {
        printk(KERN_ERR "内核 chrdev_read：从内核复制数据到用户空间操作失败！\n");
        return -EFAULT;
    }

    *off += cnt_read;
    printk(KERN_INFO "内核 chrdev_read：已从内核读出 %zu 字节的数据，当下偏移位于 %lld处。\n", cnt_read, *off);
    return cnt_read;
}

/* 提示：read/write处理风格都是：二进制安全型！所以使用char类型代表单个字节，所有以单个字节的操作都是安全且兼容性强的 */
static ssize_t dev_write(struct file *filp, const char __user *buf, size_t len_to_meet, loff_t *off) {
    struct cdev_private_data_t *data = filp->private_data;
    size_t cnt_write = min_t(size_t, len_to_meet, data->buf_size - *off); //min，二进制安全，取小。OK。
    
    if (cnt_write == 0) {
        printk(KERN_INFO "内核 chrdev_write：内核缓冲区已满，无法继续写入！\n");
        return -ENOSPC;
    }
    
    if (copy_from_user(data->buffer + *off, buf, cnt_write)) {
        printk(KERN_ERR "内核 chrdev_write：从用户空间复制数据到内核空间的操作失败！\n");
        return -EFAULT;
    }

    /* 硬件LED灯控制部分: 提示，注意 data->buffer[0] 表示缓冲区第0位。而不是 (*off) */
    if(data->buffer[0] == LEDON) {
        led_switch(LEDON);  /* 打开 LED 灯  */
    }
    else if(data->buffer[0] == LEDOFF) { 
        led_switch(LEDOFF);  /* 关闭 LED 灯  */
    }
    else{
        printk(KERN_INFO "内核缓冲区内容：data->buffer[0]的值是：%d\n", data->buffer[0]);
        printk(KERN_ERR "硬件操控失败！\n");
    }

    *off += cnt_write;
    data->data_len = max_t(size_t, data->data_len, *off); //max，二进制安全，取大。OK。
    printk(KERN_INFO "内核 chrdev_write：已写入内核： %zu 字节的数据，当下偏移位位于 %lld 处。\n", cnt_write, *off);
    return cnt_write;
}

static long dev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    struct cdev_private_data_t *data = filp->private_data;
    int ret = 0;
    int val = 0;
    int i   = 0;

    /* 验证命令有效性 */
    if (_IOC_TYPE(cmd) != CHRDEV_IOC_MAGIC) return -ENOTTY;
    if (_IOC_NR(cmd) > CHRDEV_IOC_MAXNR) return -ENOTTY;

    switch (cmd) {
        case CLEAR_BUF:  /* 清除缓冲区 */
            data->data_len = 0;
            memset(data->buffer, 0, data->buf_size);
            printk(KERN_INFO "ioctl: 缓冲区已清空\n");
            break;

        case GET_BUF_SIZE:  /* 获取缓冲区大小 */
            val = data->buf_size;
            if (copy_to_user((int __user *)arg, &val, sizeof(val)))
                return -EFAULT;
            break;

        case GET_DATA_LEN:  /* 获取当前数据长度 */
            val = data->data_len;
            if (copy_to_user((int __user *)arg, &val, sizeof(val)))
                return -EFAULT;
            break;

        case MAPLEAY_UPDATE_DAT_LEN:  /* 自定义：更新有效数据长度 */
            if (copy_from_user(&val, (int __user *)arg, sizeof(arg)))
                return -EFAULT;
            data->data_len = val;  //设置有效数据长度
            val = 12345678;        //特殊数字 仅用来测试 _IORW 的返回方向。
            if (copy_to_user((int __user *)arg, &val, sizeof(val)))
                return -EFAULT;
            break;
        case PRINT_BUF_DATA:
            printk(KERN_INFO"内核操作：打印当前缓冲区的值：开始：\n");
            for (i = 0; i < data->data_len; i++){
                printk(KERN_INFO "%d ", data->buffer[i]);
            }
            printk(KERN_INFO"内核操作：打印当前缓冲区的值：结束。\n");
            break;

        default:
            return -ENOTTY;
    }
    return ret;
}

static int dev_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "内核 chrdev_release：设备已被 pid 为 %d 的进程释放！\n", current->pid);
    return 0;
}

static struct file_operations fops = {
    .owner          = THIS_MODULE,
    .llseek         = dev_llseek,
    .open           = dev_open,
    .read           = dev_read,
    .write          = dev_write,
    .unlocked_ioctl = dev_ioctl,
    .release        = dev_release,
};

static int chrdev_init(void) {
    
    int err = 0;

    /* 1. 申请主设备号：动态申请方式（推荐方式） */
    if (alloc_chrdev_region(&chrdev.dev_num, MINOR_BASE, MINOR_COUNT, DEVICE_NAME))
    {
        printk("chrdev_init: 分配 chrdev 的字符设备号操作失败！！！\n");
        err = -ENODEV;
        goto fail_devnum;
    }
    printk("chrdev_init: 分配主设备号： %d 次设备号： %d 成功。\n", MAJOR(chrdev.dev_num), MINOR(chrdev.dev_num));
    
    /* 2. 初始化缓冲区 */
    chrdev.dev_data.buffer = kmalloc(BUF_SIZE, GFP_KERNEL);
    if (!chrdev.dev_data.buffer) {
        err = -ENOMEM;
        goto fail_buffer;
    }
    chrdev.dev_data.buf_size = BUF_SIZE;
    /* kmalloc 成功分配内存后，返回的指针指向的内存区域包含未定义的数据（可能是旧数据或随机值）。 */
    /* kzalloc 会初始化为0，但性能弱于 kmalloc */
    memset(chrdev.dev_data.buffer, 0, chrdev.dev_data.buf_size);//可以不用执行
    chrdev.dev_data.data_len = 0;
    
    /* 3. 初始化 cdev 结构体 */
    cdev_init(&chrdev.dev, &fops);
    
    /* 4. 注册 cdev 结构体到 Linux 内核 */
    if (cdev_add(&chrdev.dev, chrdev.dev_num, MINOR_COUNT) < 0)
    {
        printk("chrdev_init: 添加 chrdev 字符设备失败！！！\n");
        goto fail_cdev;
    }
    
    /* 5. 创建设备类 */
    chrdev.dev_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(chrdev.dev_class))
    {
        err = PTR_ERR(chrdev.dev_class);
        printk(KERN_ERR"创建设备类失败！错误代码：%d\n", err);
        goto fail_class;
    }
    
    /* 6. 创建设备节点 */
    chrdev.dev_device = device_create(chrdev.dev_class, NULL, chrdev.dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(chrdev.dev_device))
    {
        err = PTR_ERR(chrdev.dev_device);
        printk(KERN_ERR"创建设备节点失败！错误代码：%d\n", err);
        goto fail_device;
    }
    
    printk(KERN_INFO "chrdev_init:Hello Kernel! 模块已加载！\r\n"); 
    return 0;

fail_device:
    class_destroy(chrdev.dev_class);
fail_class:
    cdev_del(&chrdev.dev);
fail_cdev:
    kfree(chrdev.dev_data.buffer);
fail_buffer:
    unregister_chrdev_region(chrdev.dev_num, MINOR_COUNT);
fail_devnum:
    ;

    return err;
}

static void chrdev_exit(void) {
    
    /* 1. 销毁设备节点和设备类 */
    device_destroy(chrdev.dev_class, chrdev.dev_num);
    class_destroy(chrdev.dev_class);
    
    /* 2. 注销cdev */
    cdev_del(&chrdev.dev);
    
    /* 3. 释放设备号 */
    unregister_chrdev_region(chrdev.dev_num, MINOR_COUNT);
    
    /* 4. 释放缓冲区 */
    kfree(chrdev.dev_data.buffer);
    
    printk(KERN_INFO "chrdev_exit:Goodbye Kernel! 字符设备模块已卸载！\r\n");
}

static int led_probe(struct platform_device *pdev)
{
    int ret = 0;
    u32 regdata[12]; 
    const char* str;
    struct property *proper;
    

    /* 1. 获取设备节点 */
    chrdev.nd = of_find_node_by_path("/stm32mp1_led");
    if (chrdev.nd == NULL) {
        printk(KERN_ERR "设备树：解析设备节点失败！");
        return -EINVAL;
    } else {
        printk("设备树：已获取到设备节点：stm32mp1_led node found!\r\n");
    }

    /* 2. 获取 compatible 属性内容 */
    proper = of_find_property(chrdev.nd, "compatible", NULL);
    if (proper == NULL) {
        printk(KERN_ERR "设备树：解析属性内容失败！");
        return -EINVAL;
    } else {
        printk("compatible = %s\r\n", (char*)proper->value);
    }

    /* 3. 获取 status 属性内容 */ 
    ret = of_property_read_string(chrdev.nd, "status", &str);
    if (ret < 0) {
        printk(KERN_ERR "设备树：解析 status 属性内容失败！");
        return -EINVAL;
    } else {
        printk("status = %s\r\n",str);
    }

    /* 4. 获取 reg 属性内容 */
    ret = of_property_read_u32_array(chrdev.nd, "reg", regdata, 12);
    if (ret < 0) {
        printk(KERN_ERR "设备树：解析 reg 属性内容失败！");
        return -EINVAL;
    } else {
        u8 i = 0;
        printk("reg data:\r\n");
        for(i = 0; i < 12; i++) 
            { printk("%#X ", regdata[i]); }
        printk("\r\n"); 
    }

    /* 获取完硬件信息后，开始初始化 LED */ 
    /* 0. 寄存器地址映射 */ 
    MPU_AHB4_PERIPH_RCC_PI = of_iomap(chrdev.nd, 0);
    GPIOI_MODER_PI         = of_iomap(chrdev.nd, 1);
    GPIOI_OTYPER_PI        = of_iomap(chrdev.nd, 2);
    GPIOI_OSPEEDR_PI       = of_iomap(chrdev.nd, 3);
    GPIOI_PUPDR_PI         = of_iomap(chrdev.nd, 4);
    GPIOI_BSRR_PI          = of_iomap(chrdev.nd, 5);

    led_init(); //初始化LED硬件

    /* 2. 注册字符设备 */
    if(chrdev_init()){
        led_deinit();
        return -1;
    }
    
    return 0;
}

static int led_remove(struct platform_device *pdev)
{
    /* 0. 注销硬件资源：解除内核中注册的引脚映射 */
    led_deinit();
    chrdev_exit();
    printk(KERN_INFO "平台设备驱动框架:platform_driver:led_remove：正在被调用！\n");
    return 0;
}


static const struct of_device_id dts_driver_of_match[] = {
    { .compatible = "Mapleay-MP157d-led" }, // 匹配设备树中的 compatible 值
    { } // 终止符
};

static struct platform_driver chrdev_platform_drv = {
    .probe  = led_probe,
    .remove = led_remove,
    .driver = {
              /* .name 字段：只需要给不匹配的任意字符串即可触发设备树匹配。但是！！
               *             第一：不能不初始化这个字段！！
               *             第二：不能初始化为 NULL ！！ 否则模块崩溃！！
               */
              .name = "not_matched_strs",
              .of_match_table = dts_driver_of_match, //使用设备树方式
    },
};

static int __init chrdev_drv_init(void)
{
    return platform_driver_register(&chrdev_platform_drv); 
}

static void __exit chrdev_drv_exit(void)
{
    platform_driver_unregister(&chrdev_platform_drv);
}

module_init(chrdev_drv_init);
module_exit(chrdev_drv_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mapleay");
MODULE_DESCRIPTION("the driver partition of a demo of platform device driver model.");
