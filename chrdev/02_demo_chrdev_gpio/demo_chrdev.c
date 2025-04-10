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


#define CHRDEV_IOC_MAGIC   'k'
#define CLEAR_BUF              _IO(CHRDEV_IOC_MAGIC, 0)
#define GET_BUF_SIZE           _IOR(CHRDEV_IOC_MAGIC, 1, int)
#define GET_DATA_LEN           _IOR(CHRDEV_IOC_MAGIC, 2, int)
#define MAPLEAY_UPDATE_DAT_LEN _IOWR(CHRDEV_IOC_MAGIC, 3, int)
#define CHRDEV_IOC_MAXNR    3

#define DEVICE_NAME "mapleay-chrdev-device"
#define CLASS_NAME  "mapleay-chrdev-class"
#define MINOR_BASE  0     /* 次设备号起始编号为 0 */
#define MINOR_COUNT 1     /* 次设备号的数量为 1   */
#define BUF_SIZE    1024  /* 内核缓冲区大小       */


static dev_t dev_num;
static struct cdev   chrdev;
static struct class  *chrdev_class;
static struct device *chrdev_device;

struct chrdev_data_t {
    char  *buffer;         /* 内核缓冲区 */
    size_t buf_size;       /* 缓冲区大小 */
    size_t data_len;       /* 当前数据长度 */
};

static struct chrdev_data_t dev_data;


static int chrdev_open(struct inode *inode, struct file *filp) {
    filp->private_data = &dev_data;  
    printk(KERN_INFO "内核 chrdev_open：设备已被 pid %d 打开！\n", current->pid);
    return 0;
}


loff_t chrdev_llseek (struct file *filp, loff_t offset, int whence){
    
    switch (whence) {
        case SEEK_SET: //从文件头部偏移 offset 个位置。
                if (offset < 0) return -EINVAL; /* 禁止负偏移 */
                filp->f_pos = offset;
                break; /* 设定具体的偏移位置 */

        case SEEK_CUR://从当前位置偏移 offset 个位置。
                if (offset + filp->f_pos < 0) return -EINVAL; /* 禁止越界偏移 */
                filp->f_pos += offset;
                break;

        case SEEK_END: //从文件末尾偏移 offset 个位置。
                filp->f_pos = i_size_read(file_inode(filp)) + offset; 
                break;

        default: return -EINVAL; /* 禁止隐式偏移 */
    }
    return filp->f_pos;
}


static ssize_t chrdev_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
    
    struct chrdev_data_t *data = filp->private_data;  
    size_t to_read = min_t(size_t, len, data->data_len - *off); 
    
    if (to_read == 0) {
        printk(KERN_INFO "内核 chrdev_read：内核数据读出完毕！\n");
        return 0;
    }
    
    if (copy_to_user(buf, data->buffer + *off, to_read)) {
        printk(KERN_ERR "内核 chrdev_read：复制数据到用户空间操作失败！\n");
        return -EFAULT;
    }
    
    *off += to_read;
    printk(KERN_INFO "内核 chrdev_read：已从内核读出 %zu 字节的数据，当下偏移位于 %lld处。\n", to_read, *off);
    return to_read;
}



static ssize_t chrdev_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {
    struct chrdev_data_t *data = filp->private_data;
    size_t to_write = min_t(size_t, len, data->buf_size - *off); //min，二进制安全，取小。OK。
    
    if (to_write == 0) {
        printk(KERN_INFO "内核 chrdev_write：缓冲区已满，无法继续写入！\n");
        return -ENOSPC;
    }
    
    if (copy_from_user(data->buffer + *off, buf, to_write)) {
        printk(KERN_ERR "内核 chrdev_write：从用户空间复制数据到内核空间的操作失败！\n");
        return -EFAULT;
    }
    
    *off += to_write;
    data->data_len = max_t(size_t, data->data_len, *off); //max，二进制安全，取大。OK。
    printk(KERN_INFO "内核 chrdev_write：已写入内核： %zu 字节的数据，当下偏移位位于 %lld 处。\n", to_write, *off);
    return to_write;
}

