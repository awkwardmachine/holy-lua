# Building Holy Lua

Holy Lua is written in C and C++ and uses Make as its build system.

## Requirements

- **C Compiler:** GCC or Clang supporting C11 (`-std=c11`)
- **C++ Compiler:** G++ or Clang++ supporting C++17 (`-std=c++17`)
- **Build System:** Make (GNU Make recommended)
- **Archiver:** ar (for creating static libraries)

## Prerequisites

### Linux

**Debian/Ubuntu/Mint:**
```bash
sudo apt update
sudo apt install build-essential
```
This installs GCC 9+ (with C11/C++17 support), Make, and ar.

**Fedora/RHEL/CentOS:**
```bash
sudo dnf groupinstall "Development Tools"
```
This installs GCC 8+ (with C11/C++17 support), Make, and binutils.

**Arch/Manjaro:**
```bash
sudo pacman -S base-devel
```
This installs GCC 10+ (with C11/C++17 support), Make, and binutils.

**openSUSE:**
```bash
sudo zypper install -t pattern devel_basis
```
This installs GCC 7+ (with C11/C++17 support), Make, and binutils.

**Alpine:**
```bash
sudo apk add build-base
```
This installs GCC 10+ (with C11/C++17 support), Make, and binutils.

**Gentoo:**
```bash
sudo emerge --ask sys-devel/gcc sys-devel/make
```
This installs GCC 11+ (with C11/C++17 support) and Make.

**Void Linux:**
```bash
sudo xbps-install base-devel
```
This installs GCC 9+ (with C11/C++17 support), Make, and binutils.

### Windows

**Option 1: MinGW-w64 (Recommended)**

1. Download MinGW-w64 from [mingw-w64.org](https://www.mingw-w64.org/downloads/)
2. Install with these settings:
   - **Version:** 8.1.0 or newer (for C++17 support)
   - **Architecture:** x86_64
   - **Threads:** posix
   - **Exception:** seh
3. Add MinGW's `bin` directory to your PATH

MinGW-w64 includes GCC 8+ (with C11/C++17 support), mingw32-make, and ar.

**Option 2: MSYS2**

1. Install MSYS2 from [msys2.org](https://www.msys2.org/)
2. Open MSYS2 MINGW64 terminal and run:
```bash
pacman -Syu
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make
```

This installs GCC 10+ (with C11/C++17 support), mingw32-make, and binutils.

### macOS

Install Xcode Command Line Tools:
```bash
xcode-select --install
```

This installs Clang 12+ (with C11/C++17 support), Make, and ar.

## Building

1. Extract the Holy Lua source to a new folder
2. Open a terminal in that folder
3. Run the build command:

**Linux/macOS:**
```bash
make
```

**Windows (MinGW):**
```bash
mingw32-make
```

## Verifying Your Installation

Check that your compiler versions meet the requirements:

**Linux/macOS:**
```bash
gcc --version # Should be 7.0 or newer
g++ --version # Should be 7.0 or newer
make --version # Should be GNU Make 3.81 or newer
```

**Windows (MinGW):**
```bash
gcc --version # Should be 8.0 or newer
g++ --version # Should be 8.0 or newer
mingw32-make --version # Should be GNU Make 3.81 or newer
```

## Using the Compiler

After building, you can compile Holy Lua programs from within the source root folder.

**Basic usage:**

Linux/macOS:
```bash
./bin/holylua test.hlua
gcc output.c -o output -Iinclude -Llib -lholylua -lm
./output
```

Windows:
```bash
bin\holylua.exe test.hlua
gcc output.c -o output.exe -Iinclude -Llib -lholylua -lm
output.exe
```

**Custom output name:**

Linux/macOS:
```bash
./bin/holylua test.hlua -o my_program
gcc my_program.c -o my_program -Iinclude -Llib -lholylua -lm
./my_program
```

Windows:
```bash
bin\holylua.exe test.hlua -o my_program
gcc my_program.c -o my_program.exe -Iinclude -Llib -lholylua -lm
my_program.exe
```

The compiler generates a C file from your Holy Lua code, then you compile it with GCC linking against the Holy Lua runtime library.

## Build Targets

The Makefile provides several targets:

**Build everything (default):**
```bash
make # Linux/macOS
mingw32-make # Windows
```

**Clean build artifacts:**
```bash
make clean # Linux/macOS
mingw32-make clean # Windows
```

**Rebuild from scratch:**
```bash
make clean && make # Linux/macOS
mingw32-make clean && mingw32-make  # Windows
```

## Troubleshooting

**"make: command not found" or "mingw32-make: command not found"**

Install the build tools for your platform (see Prerequisites above). On Windows, ensure MinGW's `bin` directory is in your PATH.

**"g++: error: unrecognized command line option '-std=c++17'"**

Your GCC version is too old. You need GCC 7.0 or newer (8.0+ on Windows). Update your compiler:
```bash
# Debian/Ubuntu
sudo apt install gcc-9 g++-9
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 90
sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-9 90
```

**"Permission denied" on Linux/macOS**

The binary might not be executable. Run:
```bash
chmod +x bin/holylua
```

**"Cannot find -lstdc++" on Linux**

Install the C++ standard library:
```bash
# Debian/Ubuntu
sudo apt install libstdc++-dev

# Fedora
sudo dnf install libstdc++-devel
```

**"ar: command not found"**

Install binutils:
```bash
# Debian/Ubuntu
sudo apt install binutils

# Fedora
sudo dnf install binutils
```

**Build fails on Windows with "mkdir: cannot create directory"**

Make sure you're using `mingw32-make` and not `make`. The Makefile detects the OS and uses appropriate commands.