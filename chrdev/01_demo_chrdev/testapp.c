#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define DEVICE_FILE "/dev/mapleay-chr-device"
#define BUFFER_SIZE 100


int main(void) {
    int fd;
    char write_buf[BUFFER_SIZE] = "Test message for driver";
    char read_buf[BUFFER_SIZE];
    
    // 1. 打开设备文件
    fd = open(DEVICE_FILE, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device file");
        return 1;
    }
    
    // 2. 写入测试数据
    ssize_t write_bytes = write(fd, write_buf, strlen(write_buf));
    if (write_bytes < 0) {
        perror("Failed to write to device");
    } else {
        printf("Written %zd bytes to device\n", write_bytes);
    }
    
    // 3. 读取数据验证
    lseek(fd, 0, SEEK_SET); // 重置文件指针
    ssize_t read_bytes = read(fd, read_buf, sizeof(read_buf));
    if (read_bytes < 0) {
        perror("Failed to read from device");
    } else {
        read_buf[read_bytes] = '\0';
        printf("Read from device: %s\n", read_buf);
    }
    
    // 4. 关闭设备
    close(fd);
    return 0;
}