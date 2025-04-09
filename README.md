# Altera FPGA Remote Debug using Etherlink Reference Code

## Introduction

Etherlink serves as a bridge between the FPGA device and Quartus software, facilitating communication and control. It supports both the JTAG-over-protocol and HS ST Debug Interface IP components by performing read and write operations on their CSR interfaces. Concurrently, Etherlink exchanges ST packets with either the JTAG-over-protocol blaster driver in the JTAG server or the altera/remote-debug driver in the Streaming Debug Server over TCP/IP.

The Etherlink reference code relies on the IP Access API for Intel/Altera FPGAs. This API provides fundamental software access methods to interact with IP components in FPGAs using memory-mapped interfaces. Various implementations of this API are available for different operating environments and can be found in the GitHub repository: https://github.com/altera-fpga/remote-debug-for-intel-fpga.

The Etherlink reference code illustrates how to process the TCP/IP protocol as specified in the Intel documentation: https://www.intel.com/content/www/us/en/docs/programmable/728673/21-3/etherlink-application-tcp-ip-protocol.html. It demonstrates how streaming packets, defined by this protocol, can be exchanged with the corresponding IP component over the CSR interface using memory-mapped read or write operations.

## Build

### Basic Build Method
CMake is used to build the source code.

Basic build instruction to create an executable as build/etherlink

```bash
cmake . -Bbuild && cd build
make
```

Install the executable into /usr/local/bin. 
Note: This requires root permission.

```bash
make install
```

### Advanced Build Methods

#### Cross-compile or Alternative Compiler Location

Standard CMake variables CMAKE_C_COMPILER and CMAKE_CXX_COMPILER can be used to specify a different compiler from what the CMake program discovers.

For example, the commands to use the ARM cross-compile for HPS:
```bash
cmake  . -Bbuild -DCMAKE_C_COMPILER:FILEPATH=/rocketboard/s10_gsrd/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc -DCMAKE_CXX_COMPILER:FILEPATH=/rocketboard/s10_gsrd/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-g++
cmake --build build --config Release --target all --
```

Note: The above example also shows how CMake can be used to carry out the make without changing the directory.

#### IP Access API for Intel/Altera FPGAs Repo Fetching Options

This repository relies on the IP Access API, utilizing the reference implementation available at the GitHub repository [altera-fpga/fpga-ip-access](https://github.com/altera-fpga/fpga-ip-access).

Two CMake variables, IP_ACCESS_API_LIB_GIT_URL and IP_ACCESS_API_LIB_GIT_TAG, are defined to set up a git clone to obtain the code. IP_ACCESS_API_LIB_GIT_URL can be a local directory where you cloned the repo. IP_ACCESS_API_LIB_GIT_TAG can be the git branch name.

If you cloned the repo to ../fpga-ip-access, you can set IP_ACCESS_API_LIB_GIT_URL=../../../fpga-ip-access. The relative path depends on how cmake FetchContent works. It is more reliable to use an absolute path.

```bash
cmake . -Bbuild -DIP_ACCESS_API_LIB_GIT_URL:STRING=../../../fpga-ip-access 
cmake --build build --config Release --target all --
```

#### IP Access API for Intel/Altera FPGAs Platform Choice

This CMake variable, BUILD_FPGA_IP_ACCESS_MODE, is used to specify a library to use. For the available platforms, please read the README.md in that repo. By default, the library using the Linux UIO driver is selected. On HPS, if you want to use the /dev/mem device, which is enabled in GSRD currently, you can select it by specifying -DBUILD_FPGA_IP_ACCESS_MODE:STRING=DEVMEM.

```bash
cmake . -Bbuild -DBUILD_FPGA_IP_ACCESS_MODE:STRING=DEVMEM 
cmake --build build --config Release --target all --
```

#### Old CMake Version without FetchContent

Clone the IP Access API for Intel/Altera FPGAs repo and copy the fpga_ip_access_lib folder under this project's workspace. Change the cmake_minimum_required value, remove the block of code related to FetchContent, and add the CMake files of fpga_ip_access_lib using the following directive:

```cmake
add_subdirectory(${CMAKE_SOURCE_DIR}/fpga_ip_access_lib)
```

## VSCode Setup

Visual Studio Code (VSCode) https://code.visualstudio.com/ is a popular IDE used to develop the source code. clangd https://clangd.llvm.org/ using the clangd VSCode extension https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd is used to browse the symbols and format the code most importantly. The coding style is specified in .clang-format. Please enable this CMake variable CMAKE_EXPORT_COMPILE_COMMANDS to allow clangd to work in VSCode.

Example .vscode/setting.json file used:

```json
{
    "cmake.configureSettings": {
        "CMAKE_EXPORT_COMPILE_COMMANDS": "1",
        "BUILD_FPGA_IP_ACCESS_MODE": "DEVMEM",
        "IP_ACCESS_API_LIB_GIT_URL": "../../../fpga-access-api",
        "IP_ACCESS_API_LIB_GIT_TAG": "main"
    }
}
```

## Etherlink Command Arguments

There are two groups of command arguments. One group of arguments is processed by the etherlink reference code; another is processed by fpga_platform_init() in the IP Access API for Intel/Altera FPGAs. The arguments in the latter group are specific to the API platform implementation. Please consult the README.md of the specific API platform implementation to understand what arguments are available and how to specify them. Etherlink --help only shows the arguments for the UIO implementation.
