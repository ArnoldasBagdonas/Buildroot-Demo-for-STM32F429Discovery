# STM32F429Discovery Buildroot Configuration Guide

This guide describes the process for configuring, building, and managing a Buildroot-based Linux system for the **STM32F429Discovery** board. The setup is divided into two stages to optimize build performance and support reproducible toolchain and system builds.

## Directory Structure

> **Note**: The directories `buildroot-ccache/` and `buildroot-downloads/` are created automatically and used to optimize caching and downloads. These are gitignored by default.

```
workspace/
├── buildroot/                 ← Buildroot source (submodule or cloned)
├── buildroot-sdk/             ← SDK output directory (gitignored)
├── buildroot-ccache/          ← ccache directory (gitignored)
├── buildroot-downloads/       ← Downloads directory (gitignored)
├── firmware/
│   ├── board/stm32f429disco/             ← Board-specific configs and custom files
│   │   ├── linux.config                  ← Custom Linux kernel configuration file
│   │   ├── busybox.config                ← Custom BusyBox configuration file
│   │   ├── flash.sh                      ← Flashing script for programming the board
│   │   ├── rootfs-overlay/               ← Files to overlay on the target root filesystem
│   │   ├── dts/                          ← Device Tree Source files for hardware description
│   │   └── linux-patches/                ← Kernel patch files and helper scripts
│   ├── configs/                          ← Buildroot defconfigs
│   │   ├── stm32f429disco.defconfig      ← Main Buildroot configuration for the board
│   │   ├── stm32f429disco_sdk.defconfig  ← Buildroot configuration for SDK/toolchain generation
│   │   └── README.md                     ← This file
│   └── package/                          ← Custom Buildroot packages for additional software
├── scripts/
│   └── stlink-powershell.bat  ← PowerShell script to attach STLink debugger to WSL (run as admin)
├── Makefile                   ← Top-level automation wrapper
└── README.md
```

## Stage 1: SDK Configuration (Optional, Recommended)

Generating the cross-compilation SDK as a standalone artifact is useful when:

- Working with multiple Buildroot configurations or hardware targets
- Reducing full system rebuild times
- Providing toolchains for external development environments

### Steps:

1. Clone and prepare Buildroot:
   ```bash
   make buildroot
   ```

2. Create the default SDK configuration:
   ```bash
   cd buildroot && make stm32f429_disco_xip_defconfig && cd ..
   ```

3. Open the Buildroot configuration menu:
   ```bash
   make menuconfig
   ```
4. In the configuration interface:

   - To use out of tree downloads folder:
     ```bash
     Build options  --->
        (Download dir) → /workspace/buildroot-downloads
     ```

   - To use out of tree ccache folder:
     ```bash
     Build options  --->
        [*] Enable compiler cache
        (Compiler cache location) → /workspace/buildroot-ccache
     ```

   - To enable gcc size optimization:
     ```bash
     Build options  --->
        gcc optimization level → optimize for size
     ```

   - To set GCC compiler Version:
     ```bash
     Toolchain  --->
        (GCC compiler Version) → [*] gcc 13.x
     ```

   - To enable C++ support:
     ```bash
     Toolchain  --->
        [*] Enable C++ support
     ```

5. Save the SDK configuration
   ```bash
   make sdk-savedefconfig
   ```

6. Build the SDK
   ```bash
   make sdk
   ```

   The SDK will be generated as: `buildroot-sdk/arm-buildroot-uclinux-uclibcgnueabi_sdk-buildroot.tar.gz`

## Stage 2: Project Configuration
The second stage configures and builds the full root filesystem and Linux image using the external SDK and tracked configuration files.

### Steps:

1. Clone and prepare Buildroot:
   ```bash
   make buildroot
   ```

2. Create the default Buildroot project configuration:
   ```bash
   cd buildroot && make stm32f429_disco_xip_defconfig && cd ..
   ```

3. Open the Buildroot configuration menu:
   ```bash
   make menuconfig
   ```

4. In the configuration interface:

   - To use an external SDK:
     ```bash
     Toolchain  --->
         Toolchain type → (X) External toolchain
         Toolchain → (X) Custom toolchain
         Toolchain origin → (X) Toolchain to be downloaded and installed
         Toolchain URL → file:///workspace/buildroot-sdk/arm-buildroot-uclinux-uclibcgnueabi_sdk-buildroot.tar.gz
         External toolchain gcc version → (X) 13.x
         External toolchain kernel headers series → (X) 6.1.x
         External toolchain C library → (X) uClibc/uClibc-ng
         [*] Toolchain has threads support?
            [] Toolchain has threads debugging support?
            [] Toolchain has NPTL threads support?
         [*] Toolchain has C++ support?

     ```

   - To enable a tracked Linux kernel configuration:
     ```bash
     Kernel  --->
         (Kernel version) → 6.1.27 
         Kernel configuration → [*] Use a custom kernel configuration file
         (Configuration file path) →  /workspace/firmware/board/stm32f429disco/linux.config
     ```

   - To enable a tracked out-of-tree Device Tree Source file:
     ```bash
     Kernel  --->
         (Kernel version) → 6.1.27 
         (In-tree Device Tree Source file names) → stm32f429-disco (leave as it is)
         (Out-of-tree Device Tree Source file paths) →  /workspace/firmware/board/stm32f429disco/dts/stm32f429disco-custom.dts
     ```

   - To enable a tracked BusyBox configuration:
     ```bash
     Target packages  --->
        BusyBox  --->
            (BusyBox configuration file to use?) → /workspace/firmware/board/stm32f429disco/busybox.config
            (Additional BusyBox configuration fragment files) → (empty)
     ```
   - Remove any existing BusyBox configuration fragments if present in the output tree.
     ```bash
     Target packages  --->
        BusyBox  --->
            (Additional BusyBox configuration fragment files) → (empty)
     ```

5. Exit configuration interface and save the Buildroot configuration
   ```bash
   make savedefconfig
   ```

6. Customize and persist the Linux kernel configuration:
   ```bash
   make linux-menuconfig
   make linux-savedefconfig
   ```

7. Customize and persist the BusyBox configuration:
   ```bash
   make busybox-menuconfig
   make busybox-savedefconfig
   ```

## Tracking Configuration Changes

All configuration files should be added to version control for traceability and reproducibility:

```bash
git add firmware/configs/*.defconfig
git add firmware/board/stm32f429disco/*.config
git commit -m "Save project and SDK configurations"
```

## Flashing Firmware
> **Note**: On Windows + WSL systems, the script `scripts/stlink-powershell.bat` can be used to attach the ST-Link debugger. Run as Administrator and then start devcontainer.

To build and flash the firmware to the STM32F429Discovery board:

```bash
make all
sudo make flash
```

## License

This repository is licensed under the MIT License. See the LICENSE file for details.