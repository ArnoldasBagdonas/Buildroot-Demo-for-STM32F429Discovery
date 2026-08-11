# Display Example

This package enables the STM32F429Discovery LCD only when
`BR2_PACKAGE_DISPLAYEXAMPLE` is selected. It adds the display-specific Linux
configuration and device tree, `fbv`, the slideshow script, and compact PNG,
JPEG, and GIF test cards. `fbv` also accepts BMP files without another decoder
library.

The core display fragment retains procfs because Linux 6.1 framebuffer
registration needs `/proc/fb` in order to create `/dev/fb0`. Extra diagnostic
features are provided by the independent `BR2_PACKAGE_DISPLAYDEBUG` package.
It can be selected beside this or any other example and disabled again for a
smaller normal image.

## Required configuration and why

The display implementation uses conditional kernel configuration, a
board-specific device tree, and target compiler flags.

Selecting `BR2_PACKAGE_DISPLAYEXAMPLE` in [Config.in](Config.in) selects `fbv`
and its PNG, JPEG, and GIF decoders. The selection also activates the
display-only integration in [displayexample.mk](displayexample.mk) and
[external.mk](../../external.mk). When the example is disabled, the decoder
libraries, viewer, images, display kernel features, and display DTB are not
included in the firmware.

The conditional
[linux-display.config](../../board/stm32f429disco/linux-display.config) fragment
contains only the kernel facilities required by this example:

| Kernel option | Purpose |
|---|---|
| `CONFIG_FB` | Provides the Linux framebuffer API used by `fbv`. |
| `CONFIG_PROC_FS` | Allows Linux 6.1 `fbmem_init()` to register `/proc/fb` and continue creating the graphics class and `/dev/fb0`. |
| `CONFIG_PM` | Enables the STM32 DRM runtime-PM callbacks that restore the `lcd-tft` clock at the first modeset. Without it, LTDC registers remain inactive and no pixels are scanned out. |
| `CONFIG_DRM` | Enables the kernel Direct Rendering Manager framework. |
| `CONFIG_DRM_FBDEV_EMULATION` | Exposes the DRM framebuffer as `/dev/fb0` for the legacy framebuffer viewer. |
| `CONFIG_DRM_STM` | Enables the STM32 LTDC DRM driver. |
| `CONFIG_BACKLIGHT_CLASS_DEVICE` | Satisfies the ILI9341 panel driver's Kconfig dependency. |
| `CONFIG_DRM_PANEL_ILITEK_ILI9341` | Enables the panel driver that initializes the LCD controller over SPI and supplies the display mode. |

The conditional
[stm32f429disco-display.dts](../../board/stm32f429disco/dts/stm32f429disco-display.dts)
describes the board wiring:

- LTDC is enabled with `ltdc_pins_b` and connected to the panel through graph
  endpoints.
- SPI5 chip select 0 on PC1 remains assigned to the gyroscope. Chip select 1
  on PC2 controls the LCD, and PD13 is its data/command signal.
- The panel uses its board-specific `st,sf-tc240t-9370-t` compatibility and a
  maximum 10 MHz three-wire SPI control interface.
- The UART examples use UART5 on PC12/PD2 and RS-485 DE on PD4. These free
  expansion-header pins do not overlap LTDC, so serial and display examples
  can be selected together.

No LCD clock is assigned in the display DTB. The stock ILI9341 panel mode and
STM32 clock framework select the pixel clock during modeset. `CONFIG_PM` lets
the STM32 DRM runtime-PM callbacks enable that clock when scanout starts.

giflib's upstream Makefile normally replaces Buildroot's target `CFLAGS` with
`-O2 -fPIC`. The conditional `GIFLIB_BUILD_CMDS` in
[external.mk](../../external.mk) instead passes Buildroot's no-MMU,
static/FLAT-binary target flags on make's command line. This fixes GIF decoding
while keeping giflib compatible with the target ABI. fbv uses its standard
framebuffer `mmap()` implementation.

