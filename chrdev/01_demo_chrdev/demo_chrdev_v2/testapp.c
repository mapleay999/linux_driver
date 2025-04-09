#include <stdio.h> /* 标准C库的头文件 */
#include <string.h>/* 标准C库的头文件 */
#include <fcntl.h> /* UnixC系统调用头文件 open() */
#include <unistd.h>/* UnixC系统调用头文件 close() */

#define DEVICE_FILE "/dev/mapleay-chrdev-device"
#define BUFFER_SIZE 100


char read_buf [BUFFER_SIZE];


int main(int argc, char **argv) {
    
    /* 0. 判断命令行输入参数 */
    if (argc < 2)
    {
        printf("Usage: %s -w <string>\n", argv[0]);
        printf("       %s -r\n", argv[0]);
        return -1;
    }
    
    /* 1. 打开设备文件 */
    int fd = open(DEVICE_FILE, O_RDWR, 0777);
    if (fd < 0) {
        perror("Failed to open device file");
        return -1;
    }
    
    /* 2. 读写测试：这里的 read/write/lseek 函数是UC的系统调用函数 */
    if ((0 == strcmp(argv[1], "-w")) && (argc == 3))
    {
        int len = strlen(argv[2]) + 1;
        ssize_t cnt_bytes_writen = write(fd, argv[2], (len < BUFFER_SIZE ? len : 100));
        if (cnt_bytes_writen < 0) {
            perror("将应用层数据写入内核设备文件失败！");
        } else {
            printf("已将 %zd 个字节的数据写入设备。\n", cnt_bytes_writen);
        }
    }
    else if ((0 == strcmp(argv[1], "-r")) && (argc == 2)){
        lseek(fd, 0, SEEK_SET); //调整偏移
        ssize_t cnt_bytes_read = read(fd, read_buf, BUFFER_SIZE); //全部读出
        if (cnt_bytes_read < 0) {
            perror("从内核设备文件中读取数据失败！");
        } else {
            cnt_bytes_read = strlen(read_buf); //更新为真实的长度
            //read_buf[cnt_bytes_read] = '\0';
            printf("已从设备读取到 %zd 个字节： %s \n", cnt_bytes_read, read_buf);
        }
    }
    else if ((0 == strcmp(argv[1], "-wr")) && (argc == 3)){
        int len = strlen(argv[2]) + 1;
        ssize_t cnt_bytes_writen = write(fd, argv[2], (len < BUFFER_SIZE ? len : 100));
        if (cnt_bytes_writen < 0) {
            perror("将应用层数据写入内核设备文件失败！");
        } else {
            printf("已将 %zd 个字节的数据写入设备。\n", cnt_bytes_writen);
        }
        
        sleep(1);//避免UART打印混乱:延迟 1 秒
        lseek(fd, 0, SEEK_SET); //调整偏移
        
        ssize_t cnt_bytes_read = read(fd, read_buf, BUFFER_SIZE);
        if (cnt_bytes_read < 0) {
            perror("从内核设备文件中读取数据失败！");
        } else {
            cnt_bytes_read = strlen(read_buf); //更新为真实的长度
            printf("已从设备读取到 %zd 个字节： %s \n", cnt_bytes_read, read_buf);
        }
    }
    else{
        printf("非法验证方式：输入参数格式错误！\n");
    }
    
    // 4. 关闭设备
    sleep(1);//避免UART打印混乱:延迟 1 秒
    close(fd);
    return 0;
}

/*
1. read() 是 Unix C 系统调用 是 二进制安全 的系统调用，会忠实地将内核缓冲区中的字节（包括任何 '\0'）原样复制到用户空间缓冲区。 
2. strlen(const char*) 是标准C库函数，二进制不安全！：
   计算字符串长度：从 str 指向的内存地址开始，逐字节向后扫描，直到遇到 \0（空终止符）。
   终止条件：遇到第一个 \0 停止计数，\0 本身不计入长度。
   时间复杂度：O(n)，需遍历整个字符串。
*/

/*
 * ./testapp.out 的使用方式：
 * ./testapp.out -w  abc  写入
 * ./testapp.out -r       读取
 * ./testapp.out -wr abc  先写后读，比如：
 * ./testapp.out -wr "MAPLE-HUANGHELOU-20250409-LMJ-TestBeef!" (39bytes个字符组成的字符串)
 * ./testapp.out -w "AMPLE-TestBeef-2005" (19bytes个字符组成的字符串)
 * ./testapp.out -r
 * 测试结果如下：
[root@ATK-STM32MP157D]:/home/maple/linux_driver/chrdev/01_chrdev$ insmod demo_ch
rdev.ko
[ 3025.364552] chrdev_init: 分配主设备号： 241 次设备号： 0 成功。
[ 3025.370740] chrdev_init:Hello Kernel! 模块已加载！
[root@ATK-STM32MP157D]:/home/maple/linux_driver/chrdev/01_chrdev$ ./testapp.out
 -wr "MAPLE-HUANGHELOU-20250409-LMJ-TestBeef!"
[ 3042.224188] 内核 chrdev_open：设备已被 pid 245 打开！
[ 3042.228769] 内核 chrdev_write：已写入内核： 40 字节的数据，当下偏移位位于 40 处。
已将 40 个字节的数据写入设备。
[ 3043.239018] 内核 chrdev_read：已从内核读出 40 字节的数据，当下偏移位于 40处。
已从设备读取到 39 个字节： MAPLE-HUANGHELOU-20250409-LMJ-TestBeef!
[ 3044.246944] 内核 chrdev_release：设备已被 pid 为 245 的进程释放！
[root@ATK-STM32MP157D]:/home/maple/linux_driver/chrdev/01_chrdev$
[root@ATK-STM32MP157D]:/home/maple/linux_driver/chrdev/01_chrdev$
[root@ATK-STM32MP157D]:/home/maple/linux_driver/chrdev/01_chrdev$ ./testapp.out
 -w "AMPLE-TestBeef-2005"
[ 3070.227720] 内核 chrdev_open：设备已被 pid 246 打开！
[ 3070.232219] 内核 chrdev_write：已写入内核： 20 字节的数据，当下偏移位位于 20 处。
已将 20 个字节的数据写入设备。
[ 3071.242520] 内核 chrdev_release：设备已被 pid 为 246 的进程释放！
[root@ATK-STM32MP157D]:/home/maple/linux_driver/chrdev/01_chrdev$ ./testapp.out
 -r
[ 3079.716469] 内核 chrdev_open：设备已被 pid 248 打开！
[ 3079.720969] 内核 chrdev_read：已从内核读出 40 字节的数据，当下偏移位于 40处。
已从设备读取到 19 个字节： AMPLE-TestBeef-2005
[ 3080.730806] 内核 chrdev_release：设备已被 pid 为 248 的进程释放！
[root@ATK-STM32MP157D]:/home/maple/linux_driver/chrdev/01_chrdev$
 */