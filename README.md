# FileBackup
A FileBackup Software.

## Features

- **Core**: C++17 based backend for efficient file traversal, compression, encryption, and packaging.
- **Security**: OpenSSL integration for secure backups.
- **Extensibility**: Python bindings via pybind11.
- **GUI**: Modern PyQt6 interface for easy operation.

## Setup

### 1. Setup Python Environment

First, set up the Python environment. This ensures CMake finds the correct Python version and libraries for the bindings.

```bash
python3 -m venv venv

python3 -m venv venv

pip install PyQt6
```

### 2. Build C++ Core

With the Python virtual environment activated, configure and build the C++ core.

```bash
cmake -S core -B build

cmake --build build
```

*Note: This will generate the `backup_core_py` module (e.g., `backup_core_py.cpython-312-darwin.so`) in the `build` directory, which is required by the GUI.*

## Usage

Ensure you are in the project root and your venv is active, then run:

```bash
python ./gui/app.py
```

## Testing

To run the C++ unit tests (GoogleTest):

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