The `displayexample` script activates the first advertised framebuffer mode
before starting `fbv`. This first modeset initializes the panel, enables LTDC
scanout, and turns on the runtime-managed LCD clock. It then presents the
bundled PNG, JPEG, and GIF files as an interactive slideshow. BMP decoding is
available from fbv without another image library.

The optional `BR2_PACKAGE_DISPLAYDEBUG` selection is intentionally independent.
Its debugfs, logging, diagnostic applets, and `displaydebug` command are not
part of the normal display image.

## Build and run

Run these commands at the repository root:

```bash
make distclean
make configure
make menuconfig
```

Search for `BR2_PACKAGE_DISPLAYEXAMPLE`, enable it, save, and exit. Then run:

```bash
make build_all
make flash
```

Confirm that the flash output names the display DTB:

```text
Flashing DTB: buildroot/output/images/stm32f429disco-display.dtb
```

The LCD stays blank at the serial shell until the viewer is started. On the
board, run:

```sh
displayexample
```

Use `q` to quit, Space or Enter for the next image, and `<` or `>` to move
backward or forward. An integer delay in seconds and another image directory
are optional:

```sh
displayexample 5
displayexample 2 /path/to/images
```

The alternative directory may contain `.png`, `.jpg`, `.jpeg`, `.gif`, and
`.bmp` files.

## Known-good ST hardware demonstration

Before changing Linux drivers, the LCD and touch panel were verified with
ST's compiled STM32F429I-Discovery demonstration:

<https://www.st.com/resource/en/compiled_demos/32f429idiscovery_demo.zip>

Download and extract the archive on the host. The tested image is
`STM32CubeDemo_STM32F429I-Discovery_1.5.0.hex`. Flashing it replaces Linux, so
keep this as a hardware sanity test and reflash the Buildroot image afterward.

## Serial ports

The console uses USART1 on PA9/PA10. Those pins can use an external Cypress
USB-serial adapter; a DISC1 board with ST-LINK/V2-B may instead expose USART1
through its ST-LINK virtual COM port.

`ioexample7` and `ioexample8` use the independent `/dev/ttySTM1` port: UART5
TX on PC12 (P2 pin 44), RX on PD2 (P2 pin 40), and, for RS-485, DE on PD4
(P2 pin 38). None of those signals overlaps the LTDC pinctrl state, so the
UART examples can be used while the display is active.

## Debugging with the optional displaydebug package

Select `BR2_PACKAGE_DISPLAYDEBUG` in addition to this example before building.
It conditionally enables debugfs, timestamped kernel logging, a 16 KiB kernel
log buffer, and reusable BusyBox diagnostic applets. These features and the
`displaydebug` command are absent from images where the package is disabled.

Long pasted commands may lose characters on the serial connection. In GNU
Screen, press `Ctrl-A`, then `:`, enter `slowpaste 5`, and submit it before
pasting a command block. Alternatively, enter each diagnostic command
separately and wait for the shell prompt.

First check the framebuffer and run the viewer:

```sh
ls -l /dev/fb0
ls -l /sys/class/drm /sys/class/graphics 2>&1
displayexample
```

Collect a single report of system information, interrupts, the clock tree,
device nodes, kernel messages, and (when LTDC is present) display registers:

```sh
displaydebug
```

Expected results are `/dev/fb0`, at least one DRM `card*` entry, and
`/sys/class/graphics/fb0`.

Verify that the display-specific DTB was booted:

```sh
cat /sys/firmware/devicetree/base/soc/*40016800*/status
cat /sys/firmware/devicetree/base/soc/*40005000*/status
find /sys/firmware/devicetree/base/soc -name 'display@1'
```

The expected LTDC and UART5 statuses are both `okay`, and `find` must print the
SPI `display@1` node.

Check driver binding:

```sh
ls -l /sys/bus/spi/devices
ls -l /sys/bus/spi/devices/spi0.1/driver 2>&1
ls -l /sys/bus/spi/drivers/panel-ilitek-ili9341 2>&1
ls -l /sys/bus/platform/devices/40016800.display-controller/driver 2>&1
ls -l /sys/bus/platform/drivers/stm32-display 2>&1
```

Interpret the results as follows:

