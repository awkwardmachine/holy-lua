# Building Holy Lua

Holy Lua is written in C and C++ and uses **xmake** as its build system.
DISCLAIMER: Some of these commands might not work or be out of date, so refer to websites documenting these commands for more accurate commands.

---

## Requirements

* **C Compiler:** GCC or Clang supporting C11 (`-std=c11`)
* **C++ Compiler:** G++ or Clang++ supporting C++17 (`-std=c++17`)
* **Build System:** xmake
* **Archiver:** ar (usually provided by binutils or LLVM)

xmake will automatically detect and drive the toolchain once the compilers are installed.

---

## Prerequisites

### Linux

Install your platform's development tools **and xmake**.

**Debian / Ubuntu / Mint:**

```bash
sudo apt update
sudo apt install build-essential
curl -fsSL https://xmake.io/shget.text | bash
```

**Fedora / RHEL / CentOS:**

```bash
sudo dnf groupinstall "Development Tools"
curl -fsSL https://xmake.io/shget.text | bash
```

**Arch / Manjaro:**

```bash
sudo pacman -S base-devel xmake
```

**openSUSE:**

```bash
sudo zypper install -t pattern devel_basis
sudo zypper install xmake
```

**Alpine:**

```bash
sudo apk add build-base xmake
```

**Gentoo:**

```bash
sudo emerge --ask sys-devel/gcc dev-util/xmake
```

**Void Linux:**

```bash
sudo xbps-install base-devel xmake
```

---

### Windows

#### Option 1: MSYS2 (Recommended)

1. Install MSYS2 from [https://www.msys2.org/](https://www.msys2.org/)
2. Open the **MSYS2 MINGW64** terminal and run:

```bash
pacman -Syu
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-xmake
```

This installs GCC (with C11/C++17 support), binutils, and xmake.

#### Option 2: MinGW-w64 + xmake

1. Install MinGW-w64 (GCC 8.0+ recommended)
2. Add MinGW's `bin` directory to your PATH
3. Install xmake:

```powershell
winget install xmake
```

---

### macOS

1. Install Xcode Command Line Tools:

```bash
xcode-select --install
```

2. Install xmake:

```bash
brew install xmake
```

This installs Clang (with C11/C++17 support) and xmake.

---

## Building

1. Extract the Holy Lua source into a new folder
2. Open a terminal in the source root
3. Configure and build using xmake:

```bash
xmake
```

xmake will automatically detect your compiler and build all targets.

---

## Running

After building, run Holy Lua binary directly:

**Linux / macOS:**

```bash
./bin/holylua test.hlua
./test
```

**Windows:**

```cmd
bin\holylua.exe test.hlua
test.exe
```

---

## Verifying Your Toolchain

Check that your compiler and xmake versions meet the requirements:

```bash
gcc --version # GCC 11.4.0+ recommended
g++ --version # GCC 11.4.0+ recommended
xmake --version # xmake 3.0.5+ recommended
```

---

## Build Commands

**Build (default):**

```bash
xmake
```

**Clean build artifacts:**

```bash
xmake clean
```

**Rebuild from scratch:**

```bash
xmake clean && xmake
```

**Verbose build (for debugging):**

```bash
xmake -vD
```

---

## Troubleshooting

### "xmake: command not found"

xmake is not installed or not in your PATH. Reinstall xmake or restart your terminal after installation.

---

### "Permission denied" on Linux/macOS

The binary may not be executable:

```bash
chmod +x bin/holylua
```

---

### "Cannot find -lstdc++"

Install the C++ standard library:

```bash
# Debian / Ubuntu
sudo apt install libstdc++-dev

# Fedora
sudo dnf install libstdc++-devel
```

---

### Build fails on Windows

* Ensure you are using the **MINGW64** environment (MSYS2)
* Verify that `xmake --version` works
* Avoid mixing MSVC and MinGW toolchains
* Make sure you are in the root folder for the project

xmake will automatically select the correct compiler when launched from the proper environment.
