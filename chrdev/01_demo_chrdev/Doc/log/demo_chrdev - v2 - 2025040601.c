/* UTF-8编码 Unix(LF) */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>

#define DEVICE_NAME "mapleay-chr-device"
#define MINOR_BASE  0 /* 次设备号起始编号为 0 */
#define MINOR_COUNT 1 /* 次设备号的数量为 1   */

static dev_t dev_num;         /* 设备号    */
static struct cdev   chrdev;  /* 字符设备体 */
static struct class  *chrdev_class;  /* 必须定义为全局变量，跨函数使用 */
static struct device *chrdev_device; /* 推荐定义为全局变量，方便其他方面使用，若仅用于排错，可设定为局部变量 */

/* 定义 chrdev 的 file_operations 接口函数 */
static int chrdev_open(struct inode *inode, struct file *file) {
    printk(KERN_INFO "正在执行 open 函数。\n");
    return 0;
}

static ssize_t chrdev_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
    printk(KERN_INFO "读函数还没实现！\n");
    return 0;
}

static ssize_t chrdev_write(struct file *filp, const char __user *buf, size_t len, loff_t *off) {

    printk(KERN_INFO "写函数还没实现！\n");
    return 0;
}

static int chrdev_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "正在执行释放函数。\n");
    return 0;
}

/* 定义 chrdev 的 file_operations 结构体变量 */
static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .open    = chrdev_open,
    .read    = chrdev_read,
    .write   = chrdev_write,
    .release = chrdev_release,
};

static int __init chrdev_init(void) {
	
	int err = 0;
	
	/* 1. 申请主设备号：动态申请方式 */
	/*
	 * @dev_num    :保存内核分配的设备号（包含主设备号和起始次设备号）。
	 *              通过宏 MAJOR(dev_t dev) 和 MINOR(dev_t dev) 可提取主/次设备号。
	 * @MINOR_BASE :请求的起始次设备号（通常设为 0）。
	 * @MINOR_COUNT:连续分配的次设备号数量（例如 1 表示仅分配一个次设备号）。
	 * @DEVICE_NAME:设备名，传入字符串指针。
	*/
	if (alloc_chrdev_region(&dev_num, MINOR_BASE, MINOR_COUNT, DEVICE_NAME))
	{
		printk("chrdev_init: 分配 chrdev 的字符设备号操作失败！！！\n");
		return -1;
	}
	printk("chrdev_init: 分配主设备号： %d 成功。\n", MAJOR(dev_num));
	
	/* 2. 初始化 cdev 结构体 */
	cdev_init(&chrdev, &fops);
	
	/* 3. 注册 cdev 结构体到 Linux 内核 */
	if (cdev_add(&chrdev, dev_num, MINOR_COUNT) < 0)
	{
		unregister_chrdev_region(dev_num, MINOR_COUNT);
		printk("chrdev_init: 添加 chrdev 字符设备失败！！！\n");
		return -1;
	}
	printk("chrdev_init: 添加 chrdev 字符设备成功。\n");
	
	/* 4. 创建设备类 */
	chrdev_class = class_create(THIS_MODULE, DEVICE_NAME);
	if (IS_ERR(chrdev_class))
	{
		err = PTR_ERR(chrdev_class);
		printk(KERN_ERR"创建设备类失败！错误代码：%d\n", err);
		/* 注销前面注册的资源 */
		// [todo]
		return err;
	}
	printk("chrdev_init: 创建设备类成功！\n");
	
	/* 5. 创建设备节点：udev触发机制，自动创建设备节点。手动mknod创建方式已被淘汰！ */
	chrdev_device = device_create(chrdev_class, NULL, dev_num, NULL, DEVICE_NAME);
	if (IS_ERR(chrdev_device))
	{
		err = PTR_ERR(chrdev_device);
		printk(KERN_ERR"创建设备节点失败！错误代码：%d\n", err);
		/* 注销前面注册的资源 */
		// [todo]
		return err;
	}
	printk("chrdev_init: 创建设备节点成功！\n");
	
	printk(KERN_INFO "chrdev_init:Hello Kernel! 模块已加载！\r\n"); 
	
	return 0;
}

static void __exit chrdev_exit(void) {
    
    /* 1. 销毁设备节点和设备类 */
    /* device_destroy 通过设备类和设备号的组合高效定位并销毁设备，而非依赖设备指针的间接引用，这是Linux设备模型的设计精髓 */
    device_destroy(chrdev_class, dev_num);
    class_destroy(chrdev_class);
    
    /* 2. 注销cdev */
    cdev_del(&chrdev);
    
    /* 3. 释放设备号 */
    unregister_chrdev_region(dev_num, MINOR_COUNT);
    
	printk(KERN_INFO "chrdev_exit:Goodbye Kernel! 模块已卸载！\r\n");
}

module_init(chrdev_init);
module_exit(chrdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mapleay");
MODULE_DESCRIPTION("A DEMO character device driver by Mapleay.");

