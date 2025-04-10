#include <stdio.h> /* 标准C库的头文件 */
#include <string.h>/* 标准C库的头文件 */
#include <fcntl.h> /* UnixC系统调用头文件 open() */
#include <unistd.h>/* UnixC系统调用头文件 close() */
#include <sys/ioctl.h>
#include <stdlib.h> /* 使用 atoi() 函数 */

#define DEVICE_FILE "/dev/mapleay-chrdev-device"
#define BUFFER_SIZE 100
/* 必须与内核完全一致的ioctl定义 */
#define CHRDEV_IOC_MAGIC   'k'
#define CLEAR_BUF              _IO(CHRDEV_IOC_MAGIC, 0)
#define GET_BUF_SIZE           _IOR(CHRDEV_IOC_MAGIC, 1, int)
#define GET_DATA_LEN           _IOR(CHRDEV_IOC_MAGIC, 2, int)
#define MAPLEAY_UPDATE_DAT_LEN _IOWR(CHRDEV_IOC_MAGIC, 3, int)

void print_usage(const char *prog_name) {
    printf("Usage:\n");
    printf("  %s  -w  写入数据\n", prog_name);
    printf("  %s  -r  读取数据\n", prog_name);
    printf("  %s  -c  清空缓冲区\n", prog_name);
    printf("  %s  -s  获取缓冲区大小\n", prog_name);
    printf("  %s  -l  获取当前内核缓冲区数据长度\n", prog_name);
    printf("  %s  -u  更新内核缓冲区数据长度\n", prog_name);
}

int main(int argc, char **argv) {
    char read_buf  [BUFFER_SIZE];

    if (argc < 2) {
        print_usage(argv[0]);
        return -1;
    }

    /* 1. 打开设备文件 */
    int fd = open(DEVICE_FILE, O_RDWR, 0777);
    if (fd < 0) {
        perror("应用层：打开设备文件失败！");
        return -1;
    }
    
    /* 2. 读写测试 */
    if ((0 == strcmp(argv[1], "-w")) && (argc == 3)) {
        int len = strlen(argv[2]) + 1;  //+1包含字符串末尾'\0'字节。
        ssize_t cnt_bytes_writen = write(fd, argv[2], (len < BUFFER_SIZE ? len : 100));
        if (cnt_bytes_writen < 0) {
            perror("将应用层数据写入内核设备文件失败！");
        } else {
            printf("已将 %zd 个字节的数据写入设备。\n", cnt_bytes_writen);
        }
    }
    else if ((0 == strcmp(argv[1], "-r")) && (argc == 2)){
        lseek(fd, 0, SEEK_SET);  //调整偏移
        if (read(fd, read_buf, BUFFER_SIZE) <= 0) {  //将内核中安全有效数据全部读出
            perror("从内核设备文件中读取数据失败！");
        } else { //从内核读出的所有二进制安全有效的数据中，截取第一个字符串打印出来。
            printf("已从设备读取到 %zd 个字节： %s \n", strlen(read_buf), read_buf);
        }
    }
    else if ((strcmp(argv[1], "-c") == 0)){
        /* 清空缓冲区 */
        if (ioctl(fd, CLEAR_BUF) < 0) {
            perror("ioctl清除缓冲区失败");
        } else {
            printf("缓冲区已清空！\n");
        }
    }
    else if (strcmp(argv[1], "-s") == 0) {
        /* 获取缓冲区大小 */
        int buf_size;
        if (ioctl(fd, GET_BUF_SIZE, &buf_size) < 0) {
            perror("ioctl获取缓冲区大小失败");
        } else {
            printf("缓冲区总大小: %d字节\n", buf_size);
        }
    } 
    else if (strcmp(argv[1], "-l") == 0) {
        /* 获取数据长度 */
        int data_len;
        if (ioctl(fd, GET_DATA_LEN, &data_len) < 0) {
            perror("ioctl获取数据长度失败");
        } else {
            printf("当前数据长度: %d字节\n", data_len);
        }
    }
    else if (strcmp(argv[1], "-u") == 0) {
        /* 更新数据长度：没啥实际价值，仅测试 ioctl 的 _IOWR 功能 */
        int data_len = atoi(argv[2]);
        if (ioctl(fd, MAPLEAY_UPDATE_DAT_LEN, &data_len) < 0) {
            perror("ioctl获取数据长度失败");
        } else {
            printf("返回数值1~8表示更新成功，返回是： %d ！\n", data_len);
        }
    }
    else {
        printf("非法验证方式：输入参数格式错误！请使用正确的命令格式：\n");
        print_usage(argv[0]);
        close(fd);
        return -1;
    }
    
    // 4. 关闭设备
    sleep(1);//避免UART打印混乱:延迟 1 秒
    close(fd);
    return 0;
}

/*
 * ./testapp.out 的使用方式：
 * ./testapp.out -w "MAPLE-HUANGHELOU-20250409-LMJ-TestBeef!" (39bytes个字符组成的字符串)
 * ./testapp.out -r
 * ./testapp.out -w "AMPLE-TestBeef-2005" (19bytes个字符组成的字符串)
 * ./testapp.out -r
 * ./testapp.out -u "20"
 * ./testapp.out -r
 * ./testapp.out -u "40"
 * ./testapp.out -r
 */