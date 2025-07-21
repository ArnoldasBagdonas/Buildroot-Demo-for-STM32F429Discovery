# Buildroot Demo for STM32F429Discovery

This project provides a structured and automated environment for building a fully customized Linux system using [Buildroot 2024.02](https://buildroot.org) (Linux 6.1+) for the [STM32F429Discovery](https://www.st.com/en/evaluation-tools/32f429idiscovery.html) board.

## USB Setup Workflow for WSL2 + Devcontainer

> **Important**: The USB device must be connected and attached to WSL **before launching the devcontainer or Docker image**.

WSL (Windows Subsystem for Linux) enables running a Linux environment on a Windows host. However, [connecting USB devices](https://learn.microsoft.com/en-us/windows/wsl/connect-usb) to WSL requires specific setup. The following steps describe how a USB device, such as the STM32F4 Discovery board, can be made accessible within WSL and the devcontainer environment.

Before starting the development container, mount the ST-LINK device using the provided PowerShell script (run as Administrator):

```bash
scripts/stlink-powershell.bat
```

This script attaches the ST-LINK USB interface to WSL, enabling USB serial and debug access inside the devcontainer.

## Hardware Serial Console Connection

> ⚠️ Voltage levels should match (most adapters support 3.3V I/O).

Interacting with the bootloader and uCLinux running on the STM32F429Discovery requires a serial console. Serial console settings:

- **Baud Rate**: 115200
- **Data Bits**: 8
- **Parity**: None
- **Stop Bits**: 1
- **Flow Control**: None

UART connection pinout:

| STM32F429 Pin | Function    | ext USB UART | Notes         |
|---------------|-------------|--------------|---------------|
| PA9           | USART1_TX   | (RXD)        | TX from board |
| PA10          | USART1_RX   | (TXD)        | RX to board   |
| GND           | Ground      | (GND)        | Common GND    |

## Buildroot Workflow: Step-by-Step

Buildroot generates a complete Linux system including kernel, root filesystem, toolchain, bootloader, and packages. This is controlled through a configuration file (defconfig) and a Makefile-based system.

### Typical Buildroot Workflow

The standard Buildroot workflow involves the following steps:

- Setting up the Buildroot source directory (`buildroot/`) and external tree (`firmware/`).
- Configuring Buildroot using provided defconfigs or `make menuconfig` (see Configuration Targets).
- Building the SDK, root filesystem, kernel, and packages (see Build Targets).
- Flashing the board using the provided `flash.sh` script (see Flash & Deployment).
- Maintaining kernel and device tree patches in `linux-patches/`.
- Customizing the root filesystem via `rootfs-overlay/`.
- Adding custom packages under `package/`.

For detailed instructions and explanations, see the sections below.

### 1. Project Setup

The project directory structure should be as follows:

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
│   │   ├── stm32f429disco_defconfig      ← Main Buildroot configuration for the board
│   │   └── stm32f429disco_sdk_defconfig  ← Buildroot configuration for SDK/toolchain generation
│   └── package/                          ← Custom Buildroot packages for additional software
├── scripts/
│   └── stlink-powershell.bat  ← PowerShell script to attach STLink debugger to WSL (run as admin)
├── Makefile                   ← Top-level automation wrapper
└── README.md                  ← This file
```

Board and package customizations can be externalized by setting the BR2_EXTERNAL environment variable, for example:

```
BR2_EXTERNAL=myproject
```

### 2. Configuration Targets

These targets specify what to build and how:

| Target                      | What it Does                                                                 |
|-----------------------------|------------------------------------------------------------------------------|
| `make configure`            | Applies the main Buildroot config (`configs/stm32f429disco_defconfig`)       |
| `make configure_sdk`        | Applies the SDK Buildroot config (`configs/stm32f429disco_sdk_defconfig`)    |
| `make menuconfig`           | Opens Buildroot’s TUI menu to configure the global build system              |
| `make savedefconfig`        | Saves the current Buildroot config to `configs/stm32f429disco_defconfig`     |
| `make linux-defconfig`      | Applies the Linux kernel config (`board/stm32f429disco/linux.config`)        |
| `make linux-menuconfig`     | Opens the TUI menu to configure the Linux kernel                             |
| `make linux-savedefconfig`  | Saves current Linux kernel config to `board/stm32f429disco/linux.config`     |
| `make busybox-defconfig`    | Applies the BusyBox config (`board/stm32f429disco/busybox.config`)           |
| `make busybox-menuconfig`   | Opens the TUI menu to configure BusyBox                                      |
| `make busybox-savedefconfig`| Saves current BusyBox config to `board/stm32f429disco/busybox.config`        |


### 3. Build Targets

After configuration, these targets start the build process:

| Target          | What it Does                                                                             |
|-----------------|------------------------------------------------------------------------------------------|
| `make`          | Performs a full build: toolchain, kernel, root filesystem, and all enabled packages      |
| `make buildroot`| Downloads and prepares Buildroot (cloned or copied into the `buildroot/` directory)      |
| `make sdk`      | Builds only the SDK/toolchain and installs it to `buildroot-sdk/`                        |
| `make toolchain`| Builds only the cross-compilation toolchain                                              |
| `make linux`    | Builds the Linux kernel                                                                  |
| `make busybox`  | Builds BusyBox (userland utilities)                                                      |
| `make uboot`    | Builds U-Boot bootloader (if enabled in configuration)                                   |
| `make <pkg>`    | Builds a specific Buildroot package (e.g., `make my_package`)                            |

### 4. Example Build Timeline (Simplified)

Running `make` triggers these steps in Buildroot:

```
make
├── Apply defconfig
├── Build host tools
├── Build toolchain
├── Fetch and extract source packages
├── Apply patches
├── Configure packages
├── Build packages (BusyBox, kernel, etc.)
├── Run pre-build script (board/<boardname>/pre-build.sh) (optional)
├── Populate target/ filesystem
├── Run post-build script (board/<boardname>/post-build.sh) (optional)
├── Generate images (cpio, ext2, etc.)
├── Run post-image script (board/<boardname>/post-image.sh) (optional)
└── Run post-genext2fs script (board/<boardname>/post-genext2fs.sh) (optional)
```

### 5. Package Lifecycle & Hooks

Buildroot packages define lifecycle hooks using define <PKG>_<STAGE>_CMDS in .mk files. These control how packages are built and integrated.

| Hook (Make Variable)                 | When It Runs                         | Example Use                                              |
|--------------------------------------|--------------------------------------|----------------------------------------------------------|
| `<PKG>_DOWNLOAD_CMDS`                | Before source is downloaded          | Override source fetching (e.g., from a custom mirror)    |
| `<PKG>_EXTRACT_CMDS`                 | After download, before extraction    | Custom extract logic (e.g., unzip, custom script)        |
| `<PKG>_PATCH_CMDS`                   | After extract, before build          | Apply patch files manually                               |
| `<PKG>_CONFIGURE_CMDS`               | Before compilation begins            | Run `./configure` or CMake manually                      |
| `<PKG>_BUILD_CMDS`                   | Build step (compilation)             | Invoke `make`, `cmake`, or other compile logic           |
| `<PKG>_INSTALL_CMDS`                 | Install to both host and target      | Place files into Buildroot’s staging dirs                |
| `<PKG>_INSTALL_TARGET_CMDS`          | Install only to root filesystem      | Copy runtime binaries to `$(TARGET_DIR)/usr/bin` etc.    |
| `<PKG>_INSTALL_STAGING_CMDS`         | Install headers/libraries to host    | For SDK/dev usage — install to `$(STAGING_DIR)`          |
| `<PKG>_POST_INSTALL_TARGET_HOOKS`    | After target install                 | Cleanup, strip binaries, register steps                  |
| `<PKG>_POST_INSTALL_STAGING_HOOKS`   | After staging install                | Staging tweaks, symlinks, special headers                |

See [Buildroot manual on package development](https://buildroot.org/downloads/manual/manual.html#generic-package-reference).

### 6. Pre-/Post- Build System Hooks

Global build stages can be hooked by adding scripts in the board directory:

| Hook File                            | When It Runs                           |
|--------------------------------------|----------------------------------------|
| `board/<boardname>/post-build.sh`    | After root filesystem is populated     |
| `board/<boardname>/post-image.sh`    | After images are generated             |
| `board/<boardname>/post-genext2fs.sh`| Before `.ext2` filesystem is finalized |

Enable these hooks in the Buildroot defconfig by adding:

```makefile
BR2_ROOTFS_POST_BUILD_SCRIPT="board/boardname/post-build.sh"
BR2_ROOTFS_POST_IMAGE_SCRIPT="board/boardname/post-image.sh"
```

### 7. Root Filesystem & Overlay

Buildroot generates a root filesystem in:

```
output/target/
```

Static files such as configs, scripts, or services can be overlaid from:

```
BR2_ROOTFS_OVERLAY = board/boardname/rootfs-overlay
```

All contents of this directory are copied as-is into `output/target/`.

### 8. Image Generation

After building, final system images are placed in the `output/images/` directory.

| File Type                     | Path               | Description                                             |
|------------------------------|--------------------|---------------------------------------------------------|
| `zImage` / `uImage`          | `output/images/`   | Compressed Linux kernel image (ARM targets)             |
| `rootfs.ext2`, `.cpio`, etc. | `output/images/`   | Root filesystem images in various formats               |
| `sdcard.img` / `.tar`        | `output/images/`   | Optional SD card image or tarball for deployment        |
| `sdk.tar.gz`                 | `output/images/`   | SDK tarball containing toolchain and sysroot            |

### 9. Rebuilding & Cleaning Targets

These targets are available to clean or force rebuild specific components:

| Target                | Description                                                     |
|-----------------------|-----------------------------------------------------------------|
| `make clean`          | Cleans all build output, but keeps downloads                    |
| `make distclean`      | Full clean: removes `output/`, downloads, `.config`, etc.       |
| `make clean-rootfs`   | Cleans `target/`                                                |
| `make rebuild-rootfs` | Forces a full rebuild of the root filesystem (cleans `target/`) |
| `make linux-rebuild`  | Forces a full rebuild of the Linux kernel                       |
| `make busybox-rebuild`| Forces a rebuild of BusyBox                                     |
| `make <pkg>-rebuild`  | Rebuilds any specific package (e.g., `make mypkg-rebuild`)      |

### 10. Flash & Deployment (Custom Targets)

Buildroot does not include built-in flash or deployment targets. Custom targets can be added to the project’s `Makefile` to handle flashing or deploying build artifacts.

Tools such as [stlink](https://github.com/stlink-org/stlink), `dfu-util`, [openocd](https://github.com/openocd-org/openocd), `scp`, or `rsync` can be used to implement flashing or deployment commands.

Example usage:

```bash
sudo make flash
```

## Getting Started

- Clone the repository:
  ```
  git clone https://gitlab.com/bagdoportfolio/buildroot-stm32f429-discovery-demo.git
  cd buildroot-stm32f429-discovery-demo
  ```

- Follow the USB Setup Workflow above, then start the devcontainer.

- Build the project as described.
  ```
  make all
  ```

- Flash the STM32F429Discovery board.

## License

This repository is licensed under the MIT License. See the LICENSE file for details.