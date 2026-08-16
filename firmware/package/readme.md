# Manually Build and Test Each Example

Use the following procedure to put **one example at a time** into the
STM32F429Discovery firmware. Run all build commands from the repository root,
not from this directory. In the devcontainer, the repository root is normally
`/workspace`.

## Prerequisites

- Connect the STM32F429Discovery board and make its ST-Link USB device
  available to the devcontainer.
- Build the saved cross-compilation SDK once. The SDK is reused by every
  example and is not removed by `make distclean`:

  ```bash
  cd /workspace
  make sdk
  ```

  On the host, replace `/workspace` with the path to this repository.

## Serial console

Open the board console at 115200 baud, for example:

```bash
screen /dev/ttyACM0 115200
```

To close `screen`, press `Ctrl-A`, then `\`, and confirm with `y`. To leave
it running in the background instead, press `Ctrl-A`, then `d`. If the serial
device is reported as busy, list or stop old sessions with:

```bash
screen -ls
screen -S SESSION_ID -X quit
```

`sudo fuser -k /dev/ttyACM0` prints nothing when no process currently owns the
device; that is a normal result rather than an error.

## Clean, select, build, and flash one example

Repeat all of these steps for every example that you want to check.

### 1. Remove the previous build

```bash
make distclean
```

This removes `buildroot/output` and the previous Buildroot configuration at
`buildroot/.config`. It preserves the SDK, downloaded source archives, and
compiler cache.

### 2. Load the board configuration

```bash
make configure
```

This creates a fresh Buildroot configuration from
`firmware/configs/stm32f429disco.defconfig`. None of the examples are enabled
by that defconfig, so the next step starts with a clean selection.

### 3. Select one example

```bash
make menuconfig
```

In `menuconfig`, press `/`, enter the package name (for example,
`BR2_PACKAGE_IOEXAMPLE1`), and press Enter. Follow the displayed location,
enable the package with the Space key, then choose **Save** and **Exit**.

Enable only one example symbol for each test. The optional `displaydebug`
diagnostic package may be selected in addition to any one example:

| Package | `menuconfig` symbol | Program on the board | Purpose |
|---|---|---|---|
| `hellomk` | `BR2_PACKAGE_HELLOMK` | `hellomk` | C Makefile/config-file example |
| `hellomkcpp` | `BR2_PACKAGE_HELLOMKCPP` | `hellomkcpp` | C++ Makefile/config-file example |
| `sleepexample` | `BR2_PACKAGE_SLEEPEXAMPLE` | `sleepexample` | Linux sleep and time tests |
| `ioexample1` | `BR2_PACKAGE_IOEXAMPLE1` | `ioexample1` | Interactive PG14 red LED control |
| `ioexample2` | `BR2_PACKAGE_IOEXAMPLE2` | `ioexample2` | PA0 button controlling the PG13 green LED |
| `ioexample3` | `BR2_PACKAGE_IOEXAMPLE3` | `ioexample3` | I2C bus scan on `/dev/i2c-0` |
| `ioexample4` | `BR2_PACKAGE_IOEXAMPLE4` | `ioexample4` | SPI gyroscope/temperature readout |
| `ioexample5` | `BR2_PACKAGE_IOEXAMPLE5` | `ioexample5` | RCC/CPU-frequency readout through `/dev/mem` |
| `ioexample6` | `BR2_PACKAGE_IOEXAMPLE6` | `ioexample6` | Interactive PWM test on TIM3_CH1/PB4 |
| `ioexample7` | `BR2_PACKAGE_IOEXAMPLE7` | `ioexample7` | UART test, defaulting to `/dev/ttySTM1` |
| `ioexample8` | `BR2_PACKAGE_IOEXAMPLE8` | `ioexample8` | RS-485 test, defaulting to `/dev/ttySTM1` |
| `display` | `BR2_PACKAGE_DISPLAY` | `display`, `display-auto` | LCD slideshow with built-in images and optional automounted SD-card images |
| `displaydebug` | `BR2_PACKAGE_DISPLAYDEBUG` | `displaydebug` | Optional reusable kernel and peripheral diagnostics |
| `usbserialdevice` | `BR2_PACKAGE_USBSERIALDEVICE` | `usbserialchat` | USB CDC ACM data port with a loopback test utility |
| `spinand` | `BR2_PACKAGE_SPINAND` | `spinand-ubi` | W25N02KV with a 32 MiB UBI/UBIFS data volume |
| `sdcard` | `BR2_PACKAGE_SDCARD` | `sdcard` | Display-safe SPI4 SD/SDHC card with FAT storage |

Wiring, build, initialization, and persistence-test instructions for the
SPI-NAND package are in [`spinand/README.md`](spinand/README.md).
The display-compatible SD-card wiring and usage are in
[`sdcard/README.md`](sdcard/README.md).

Optional: confirm the selection before building:

```bash
grep -E '^BR2_PACKAGE_(HELLOMK(CPP)?|SLEEPEXAMPLE|IOEXAMPLE[1-8]|DISPLAY(_AUTOSTART|DEBUG)?|USBSERIALDEVICE|SPINAND|SDCARD|PERIPHERY)=y$' buildroot/.config
```

Examples 3 through 7 automatically select `BR2_PACKAGE_PERIPHERY`. It is an
internal build dependency, so you only need to select the example itself.

UART examples use `/dev/ttySTM1`, backed by UART5 on PC12 (TX) and PD2 (RX).
UART5 is enabled in every board DTB and can coexist with `display` and
the W5500 SPI4 interface. The USART1 ST-Link serial console remains available
in every image as `/dev/ttySTM0`.

Selecting `usbserialdevice` makes the USB USER micro-AB connector enumerate on
the host as a CDC ACM adapter (normally `/dev/ttyACM0`) while keeping the Linux
console and shell on USART1. Applications use `/dev/ttyGS0` on the board as an
independent bidirectional data port; `usbserialchat` provides a loopback test.
The connector uses the OTG HS controller's internal full-speed PHY. If the
kernel was already configured before selecting the package, run
`make -C buildroot BR2_EXTERNAL=/workspace/firmware linux-reconfigure` once
before `make build_all`. See
[`usbserialdevice/readme.md`](usbserialdevice/readme.md) for usage.

### 4. Build the flashable firmware

```bash
make build_all
```

Do **not** use `make` or `make all` here. The `all` target runs
`make configure` again and would replace the temporary selection made in
`menuconfig` with the tracked defconfig.

The completed images are written to `buildroot/output/images/`. In particular,
the example is installed into the root filesystem embedded in `xipImage`.

### 5. Flash the board

```bash
make flash
```

The flash target uses OpenOCD to program the bootloader image, device tree, and
`xipImage`, then resets the board. Run this inside the devcontainer without
`sudo`.

### 6. Run the example on the board

Log in through the board's serial console and run the program shown in the
table. For example:

```sh
ioexample1
```

The UART examples optionally accept a device path, and the PWM example
optionally accepts a PWM chip and channel:

```sh
ioexample7 /dev/ttySTM1
ioexample8 /dev/ttySTM1
ioexample6 0 0
```

The default-enabled display autostart option begins the slideshow during boot.
It uses supported images in the root of an automounted `/mnt/sdcard` when the
SD-card package is selected and usable images are present; otherwise it uses
the bundled demonstration images. Check the running source with:

```sh
display-auto status
```

For an interactive display session, run:

```sh
display
```

This stops the background slideshow first so it can take exclusive control of
the framebuffer. Package-specific service controls and diagnostics are
documented in `firmware/package/display/readme.md`.

It displays the bundled 240x320 PNG, JPEG, and GIF test cards once, waiting
three seconds between images. Press `q` to quit, Space or Enter to advance, or
`<`/`>` to move backward/forward. An integer delay and an alternative image
directory are optional:

```sh
display 5
display 2 /path/to/my/images
```

The alternative directory may also contain BMP files. Oversized images from
any source are reduced to fit the framebuffer width and height while preserving
aspect ratio; smaller images are enlarged. BMP decoding is built into `fbv`,
so it does not select another image library. A BMP test card is not bundled
because its uncompressed pixels would consume unnecessary flash.

The script uses BusyBox `/bin/sh` syntax rather than requiring GNU Bash, so
selecting the example does not add a Bash interpreter to the image. Check the
display device and bundled files with:

```sh
ls -l /dev/fb0
ls -l /usr/share/display
```

Selecting only `BR2_PACKAGE_DISPLAY` automatically enables the Linux
DRM/LTDC/ILI9341 framebuffer configuration, builds the display-specific DTB,
and selects `fbv` plus PNG, JPEG, and GIF decoder support. `make flash` reads
`buildroot/.config` and flashes that DTB automatically. Do not configure
Linux or BusyBox manually.

When the display example is not selected, its kernel fragment is not applied,
the display DTB is not built or flashed, and `fbv`, the three decoder
libraries, the shell script, and the test images are absent from the root
filesystem. The normal minimal image therefore pays no display-example size
cost.

The complete clean rebuild, flash, serial-paste, driver-binding, kernel-log,
and retest procedure is documented in
[`display/readme.md`](display/readme.md). Follow that loop until
both `/dev/fb0` and the slideshow have been verified on the board.

For `ioexample6`, connect an external LED to the TIM3 channel 1 output:

```text
PB4 -> 330-680 ohm resistor -> LED anode
GND -> LED cathode
```

Do not omit the resistor. The example uses 1 kHz PWM and accepts any integer
duty cycle from 0 to 100 percent; enter `q` to quit. This lets you compare LED
brightness without an oscilloscope. The onboard PG13 and PG14 LEDs are not
connected to timer-capable alternate functions on this MCU.

The minimal boot script automatically mounts `devtmpfs` on `/dev` before
starting the interactive shell. This creates kernel-managed device nodes such
as `/dev/gpiochip*`, `/dev/i2c-*`, `/dev/spidev*`, `/dev/ttySTM*`, and
`/dev/rtc*`. The following `sysfs` mount provides interfaces such as
`/sys/class/pwm`. Without these mounts, hardware examples fail with
`No such file or directory` even when their kernel drivers are enabled.

To verify the GPIO device nodes on the board, run:

```sh
ls -l /dev/gpiochip*
```

On STM32F429Discovery, the GPIO examples use these mappings:

| Device and line | Board signal |
|---|---|
| `/dev/gpiochip0`, line 0 | PA0 user button |
| `/dev/gpiochip6`, line 13 | PG13 green LED |
| `/dev/gpiochip6`, line 14 | PG14 red LED |

For an older image that does not mount `devtmpfs` automatically, it can be
mounted temporarily without reflashing:

```sh
mount -t devtmpfs devtmpfs /dev
```

After checking the example, return to step 1 and repeat the complete sequence
for the next package.

## Command checklist

After the one-time `make sdk`, the complete per-example sequence is:

```bash
make distclean
make configure
make menuconfig       # Enable and save exactly one example
make build_all
make flash
```

The `menuconfig` selection is intentionally temporary. Do not run
`make savedefconfig` unless you want to change the project's tracked default
configuration.

## Shared periphery static library

Examples 3 through 7 use the single local c-periphery source tree in
`firmware/package/periphery/project`. Buildroot compiles it once as
`libperiphery.a`, installs the archive and headers into the staging sysroot,
and then links each selected example against it.

The static archive itself is not copied into the target root filesystem. The
linker extracts only the object needed by an example (I2C, SPI, MMIO, PWM, or
serial), so this organization removes duplicate source trees without adding
unused periphery modules to the firmware image. Examples 1, 2, and 8 use Linux
interfaces directly, and `sleepexample` uses only libc, so they do not select
the library.

The library probes the Linux UAPI headers supplied by the selected toolchain
at build time. When both GPIO character-device APIs are available, it compiles
both, tries v2 first, and automatically retries with v1 if an older running
kernel rejects the v2 ioctl. Older headers build only v1; very old headers use
the legacy sysfs implementation. SPI and serial also use header feature checks
for optional ioctls and baud rates; the I2C, MMIO, and PWM interfaces used here
are stable.

This allows a library built with current headers to run on most kernels back
to Linux 4.8, where GPIO cdev v1 was introduced. If the toolchain headers are
older than the target kernel and do not define a newer API that you need,
rebuild the SDK as well; source code cannot use declarations absent from its
toolchain headers.
