# FileBackup

[中文](./README_Chinese.md) | English

This project contains a high-performance backup core written in C++ and a graphical interface based on PyQt6.

## Features

- **Core**: C++17 based backend for efficient file traversal, compression, encryption, and packaging.
- **Security**: OpenSSL integration for secure backups.
- **Extensibility**: Python bindings via pybind11 for easy secondary development.
- **GUI**: Modern PyQt6 interface for easy operation.

## Setup

### 1. Setup Prerequisites

#### A System Requirements

- CMake (version 3.15 or higher)
- A C++17 compatible compiler (e.g., GCC 7+, Clang 5+, MSVC 2017+)
- Python 3.8 or higher

#### B Install Dependencies

**Ubuntu/Debian:**

Open a terminal and run the following command to install all dependencies:

```bash
sudo apt update
sudo apt install -y build-essential cmake python3 python3-venv libssl-dev pybind11-dev openssl
```

Additionally, install the required XCB libraries for PyQt6:

```bash
sudo apt update
sudo apt install libxcb-cursor0 libxcb-xinerama0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 libxcb-shape0
```

**macOS:**

If you have installed [Homebrew](https://brew.sh/), please run:

```bash
brew install cmake openssl pybind11
```

*Note: The path to OpenSSL installed by Homebrew might need to be manually specified in CMakeLists.txt, or by setting the environment variable `OPENSSL_ROOT_DIR`.*

### 2. Setup Python Environment

First, set up the Python virtual environment to ensure CMake can find the correct Python version and libraries.

If you don't have an existing virtual environment, it is recommended to create a new one in the project root directory using the following commands:

```bash
python3 -m venv venv

source venv/bin/activate
```

Then, install PyQt6 (activate the virtual environment first):

```bash
pip install PyQt6
```

### 3. Build C++ Core

Manually run the following commands with the virtual environment activated:

```bash
cmake -S core -B build
cmake --build build
```

We provide an automated compilation script. This script will automatically create the `build` folder, call CMake for configuration and compilation, and finally generate the `backup_core_py` module.

Linux/macOS:

```bash
bash setup.sh
```

## Usage

Ensure you are in the project root and your virtual environment is activated, then run:

```bash
python ./gui/app.py
```

## Testing

To run the C++ unit tests (based on GoogleTest), execute the following commands:

```bash
cd build
ctest --output-on-failure
```

You can also run specific test executables directly from the build directory:

```bash
./test_traverser
./test_packer
# etc.
```

## Project Structure

- `core/`: C++ source code, headers, and CMake configuration.
- `gui/`: Python GUI application.
- `build/`: Build artifacts (created during setup).