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
#include "chrdev_ioctl.h"
#include "chrdev.h"         /* 自定义内核字符设备驱动框架信息 */
#include "stm32mp157d.h"    /* 自定义内核控制的硬件相关信息 */


static chrdev_t chrdev;

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
        printk(KERN_INFO "内核 chrdev_read：内核数据读出完毕！\n");
        return 0;
    }

    if (copy_to_user(buf, data->buffer + *off, cnt_read)) {
        printk(KERN_ERR "内核 chrdev_read：复制数据到用户空间操作失败！\n");
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
        printk(KERN_INFO "内核 chrdev_write：缓冲区已满，无法继续写入！\n");
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
        printk(KERN_INFO "内核缓冲区内容：data->buffer[0]的值是：%u\n", data->buffer[0]);
        printk(KERN_ERR "硬件控制失败！\n");
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

static int __init chrdev_init(void) {
    
    int err = 0;
    /* 0. 初始化 stm32mp157d 的 gpio 硬件部分 */
    led_init();

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
    led_deinit();

    return err;
}

static void __exit chrdev_exit(void) {
    
    /* 0. 注销硬件资源：解除内核中注册的引脚映射 */
    led_deinit();
    
    /* 1. 销毁设备节点和设备类 */
    device_destroy(chrdev.dev_class, chrdev.dev_num);
    class_destroy(chrdev.dev_class);
    
    /* 2. 注销cdev */
    cdev_del(&chrdev.dev);
    
    /* 3. 释放设备号 */
    unregister_chrdev_region(chrdev.dev_num, MINOR_COUNT);
    
    /* 4. 释放缓冲区 */
    kfree(chrdev.dev_data.buffer);
    
    printk(KERN_INFO "chrdev_exit:Goodbye Kernel! 模块已卸载！\r\n");
}

module_init(chrdev_init);
module_exit(chrdev_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mapleay");
MODULE_DESCRIPTION("A DEMO character device driver by Mapleay.");