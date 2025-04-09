/* UTF-8编码 Unix(LF) */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>  /* 用户空间数据拷贝 */
#include <linux/slab.h>     /* 内核内存分配    */

#define DEVICE_NAME "mapleay-chrdev-device"
#define CLASS_NAME  "mapleay-chrdev-class"
#define MINOR_BASE  0     /* 次设备号起始编号为 0 */
#define MINOR_COUNT 1     /* 次设备号的数量为 1   */
#define BUF_SIZE    1024  /* 新增：内核缓冲区大小  */

static dev_t dev_num;         /* 设备号    */
static struct cdev   chrdev;  /* 字符设备体 */
static struct class  *chrdev_class;  /* 必须定义为全局变量，跨函数使用 */
static struct device *chrdev_device; /* 推荐定义成全局变量，亦可改为局部变量 */

struct chrdev_data_t {
    char  *buffer;         /* 内核缓冲区 */
    size_t buf_size;       /* 缓冲区大小 */
    size_t data_len;       /* 当前数据长度 */
};

static struct chrdev_data_t dev_data;

/* 定义 chrdev 的 file_operations 接口函数 */
static int chrdev_open(struct inode *inode, struct file *filp) {
	// 关联私有数据：内核进程关联字符设备驱动.
	// filp->private_data 在 struct file 中定义的就是一个 void 类型的指针。
	// 因此 ((struct file*) filp)->private_data 这个 void 类型的指针，可以灵活指向甲方自定义的私有数据空间。
	// 内核的字符设备驱动模块将其自定义数据传递给其匹配的进程，就是通过 filp->private_data 挂接。
    filp->private_data = &dev_data;  
    printk(KERN_INFO "内核 chrdev_open：设备已被 pid %d 打开！\n", current->pid);
    return 0;
}

/* 
 * 为什么要是现在这个 llseek 函数？
 * 因为若不实现此函数，应用程序先write再read就read错误，偏移量错误。
 * 内核中的llseek方法，参数是：
 *   文件指针(filp)、偏移量(offset)、起始位置(whence)
 * 内核中的字符设备驱动接口函数 llseek 操作内核进程文件中 offset
*/
loff_t chrdev_llseek (struct file *filp, loff_t offset, int whence){
    
    switch (whence) {
        case SEEK_SET: //绝对偏移：从文件头偏移 offset 个位置。
                if (offset < 0) return -EINVAL; /* 禁止负偏移 */
                filp->f_pos = offset;
                break; /* 设定具体的偏移位置 */

        case SEEK_CUR://相对偏移：从当前位置偏移 offset 个位置。
                if (offset + filp->f_pos < 0) return -EINVAL; /* 禁止越界偏移 */
                filp->f_pos += offset;
                break;

        case SEEK_END: //相对偏移：从当文件末尾处偏移 offset 个位置。此处没吃透，需要后续仔细研究！！！
                filp->f_pos = i_size_read(file_inode(filp)) + offset; 
                break;

        default: return -EINVAL; /* 禁止隐式偏移 */
    }
    return filp->f_pos;
}

/* 从内核读出数据到用户空间。
 * 提醒：在open后，进程的 filp 确实已被关联挂接到此字符设备(file_operations + chrdev_data)。
 * 提醒：这里的进程自身的信息 filp 也是属于 Linux 内核内部的进程子系统数据部分，不属于外部应用程序！
 * @struct file *filp 指向文件结构体的指针，表示当前打开的设备文件。该结构体包含文件的私有数据
 *                   （如 private_data 成员）、操作标志等信息，驱动可通过它获取设备上下文。
 * @char __user *buf  用户空间缓冲区的指针,需通过 copy_to_user 等安全函数将数据
 *                    从内核复制到用户空间，避免直接解引用。
 * @size_t len  请求读取的数据长度（字节数）。驱动需检查该值是否超过设备缓冲区限制，必要时截断。
 * @loff_t *off 指向文件偏移量的指针，表示读取操作的起始位置。驱动需更新该偏移量。
 *              比如 *off += len，以反映读取后的新位置。loff_t是long long整形的数据类型。
 */
