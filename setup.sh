#!/bin/bash

GREEN='\033[0,32m'
RED='\033[0,31m'
NC='\033[0m'

echo -e "${GREEN}>>> 开始构建 FileBackup 核心模块...${NC}"

# 1. 运行 CMake
echo -e "${GREEN}正在配置 CMake (Source: core, Build: build)...${NC}"
cmake -S core -B build

if [ $? -ne 0 ]; then
    echo -e "${RED}CMake 配置失败，请检查是否安装了必要的依赖（OpenSSL, pybind11）。${NC}"
    exit 1
fi

# 2. 执行编译
echo -e "${GREEN}开始编译核心...${NC}"
cmake --build build --config Release -j$(nproc 2>/dev/null || echo 4)

if [ $? -eq 0 ]; then
    echo -e "${GREEN}>>> 构建成功！${NC}"
    echo "Python 模块已生成在 build 目录下。"
    echo "运行程序: python3 gui/app.py"
else
    echo -e "${RED}编译失败！${NC}"
    exit 1
fi