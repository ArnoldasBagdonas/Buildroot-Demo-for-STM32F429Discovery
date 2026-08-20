# Example packages

Build and test one package at a time. The measurements below use the tracked
STM32F429Discovery configuration, the package's default options, and no other
example packages.

All examples use `stm32f429disco-unified.dts`. The DTB describes the complete
supported board wiring; each package enables only the kernel drivers and
BusyBox commands it needs. `sysdiag` is the exception: it enables additional
kernel and BusyBox diagnostics for debugging.

## Isolated image results

| Package | Capabilities | `xipImage` |
|---|---|---:|
| `hello-c` | C Makefile example with configuration parsing, arithmetic, and time output. | 1,169,957 bytes |
| `hello-cpp` | C++ version of the Makefile example, including the target C++ runtime. | 1,374,758 bytes |
| `hwtools` (all applets) | GPIO LED and button, bootloader button handoff, ADC1 input, I2C scan, SPI gyroscope, RCC clocks, PWM LED, UART5, RS-485, timers, and RTC. | 1,327,553 bytes |
| `sysdiag` | System, memory, interrupt, clock, device, kernel-log, debugfs, networking, bus, GPIO, PWM, and RTC diagnostics. | 1,947,911 bytes |
| `gallery` | PNG/JPEG/GIF/BMP framebuffer slideshow, embedded images, gzip initramfs, and boot autostart; optional integrated SD-card support. | 1,599,851 bytes |
| `display` | PNG/JPEG/GIF/BMP framebuffer slideshow, embedded test images, scaling, gzip initramfs, and boot autostart. | 1,600,035 bytes |
| `screen` | Touch console with an on-screen keyboard and child shell, gzip initramfs, and boot autostart. | 1,414,442 bytes |
| `sdcard` | SPI4 SD/SDHC FAT storage with boot automount and periodic insertion/removal detection enabled by default. | 1,388,209 bytes |
| `spinor` | SPI5 W25Q128FV with a 16,376 KiB JFFS2 data volume, two reserved 4 KiB find-me state sectors, and boot automount enabled by default. | 1,162,856 bytes |
| `spinand` | SPI5 W25N02KV with a 32 MiB UBI/UBIFS data volume and boot automount enabled by default. | 1,281,008 bytes |
| `networking` | Kernel and BusyBox networking with the SPI5 W5500 controller enabled by default. | 1,703,993 bytes |
| `find-me` | W5500 IPv4/DHCP, mDNS discovery, onboarding/OAuth service, gzip initramfs, boot autostart, and configurable persistent-state backends. | 1,709,661 bytes |
| `usb-cdc` | USB USER connector as a CDC ACM data port, `/dev/ttyGS0`, and the `usb-cdc` echo example. | 1,333,924 bytes |

`periphery` is a hidden static-library dependency selected by the `i2c-scan`,
`spi-gyro`, `rcc-clock`, `pwm-led`, and `uart-send` hwtools applets. It does not
produce a standalone example image or target command. The linker includes only
the library objects referenced by the selected applets.

The SD, SPI-NAND, and SPI-NOR boot services mount an existing filesystem by
default. NAND and NOR provide an explicit one-time initialization command for
new media; SD cards use an existing FAT16 or FAT32 partition.

## Build one package

Run these commands from the repository root. In the devcontainer it is
`/workspace`.

```bash
make sdk
make distclean
make configure
make menuconfig
make build_all
make flash
```

`make sdk` is needed once. `make distclean` preserves the SDK, downloads, and
compiler cache. In `menuconfig`, open **External options** and select one
package. For `hwtools`, also select the applets to include.

Use `make build_all` after changing `menuconfig`. The default `make` target
reloads the tracked configuration and discards the temporary package
selection.

The generated files are:

```text
buildroot/output/images/stm32f429i-disco.bin
buildroot/output/images/stm32f429disco-unified.dtb
buildroot/output/images/xipImage
```

## Console and package commands

Open the ST-Link serial console at 115200 baud:

```bash
screen /dev/ttyACM0 115200
```

Common target commands are:

| Package | Command | Arguments and behavior |
|---|---|---|
| `hello-c` | `hello-c` | No arguments. Reads `/etc/hello-c.ini` and prints arithmetic, time, and configuration values. |
| `hello-cpp` | `hello-cpp` | No arguments. Reads `/etc/hello-cpp.ini` and prints the C++ version of the example. |
| `hwtools` | `gpio-led`, `button-led`, `boot-button`, `adc-read`, `i2c-scan`, `spi-gyro`, `rcc-clock`, `pwm-led`, `uart-send`, `rs485-chat`, `timers` | `gpio-led`, `button-led`, `boot-button`, `i2c-scan`, `rcc-clock`, and `timers` take no arguments. `boot-button` prints the PA0 state passed by AFBOOT. `adc-read [sample-count [interval-ms]]` reads ADC1_IN13 on PC3/P2 pin 13 and defaults to one sample and a 1000 ms interval. `spi-gyro [delay_ms]` sets the sample interval. `pwm-led [pwmchip-number pwm-channel-number]` defaults to `/sys/class/pwm/pwmchip0`, channel `0` (TIM3_CH1 on PB4). `uart-send [tty-device]` and `rs485-chat [tty-device]` default to `/dev/ttySTM1`. The same applets can be called as `hw APPLET ...`. |
| `sysdiag` | `sysdiag` | No arguments. Prints the complete diagnostic report. |
| `gallery` | `gallery` | `[--autoplay] [delay-seconds] [image-directory]`; defaults to a 3-second delay and embedded images. |
| `display` | `display` | `[--autoplay] [delay-seconds] [image-directory]`; defaults to a 3-second delay and `/usr/share/display`. |
| `screen` | `screen` | No arguments. Runs the touch console in the foreground. |
| `sdcard` | `sdcard status` | Action is `status`, `mount`, `unmount`, or `watch`; omitted action means `status`. |
| `spinor` | `spinor-jffs2 status` | Action is `status`, `mount`, `auto`, `unmount`, or `format --yes`; omitted action means `status`. |
| `spinand` | `spinand-ubi status` | Action is `status`, `mount`, `auto`, `unmount`, or `format --yes`; omitted action means `status`. |
| `networking` | `ifconfig -a` | `-a` lists every interface, including interfaces that are down. Standard BusyBox `ifconfig` address and interface arguments are also available. |
| `find-me` | `find-me-service status` | Service action is `start`, `stop`, `restart`, or `status`. This command links to the boot script `S40find-me`. The daemon also accepts `--interface`, `--port`, `--state`, `--name`, `--model`, GPIO, test-address, and test-device-ID options; run `find-me --help` for the selected backend flags. |
| `usb-cdc` | `usb-cdc` | `[device]`; defaults to `/dev/ttyGS0` and echoes received bytes until interrupted. |

Package-specific wiring and usage are documented in each package directory.