static ssize_t chrdev_read(struct file *filp, char __user *buf, size_t len, loff_t *off) {
    
    struct chrdev_data_t *data = filp->private_data;  //获取进程上挂接的字符设备私有数据部分的指针。
    size_t to_read = min_t(size_t, len, data->data_len - *off); //min 截断，取最小值
    
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


/*  从用户空间写入数据到内核空间。
 * 提醒：在open后，进程的 filp 确实已被关联挂接到此字符设备(file_operations + chrdev_data)。
 * 提醒：这里的进程自身的信息 filp 也是属于 Linux 内核内部的进程子系统数据部分，不属于外部应用程序！
 * @struct file *filp 指向文件结构体的指针，表示当前打开的设备文件。该结构体包含文件的私有数据
 *                   （如 private_data 成员）、操作标志等信息，驱动可通过它获取设备上下文。
 * @const char __user *buf 用户空间缓冲区的指针，存储待写入设备的数据。__user 标记表示该指针
 *                    指向用户空间地址，需通过 copy_from_user 等安全函数访问，避免直接解引用
 * @size_t len    请求写入的数据长度（字节数）。驱动需检查该值是否超过设备缓冲区限制，必要时截断。
 * @loff_t *off   指向文件偏移量的指针，表示写入操作的起始位置。驱动需更新该偏移量。
 *                比如 *off += len，以反映写入后的新位置。loff_t是long long整形的数据类型。
 *
 */
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

static int chrdev_release(struct inode *inode, struct file *file) {
    printk(KERN_INFO "内核 chrdev_release：设备已被 pid 为 %d 的进程释放！\n", current->pid);
    return 0;
}

/* 定义 chrdev 的 file_operations 结构体变量 */
static struct file_operations fops = {
    .owner   = THIS_MODULE,
    .llseek  = chrdev_llseek,
    .open    = chrdev_open,
    .read    = chrdev_read,
    .write   = chrdev_write,
    .release = chrdev_release,
};

static int __init chrdev_init(void) {
    
    int err = 0;
    
    /* 1. 申请主设备号：动态申请方式（推荐方式） */
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
    /* device_destroy 通过设备类和设备号的组合高效定位并销毁设备，
        而非依赖设备指针的间接引用，这是Linux设备模型的设计精髓 */
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


/*
【1】 内核中使用内置比较宏 max min 需要留意的细节：
内核宏实现
min(x, y)最终展开为__careful_cmp(x, y, <)，其核心通过__typecheck宏比较(typeof(x)*)和(typeof(y)*)的指针类型。
类型安全设计
(void)(&_x == &_y)通过指针比较触发编译器类型检查，若类型不同则产生警告，但运行时无实际效果。
1.1 使用gcc -E预处理查看宏展开结果
1.2 通过sizeof(typeof(x))调试确认参数类型是否匹配
*/

/* 
xref: /linux-5.4.290/include/linux/fs.h
struct file_operations {
    struct module *owner;
    loff_t (*llseek) (struct file *, loff_t, int);
    ssize_t (*read) (struct file *, char __user *, size_t, loff_t *);
    ssize_t (*write) (struct file *, const char __user *, size_t, loff_t *);
    ssize_t (*read_iter) (struct kiocb *, struct iov_iter *);
    ssize_t (*write_iter) (struct kiocb *, struct iov_iter *);
    int (*iopoll)(struct kiocb *kiocb, bool spin);
    int (*iterate) (struct file *, struct dir_context *);
    int (*iterate_shared) (struct file *, struct dir_context *);
    __poll_t (*poll) (struct file *, struct poll_table_struct *);
    long (*unlocked_ioctl) (struct file *, unsigned int, unsigned long);
    long (*compat_ioctl) (struct file *, unsigned int, unsigned long);
    int (*mmap) (struct file *, struct vm_area_struct *);
    unsigned long mmap_supported_flags;
    int (*open) (struct inode *, struct file *);
    int (*flush) (struct file *, fl_owner_t id);
    int (*release) (struct inode *, struct file *);
    int (*fsync) (struct file *, loff_t, loff_t, int datasync);
    int (*fasync) (int, struct file *, int);
    int (*lock) (struct file *, int, struct file_lock *);
    ssize_t (*sendpage) (struct file *, struct page *, int, size_t, loff_t *, int);
    unsigned long (*get_unmapped_area)(struct file *, unsigned long, unsigned long, unsigned long, unsigned long);
    int (*check_flags)(int);
    int (*flock) (struct file *, int, struct file_lock *);
    ssize_t (*splice_write)(struct pipe_inode_info *, struct file *, loff_t *, size_t, unsigned int);
    ssize_t (*splice_read)(struct file *, loff_t *, struct pipe_inode_info *, size_t, unsigned int);
    int (*setlease)(struct file *, long, struct file_lock **, void **);
    long (*fallocate)(struct file *file, int mode, loff_t offset,
              loff_t len);
    void (*show_fdinfo)(struct seq_file *m, struct file *f);
#ifndef CONFIG_MMU
    unsigned (*mmap_capabilities)(struct file *);
#endif
    ssize_t (*copy_file_range)(struct file *, loff_t, struct file *,
            loff_t, size_t, unsigned int);
    loff_t (*remap_file_range)(struct file *file_in, loff_t pos_in,
                   struct file *file_out, loff_t pos_out,
                   loff_t len, unsigned int remap_flags);
    int (*fadvise)(struct file *, loff_t, loff_t, int);
    bool may_pollfree;
} __randomize_layout;
*/

/*
在 Linux 内核中，struct file 的定义位于 include/linux/fs.h 头文件中。这是文件操作和文件描述符相关核心数据结构的定义位置。
所在行数：938~976.
struct file 的作用：
表示一个“已打开的文件”的上下文信息。
每个打开的文件（例如通过 open() 系统调用）在内核中对应一个 struct file 实例。
包含文件读写位置（f_pos）、访问模式（f_mode）、文件操作函数集（f_op）等关键信息。

xref: /linux-5.4.290/include/linux/fs.h
所在行数：938~976.
struct file {
    union {
        struct llist_node   fu_llist;
        struct rcu_head     fu_rcuhead;
    } f_u;
    struct path     f_path;
    struct inode        *f_inode;   // cached value 
    const struct file_operations    *f_op;

    spinlock_t      f_lock;
    enum rw_hint        f_write_hint;
    atomic_long_t       f_count;
    unsigned int        f_flags;
    fmode_t         f_mode;
    struct mutex        f_pos_lock;
    loff_t          f_pos;
    struct fown_struct  f_owner;
    const struct cred   *f_cred;
    struct file_ra_state    f_ra;

    u64         f_version;

    // needed for tty driver, and maybe others 
    void            *private_data; //这里 这里！！

    struct address_space    *f_mapping;
    errseq_t        f_wb_err;
} __randomize_layout
*/

/* loff_t 类型定义： long long 整形。
xref: /linux-5.4.290/include/linux/types.h
typedef __kernel_loff_t loff_t;
xref: /linux-5.4.290/include/uapi/asm-generic/posix_types.h
typedef long long __kernel_loff_t;
*/

/*
 * min_t - return minimum of two values, using the specified type
 * @type: data type to use
 * @x: first value
 * @y: second value

#define min_t(type, x, y)   __careful_cmp((type)(x), (type)(y), <)
*/