static long chrdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg) {
    struct chrdev_data_t *data = filp->private_data;
    int ret = 0;
    int val = 0;

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

        default:
            return -ENOTTY;
    }
    return ret;
}

static int chrdev_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "内核 chrdev_release：设备已被 pid 为 %d 的进程释放！\n", current->pid);
    return 0;
}

static struct file_operations fops = {
    .owner          = THIS_MODULE,
    .llseek         = chrdev_llseek,
    .open           = chrdev_open,
    .read           = chrdev_read,
    .write          = chrdev_write,
    .unlocked_ioctl = chrdev_ioctl,
    .release        = chrdev_release,
};

static int __init chrdev_init(void) {
    
    int err = 0;
    
    /* 1. 申请主设备号：动态申请方式（推荐方式） */
    if (alloc_chrdev_region(&dev_num, MINOR_BASE, MINOR_COUNT, DEVICE_NAME))
    {
        printk("chrdev_init: 分配 chrdev 的字符设备号操作失败！！！\n");
        return -ENODEV;
    }
    printk("chrdev_init: 分配主设备号： %d 次设备号： %d 成功。\n", MAJOR(dev_num), MINOR(dev_num));
    
    /* 2. 初始化缓冲区 */
    dev_data.buffer = kmalloc(BUF_SIZE, GFP_KERNEL);
    if (!dev_data.buffer) {
        err = -ENOMEM;
        goto fail_buffer;
    }
    dev_data.buf_size = BUF_SIZE;
    dev_data.data_len = 0;
    
    /* 3. 初始化 cdev 结构体 */
    cdev_init(&chrdev, &fops);
    
    /* 4. 注册 cdev 结构体到 Linux 内核 */
    if (cdev_add(&chrdev, dev_num, MINOR_COUNT) < 0)
    {
        printk("chrdev_init: 添加 chrdev 字符设备失败！！！\n");
        goto fail_cdev;
    }
    
    /* 5. 创建设备类 */
    chrdev_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(chrdev_class))
    {
        err = PTR_ERR(chrdev_class);
        printk(KERN_ERR"创建设备类失败！错误代码：%d\n", err);
        goto fail_class;
    }
    
    /* 6. 创建设备节点：udev触发机制，自动创建设备节点。手动mknod创建方式已被淘汰！ */
    chrdev_device = device_create(chrdev_class, NULL, dev_num, NULL, DEVICE_NAME);
    if (IS_ERR(chrdev_device))
    {
        err = PTR_ERR(chrdev_device);
        printk(KERN_ERR"创建设备节点失败！错误代码：%d\n", err);
        goto fail_device;
    }
    
    printk(KERN_INFO "chrdev_init:Hello Kernel! 模块已加载！\r\n"); 
    return 0;

fail_device:
    class_destroy(chrdev_class);
fail_class:
    cdev_del(&chrdev);
fail_cdev:
    kfree(dev_data.buffer);
fail_buffer:
    unregister_chrdev_region(dev_num, MINOR_COUNT);

    return err;
}

static void __exit chrdev_exit(void) {
    
    /* 1. 销毁设备节点和设备类 */
    device_destroy(chrdev_class, dev_num);
    class_destroy(chrdev_class);
    
    /* 2. 注销cdev */
    cdev_del(&chrdev);
    
    /* 3. 释放设备号 */
    unregister_chrdev_region(dev_num, MINOR_COUNT);
    
    /* 4. 释放缓冲区 */
    kfree(dev_data.buffer);
    
    printk(KERN_INFO "chrdev_exit:Goodbye Kernel! 模块已卸载！\r\n");
}

module_init(chrdev_init);
module_exit(chrdev_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mapleay");
MODULE_DESCRIPTION("A DEMO character device driver by Mapleay.");