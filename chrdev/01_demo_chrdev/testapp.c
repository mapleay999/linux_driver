#include <stdio.h> /* 标准C库的头文件 */
#include <string.h>/* 标准C库的头文件 */
#include <fcntl.h> /* UnixC系统调用头文件 open() */
#include <unistd.h>/* UnixC系统调用头文件 close() */

#define DEVICE_FILE "/dev/mapleay-chrdev-device"
#define BUFFER_SIZE 100


/*
1. write_buf 使用字符串字面量初始化，合法OK。 
字符串字面量会被自动转换为字符数组，隐含 \0 终止符。
若数组长度 > 字符串长度，剩余元素填充 \0。
若数组长度 < 字符串长度（含 \0），编译报错。
2. 关于字符数组的长度计算等问题：
char write_buf[BUFFER_SIZE] = "Mapleay: 这段文字来自应用程序的写缓冲区数据内容！";
这个字符串初始化 write_buf 数组后，字符数组被占用 69 个字节，因为：
1. 代码使用 UTF-8 编码，故单个汉字占用3字节。一共20个汉字x3字节=60个字节；
2. 单个英文字符占用 1 个字节，一共9个英文字符（加上空格）。
3. 字符串长度，存储在字符数组时，会在字符串结尾处多存放一个'\0'数据字节，但不计入长度。
 */
char write_buf[BUFFER_SIZE] = "Mapleay: 这段文字来自应用程序的写缓冲区数据内容！";
char read_buf [BUFFER_SIZE];
char* g_test_arr_buf = "纯应用程序测试的字符串内容！";

// 通用函数：打印字符串数组（需显式传递长度）
int print_string_array(char *arr, int length) {
    
    if (arr == NULL) 
    {
        printf("print_string_array：失败：字符串数组地址非法！\n");
        return -1;
    }
    
    for (int i = 0; i < length; i++) {
        if (arr[i] != '\0') { 
            /* printf里是"%c"而不是"%s"，"%s" 是当字符串来处理的！*/
            printf("%c", arr[i]); 
        }
    }
    printf("\n");
    return 0;
}

int main(void) {
    
    // 1. 打开设备文件
    int fd = open(DEVICE_FILE, O_RDWR, 0777);
    if (fd < 0) {
        perror("Failed to open device file");
        return -1;
    }
    
    // 2. 写入测试数据：这里的 write 函数是UC的系统调用函数。
    ssize_t write_bytes = write(fd, write_buf, strlen(write_buf));
    if (write_bytes < 0) {
        perror("Failed to write to device");
    } else {
        printf("已将 %zd 个字节的数据写入设备。\n", write_bytes);
    }
    
    sleep(1);//避免打印混乱:延迟 1 秒
    
    // 3. 读取数据验证：这里的 read 函数是UC的系统调用函数。
    lseek(fd, 0, SEEK_SET); //调整偏移
    ssize_t read_bytes = read(fd, read_buf, sizeof(read_buf));
    if (read_bytes < 0) {
        perror("Failed to read from device");
    } else {
        read_buf[read_bytes] = '\0';
        printf("已从设备读取到 %zd 个字节。\n", read_bytes);
    }
    
    printf("应用程序：打印从内核读取到的字符数组的值： \n");
    //printf("直接使用printf打印字符串： %s\n", g_test_arr_buf);
    print_string_array(read_buf, sizeof(read_buf));
    
    // 4. 关闭设备
    sleep(1); //避免打印混乱：延迟 1 秒
    close(fd);
    return 0;
}