- A missing `display@1` node means the minimal DTB was flashed instead of the
  display DTB.
- A present `spi0.1` without a `driver` link means the ILI9341 panel driver did
  not bind.
- `spi0.1` bound to `panel-ilitek-ili9341`, but no driver link on
  `40016800.display-controller`, means LTDC failed to probe. Check the kernel
  log for clock, pinctrl, or memory-allocation errors.
- Both drivers bound but no `/dev/fb0` indicates a DRM framebuffer-emulation or
  memory-allocation failure.
- `Unable to create device for framebuffer 0; errno = -19` followed by
  `fb0: stmdrmfb frame buffer device` means Linux allocated the framebuffer but
  `fbmem_init()` did not create the graphics class. On Linux 6.1 this happens
  when `CONFIG_PROC_FS` is disabled: creation of `/proc/fb` fails first. The
  display-only kernel fragment therefore enables procfs; the minimal base
  kernel still excludes it.

In that case, collect the relevant kernel messages:

```sh
dmesg | grep -Ei 'drm|ltdc|ili9341|framebuffer|fb0|pinctrl|memory'
```

If the filtered output is empty, save the complete log instead:

```sh
dmesg
```

When reporting a failure, include all of the following in one message:

```sh
ls -l /dev/fb* /dev/dri 2>&1
ls -l /sys/class/drm /sys/class/graphics 2>&1
ls -l /sys/bus/spi/devices/spi0.1/driver 2>&1
ls -l /sys/bus/platform/devices/*40016800*/driver 2>&1
dmesg | grep -Ei 'drm|ltdc|ili9341|framebuffer|fb0|pinctrl|memory'
```

The slideshow script avoids combined `set -eu`, which this project's minimal
BusyBox `hush` rejects.

## Rebuild and retest loop

Use a clean rebuild after changing the display DTS, Linux or BusyBox fragments,
package selection, decoder configuration, slideshow script, or bundled images.
This avoids testing a new DTB with an old embedded root filesystem or kernel.
From either the repository root on the host or `/workspace` in the
devcontainer, run:

```bash
make distclean
make configure
make menuconfig
make build_all
make flash
```

In `make menuconfig`, select `displayexample` and the independent
`displaydebug` package while diagnosing the LCD. `ioexample7` and `ioexample8`
may also be selected because they use conflict-free UART5 pins. Disable
`displaydebug` for the final image-size check.

Check these milestones during every iteration:

1. `make build_all` must finish without an error.
2. `make flash` must print
   `Flashing DTB: .../stm32f429disco-display.dtb`.
3. The new BusyBox build timestamp must appear after reset.
4. Both panel and LTDC `driver` links must exist.
5. `/sys/class/drm/card0` and `/dev/fb0` must exist.
6. Run `displaydebug`; after display activation, verify that `lcd-tft` is
   enabled, LTDC registers are nonzero, and LTDC interrupt counts rise.
7. Run `displayexample`; verify that the bundled PNG, JPEG, and GIF cards are
   visible, navigation keys work, and `q` exits.

If any milestone fails, stop there, collect the diagnostic block above, fix
that layer, and repeat the complete clean sequence. `make flash` alone never
rebuilds changed sources.

During a quick developer iteration, a changed kernel fragment can be forced
through Kconfig without rebuilding host tools:

```bash
make -C buildroot BR2_EXTERNAL="$PWD/firmware" linux-reconfigure
make build_all
```

Always verify the resulting option before flashing. For example:

```bash
grep '^CONFIG_PROC_FS=y' buildroot/output/build/linux-*/.config
```

Use the complete `distclean` sequence for final/manual verification; a plain
incremental `make build_all` may only relink the previous kernel configuration
after a fragment changes.

The repository limits OpenOCD to 1 MHz after `reset init`. Old ST-LINK/V2
V2J17 firmware requires this rate; the upstream target script otherwise raises
SWD to 8 MHz and may fail with `flash write algorithm aborted by target`.
Override the conservative value only for known reliable hardware:

```bash
OPENOCD_ADAPTER_SPEED_KHZ=2000 make flash
```
