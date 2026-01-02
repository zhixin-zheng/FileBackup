# FileBackup

中文 | [English](./README.md)

本项目包含 C++ 编写的高性能备份核心以及基于 PyQt6 的图形界面。

## 功能特点
- **核心**：基于 C++17 的高效文件遍历、压缩、加密和打包后端。
- **安全性**：集成 OpenSSL，确保备份数据的安全。
- **可扩展性**：通过 pybind11 提供 Python 绑定，方便二次开发。
- **图形界面**：现代化的 PyQt6 界面，操作简便。

## 环境搭建

### 1. 预装环境

#### A 系统要求

- CMake（版本 3.15 及以上）
- 支持 C++17 的编译器（如 GCC 7+、Clang 5+、MSVC 2017+）
- Python 3.8 及以上

#### B 安装依赖

**Ubuntu/Debian 系统：**

打开终端并运行以下命令安装所有依赖：

```bash
sudo apt update
sudo apt install -y build-essential cmake python3 python3-venv libssl-dev pybind11-dev
```

**macOS 系统：**

如果您安装了 [Homebrew](https://brew.sh/)，请运行：

```bash
brew install cmake openssl pybind11
```

*注意：Homebrew 安装的 OpenSSL 路径可能需要手动在 CMakeLists.txt 中指定，或设置环境变量 `OPENSSL_ROOT_DIR`。*


### 2. 设置 Python 环境

首先，设置 Python 虚拟环境，确保 CMake 能找到正确的 Python 版本和库。

如果没有现成的虚拟环境，建议通过以下指令在项目根目录创建一个新的虚拟环境：

```bash
python3 -m venv venv
source venv/bin/activate
```

然后，安装 PyQt6（先激活虚拟环境）：

```bash
pip install PyQt6
```

### 3. 构建 C++ 核心

在虚拟环境激活的情况下手动运行以下命令：

```bash
cmake -S core -B build
cmake --build build
```

我们提供了一个自动化编译脚本。该脚本会自动创建 `build` 文件夹，调用 CMake 进行配置并执行编译，最终生成 `backup_core_py` 模块。

Linux/macOS:

```bash
bash setup.sh
```
## 使用说明
确保您在项目根目录并且虚拟环境已激活，然后运行：

```bash
python ./gui/app.py
```

## 测试

要运行 C++ 单元测试（基于 GoogleTest），请执行以下命令：

```bash
cd build
ctest --output-on-failure
``` 
您也可以直接从构建目录运行特定的测试可执行文件：

```bash
./test_traverser
./test_packer
# 等等。
```

## 项目结构

- `core/`：C++ 源代码、头文件和 CMake 配置。
- `gui/`：Python 图形界面应用程序。
- `build/`：构建产物（在设置过程中创建）。