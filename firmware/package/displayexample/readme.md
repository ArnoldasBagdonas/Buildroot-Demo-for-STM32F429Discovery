# Display Example

This package enables the STM32F429Discovery LCD only when
`BR2_PACKAGE_DISPLAYEXAMPLE` is selected. It adds the display-specific Linux
configuration and device tree, `fbv`, the slideshow script, and compact PNG,
JPEG, and GIF test cards. `fbv` also accepts BMP files without another decoder
library.

The core display fragment retains procfs because Linux 6.1 framebuffer
registration needs `/proc/fb` in order to create `/dev/fb0`.

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
- The base DTB keeps USART3 disabled because its PB10/PB11 pins conflict with
  LTDC G4/G5. UART examples use a separate USART3-enabled DTB, and the build
  rejects selecting those examples together with the display.

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

## USART3/LTDC pin conflict

USART3 uses PB10/PB11 for `ioexample7` and `ioexample8`. The LCD needs the same
pins as `LTDC_G4` and `LTDC_G5`. If both peripherals are enabled, the ILI9341
SPI control interface binds but LTDC cannot claim its pinctrl state;
consequently DRM creates neither `card0` nor `/dev/fb0`.

The base DTB now keeps USART3 disabled. Selecting `ioexample7` or `ioexample8`
builds and flashes `stm32f429disco-usart3.dtb`, which enables it. The display
package builds its own DTB with LTDC enabled. `make build_all`, direct
Buildroot kernel builds, and `make flash` reject a configuration that selects
both kinds of example.

This does not affect the serial console. The console uses USART1 on PA9/PA10.
Those pins can use an external Cypress USB-serial adapter; a DISC1 board with
ST-LINK/V2-B may instead expose USART1 through its ST-LINK virtual COM port.
Only the separate USART3 example port is disabled in the base and display
images.

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

In `make menuconfig`, select `displayexample`. Do not select `ioexample7` or
`ioexample8`; the build deliberately rejects that PB10/PB11 conflict.

Check these milestones during every iteration:

1. `make build_all` must finish without an error.
2. `make flash` must print
   `Flashing DTB: .../stm32f429disco-display.dtb`.
3. The new BusyBox build timestamp must appear after reset.
4. Both panel and LTDC `driver` links must exist.
5. `/sys/class/drm/card0` and `/dev/fb0` must exist.
6. Run `displayexample`; verify that the bundled PNG, JPEG, and GIF cards are
   visible, navigation keys work, and `q` exits.

If any milestone fails, stop there, record the serial output, fix that layer,
and repeat the complete clean sequence. `make flash` alone never rebuilds
changed sources.

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
