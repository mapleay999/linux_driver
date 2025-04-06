#!/bin/bash

# sudo cp -f /home/ericedward/linux_space/linux_driver/chrdev/01_chrdev/**.ko /home/ericedward/linux_space/tools/nfs/rootfs/home/maple/linux_driver/chrdev/01_chrdev/

#!/bin/bash

# 配置源目录和目标目录
SOURCE_DIR="/home/ericedward/linux_space/linux_driver/chrdev/01_chrdev"
TARGET_DIR="/home/ericedward/linux_space/tools/nfs/rootfs/home/maple/linux_driver/chrdev/01_chrdev"
FILE_EXT=".ko"  # 明确指定文件类型

# 日志记录
LOG_FILE="/var/log/ko_file_copy_$(date +%Y%m%d).log"
exec > >(tee -a "$LOG_FILE") 2>&1  # 同时输出到屏幕和日志文件

echo "===== 开始执行 $(date) ====="

# 检查目录有效性
check_directory() {
    if [ ! -d "$1" ]; then
        echo "错误：目录 $1 不存在"
        return 1
    fi
    return 0
}

# 主执行函数
copy_ko_files() {
    echo "正在复制 $FILE_EXT 文件..."
    
    # 使用find命令确保精确匹配
    find "$SOURCE_DIR" -maxdepth 1 -type f -name "*$FILE_EXT" -print0 | while IFS= read -r -d $'\0' file; do
        echo "正在处理: $(basename "$file")"
        sudo cp -fv "$file" "$TARGET_DIR/"
    done
    
    echo "复制操作完成"
}

# 主程序
main() {
    check_directory "$SOURCE_DIR" || exit 1
    check_directory "$TARGET_DIR" || {
        echo "尝试创建目标目录..."
        sudo mkdir -p "$TARGET_DIR" || {
            echo "无法创建目标目录"
            exit 1
        }
    }
    
    copy_ko_files
}

main

echo "===== 执行结束 $(date) ====="



