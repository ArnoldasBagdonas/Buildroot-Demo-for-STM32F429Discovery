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

## Buildroot Workflow Overview

Buildroot automates the creation of complete embedded Linux systems, including the bootloader, toolchain, kernel, root filesystem, and packages. This is managed through `defconfig` files and a `Makefile`-driven build system.

### Primary Build Steps

- Set up the source and external trees.
- Configure Buildroot via provided defconfigs or interactive menus.
- Build the toolchain, kernel, rootfs, and packages.
- Flash the target board using `flash.sh`.
- Maintain kernel/device tree patches in `linux-patches/`.
- Customize the root filesystem via `rootfs-overlay/`.

### Project Directory Structure
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
│   │   └── README.md                     ← Buildroot Configuration Guide
│   └── package/                          ← Custom Buildroot packages for additional software
├── scripts/
│   └── stlink-powershell.bat  ← PowerShell script to attach STLink debugger to WSL (run as admin)
├── Makefile                   ← Top-level automation wrapper
└── README.md                  ← This file
```

Board and package customizations may be externalized via:

```
BR2_EXTERNAL=firmware
```

### Configuration Targets

| Target                      | What it Does                                                                 |
|-----------------------------|------------------------------------------------------------------------------|
| `make configure`            | Loads main Buildroot config (`configs/stm32f429disco.defconfig`)             |
| `make sdk-configure`        | Loads SDK-specific Buildroot config (`configs/stm32f429disco_sdk.defconfig`) |
| `make menuconfig`           | Opens Buildroot menu interface                                               |
| `make savedefconfig`        | Saves current Buildroot config back to `configs/stm32f429disco.defconfig`    |
| `make sdk-savedefconfig`    | Saves current Buildroot config back to `configs/stm32f429disco_sdk.defconfig`|
| `make linux-menuconfig`     | Opens kernel config interface                                                |
| `make linux-savedefconfig`  | Saves kernel config to `board/stm32f429disco/linux.config`                   |
| `make busybox-menuconfig`   | Opens BusyBox configuration                                                  |
| `make busybox-savedefconfig`| Saves BusyBox config to `board/stm32f429disco/busybox.config`                |

### Build Targets

| Target          | What it Does                                                    |
|-----------------|-----------------------------------------------------------------|
| `make`          | Full system build: toolchain, kernel, rootfs, and packages      |
| `make buildroot`| Downloads/prepares Buildroot in `buildroot/`                    |
| `make sdk`      | Builds the standalone SDK to  `buildroot-sdk/`                  |
| `make toolchain`| Builds only the cross-toolchain                                 |
| `make linux`    | Compiles the Linux kernel                                       |
| `make busybox`  | Compiles BusyBox                                                |
| `make uboot`    | Builds U-Boot bootloader (if enabled)                           |
| `make <pkg>`    | Builds an individual package                                    |

### Simplified Build Timeline

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

### Package Lifecycle & Hooks

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

### Pre-/Post- Build System Hooks

Global hooks can be defined per board:

| Hook File                            | When It Runs                                 |
|--------------------------------------|----------------------------------------------|
| `board/<boardname>/pre-build.sh`     | Runs before the root filesystem is populated |
| `board/<boardname>/post-build.sh`    | Runs after populating `output/target/`       |
| `board/<boardname>/post-image.sh`    | Runs after image creation                    |
| `board/<boardname>/post-genext2fs.sh`| Runs before finalizing `.ext2` image         |

Enable these by declaring in defconfig:

```makefile
BR2_ROOTFS_POST_BUILD_SCRIPT="board/boardname/post-build.sh"
BR2_ROOTFS_POST_IMAGE_SCRIPT="board/boardname/post-image.sh"
```

### Root Filesystem Overlay

Buildroot generates a root filesystem in:

```
output/target/
```

Overlay files placed in:

```
BR2_ROOTFS_OVERLAY = board/boardname/rootfs-overlay
```
are copied directly into the final root filesystem at `output/target/`.

### Output Image Artifacts

Final image files are located under `output/images/`.

| File Type                    | Path               | Description                                |
|------------------------------|--------------------|--------------------------------------------|
| `zImage` / `uImage`          | `output/images/`   | Compressed Linux kernel image              |
| `rootfs.ext2`, `.cpio`, etc. | `output/images/`   | Root filesystem in various formats         |
| `sdcard.img` / `.tar`        | `output/images/`   | Optional prebuilt SD card image or archive |
| `sdk.tar.gz`                 | `output/images/`   | Prebuilt toolchain and sysroot archive     |

### Rebuilding & Cleaning Targets

| Target                | Description                                        |
|-----------------------|----------------------------------------------------|
| `make clean`          | Removes all output files except downloaded sources |
| `make distclean`      | Full clean including configs and downloads         |
| `make clean-rootfs`   | Cleans `target/`                                   |
| `make rebuild-rootfs` | Rebuilds root filesystem                           |
| `make linux-rebuild`  | Rebuilds Linux kernel                              |
| `make busybox-rebuild`| Rebuilds BusyBox                                   |
| `make <pkg>-rebuild`  | Rebuilds a specified package                       |

### Flashing & Deployment

Buildroot does not include built-in flash or deployment targets. Custom targets can be added to the project’s `Makefile` to handle flashing or deploying build artifacts.

Tools such as [stlink](https://github.com/stlink-org/stlink), `dfu-util`, [openocd](https://github.com/openocd-org/openocd), `scp`, or `rsync` can be used to implement flashing or deployment commands.

Example flash command:

```bash
sudo make flash
```

## Getting Started

Clone the repository:
```
git clone https://gitlab.com/bagdoportfolio/buildroot-stm32f429-discovery-demo.git
cd buildroot-stm32f429-discovery-demo
```

Start the devcontainer **after** attaching USB devices as described above.

Build and deploy using:

```
make all
sudo make flash
```

## References

### Buildroot Official Resources

- Project Website
  
  [https://buildroot.org](https://buildroot.org)

- Buildroot Git Repository (official mirror)
  
  [https://github.com/buildroot/buildroot](https://github.com/buildroot/buildroot)

- Buildroot Manual (Latest Release)
  
  [https://buildroot.org/downloads/manual/manual.html](https://buildroot.org/downloads/manual/manual.html)

- Buildroot Configuration System Overview
  
  [https://buildroot.org/downloads/manual/manual.html#configure](https://buildroot.org/downloads/manual/manual.html#configure)

- Buildroot Developer Manual (Package Guidelines, Staging, Overlays)
  
  [https://buildroot.org/downloads/manual/manual.html#_developer_guide](https://buildroot.org/downloads/manual/manual.html#_developer_guide)

### Bootlin Training & Educational Materials

- Bootlin Embedded Linux Training (PDF Slides)
  
  [https://bootlin.com/doc/training/buildroot/buildroot-slides.pdf](https://bootlin.com/doc/training/buildroot/buildroot-slides.pdf)

- Bootlin Buildroot Training Git Repository
  
  [https://github.com/bootlin/training-materials](https://github.com/bootlin/training-materials)

- Bootlin Buildroot Labs (Hands-on exercises)

  [https://bootlin.com/doc/training/buildroot/buildroot-labs.pdf](https://bootlin.com/doc/training/buildroot/buildroot-labs.pdf)

- Bootlin YouTube Channel (Video Courses and Talks)
  
  [https://www.youtube.com/@Bootlin](https://www.youtube.com/@Bootlin)

- Bootlin Training Page

  [https://bootlin.com/training/buildroot/](https://bootlin.com/training/buildroot/)

## License

This repository is licensed under the MIT License. See the LICENSE file for details.