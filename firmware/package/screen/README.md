# Screen touch-console example

`BR2_PACKAGE_FIRMWARE_SCREEN` turns the STM32F429I-DISCO's 240x320 LCD and
resistive touchscreen into a small interactive terminal. It is an independent
root-level example: it does not select Display, Gallery, `fbv`, image decoder
libraries, or slideshow images. The learning goal is to connect a DRM fbdev
framebuffer, evdev touchscreen, terminal emulator, on-screen keyboard, and an
interactive no-MMU BusyBox shell without using a desktop graphics stack.

This package is unrelated to Buildroot's GNU Screen package. Its external
symbol and infrastructure prefix are deliberately distinct:

```text
BR2_PACKAGE_FIRMWARE_SCREEN
FIRMWARE_SCREEN_*
```

The installed foreground command can still use the natural name
`/usr/bin/screen`. Kconfig prevents selecting it when GNU Screen owns that
path. GNU Screen also requires an MMU and is unavailable on this target.

## Architecture

[`screen.c`](screen.c) opens `/dev/fb0`, activates the first advertised panel
mode, and maps the kernel framebuffer. It does not allocate a second 240x320
image. A small character-cell array stores only the console contents; glyphs,
scrolling rows, the cursor, and keyboard keys are drawn directly into fb0.
The built-in public-domain 5x7 font avoids a font package or runtime files.

The portrait display is split into two regions:

- the upper three fifths are a 30-column, 24-row automatically scrolling
  terminal at the native 240x320 mode;
- the lower two fifths contain five keyboard rows.

The terminal handles printable ASCII, CR, LF, Backspace, four-column tabs,
and the common BusyBox ANSI CSI operations for cursor movement, cursor
position, erase-line, erase-display, saved position, and basic colors. The
keyboard provides lowercase letters, latched Shift/uppercase, digits, shifted
shell punctuation, Space, Enter, Backspace, Tab, and Ctrl-C.

Screen creates one UNIX98 pseudo-terminal and runs `/bin/sh -i` on its slave.
The PTY is genuinely needed: its line discipline supplies echo, canonical
editing, Backspace, Tab, and signal generation for Ctrl-C. Screen creates
`/dev/pts` after devtmpfs is mounted and mounts devpts with `nosuid,noexec` and
restrictive slave permissions. It then uses `vfork()`, `setsid()`,
`TIOCSCTTY`, and `execv()`; it never calls `fork()`, which is unavailable on
this FLAT/no-MMU target.

Shell stdout and stderr return through the PTY master and are parsed into the
LCD console. Keyboard taps go to that same master. The program never reads
USART stdin and never changes the kernel `stdout-path`, so the independent
USART1 `cttyhack /bin/sh -i` session remains the recovery/debug console.

## Menuconfig options and conflicts

Search menuconfig for `BR2_PACKAGE_FIRMWARE_SCREEN`:

```text
[*] Screen: touch console with on-screen keyboard
    Screen options  --->
        [ ] Start the touch console automatically at boot
```

Autostart is intentionally default-off. Screen, Display, and Gallery all own
fb0, so selecting Screen makes the other two unavailable. Their Kconfig
dependencies also force an old conflicting configuration back to one owner.
Screen does not pull in `BR2_PACKAGE_FBV` or any Display/Gallery package.

The package applies the shared LCD fragment plus the size-oriented compact
fragment. Its own
[`linux-screen.config`](../../board/stm32f429disco/linux-screen.config)
re-enables only the input, STMPE811, I2C3, TTY, and UNIX98 PTY facilities that
Screen needs. It is applied after storage fragments so SD, SPI-NAND, SPI-NOR,
USB serial, and Find My Device compositions resolve consistently. When Find
My Device is absent, the unused network stack is removed to preserve internal
flash headroom; its network fragment is retained when selected.

The important built-in options are:

| Kernel option | Purpose |
|---|---|
| `CONFIG_INPUT` / `CONFIG_INPUT_EVDEV` | Input core and `/dev/input/event*` interface. |
| `CONFIG_INPUT_TOUCHSCREEN` | Touchscreen driver menu. |
| `CONFIG_MFD_STMPE` / `CONFIG_STMPE_I2C` | STMPE811 core and I2C transport. |
| `CONFIG_TOUCHSCREEN_STMPE` | Reports STMPE touch samples as evdev events. |
| `CONFIG_I2C` / `CONFIG_I2C_STM32F4` | Built-in I2C3 controller support. |
| `CONFIG_TTY` / `CONFIG_UNIX98_PTYS` | Private interactive shell terminal. |

Linux resolves `CONFIG_MFD_CORE` automatically. The resolved 6.6
configuration already has `CONFIG_REGMAP=y` for board syscon users, but
STMPE811's I2C driver performs direct I2C transfers and does not select
`CONFIG_REGMAP_I2C`; Screen therefore does not guess or force that transport.
Unused keyboard, mouse, serio, HID, and legacy PTY drivers are explicitly
disabled.

## LCD and touchscreen device tree

[`stm32f429disco-screen.dts`](../../board/stm32f429disco/dts/stm32f429disco-screen.dts)
is a complete touch-enabled display base. During the kernel build Screen copies
it to `stm32f429disco-display.dts` in the Linux build tree. All existing
`*-display*.dts` USB, network, SD, SPI-NAND, and SPI-NOR compositions can then
be reused without adding another filename matrix. Mutual exclusion guarantees
that Display or Gallery cannot copy a different base in the same build.

The LCD uses LTDC for pixel scanout. SPI5 chip select 1 on PC2 initializes the
ILI9341-compatible controller; PD13 is data/command. The touchscreen controller
is an STMPE811 at I2C3 address `0x41`, with its falling-edge interrupt on PA15.
The Linux 6.6 schema-compliant child is named exactly `touchscreen` and uses
`compatible = "st,stmpe-ts"`. No kernel patch is required: the upstream STMPE
MFD, I2C, touchscreen, evdev, STM32 DRM, and ILI9341 panel drivers provide the
complete path.

I2C3 remains a shared 100 kHz bus. Find My Device may add a 24LC16B at
`0x50`; that EEPROM responds over the device's normal `0x50`-`0x57` block
address range and does not conflict with STMPE811 at `0x41`. Both child nodes
remain in combined device trees.

SPI5 is also a valid shared bus. The gyroscope uses CS0/PC1, LCD CS1/PC2,
W5500 CS2/PD5, SPI-NAND CS3/PG3, and FRAM or SPI-NOR CS4/PG2. Linux serializes
their control transfers. LCD pixels travel through LTDC after initialization,
but heavy traffic on another SPI5 device can delay later panel control
transactions. Each device must retain a unique chip select and tristate MISO
while deselected.

## Touch coordinates

Screen discovers the controller by scanning every `/dev/input/event*` node.
It accepts only an input device named `stmpe-ts`, identified as `BUS_I2C`, with
`EV_ABS`, `ABS_X`, `ABS_Y`, `EV_KEY`, and `BTN_TOUCH` capabilities. It never
assumes that the touchscreen is `event0`.

At startup it reads both axes with `EVIOCGABS`, clamps samples to the reported
minimum/maximum, and scales them to the current framebuffer width and height.
The STM32F429I-DISCO panel wiring has X reversed in the normal portrait
orientation, so X inversion is enabled by default. Board revisions or a
rotated replacement panel can be adjusted from the serial console before
starting Screen:

```sh
SCREEN_INVERT_X=0 screen
SCREEN_INVERT_Y=1 screen
SCREEN_SWAP_XY=1 screen
SCREEN_SWAP_XY=1 SCREEN_INVERT_Y=1 screen
```

Each variable treats `0`, `no`, and `false` as disabled. Calibration uses the
kernel-reported ranges rather than hard-coded 12-bit constants. These flags
change orientation; a panel whose active edges do not reach the reported ADC
limits may still need board-specific min/max calibration after physical test.

## Commands and boot behavior

Run the console in the foreground:

```sh
screen
```

Only one instance can own fb0. `/run/screen.pid` prevents a foreground command
from racing the boot worker. With autostart enabled, `/usr/bin/screen-auto` is
a link to the same BFLT executable and supports:

```sh
screen-auto start
screen-auto stop
screen-auto restart
screen-auto status
```

The minimal init script starts the worker before it launches the USART shell.
The worker returns control to init immediately, then waits up to 60 seconds
for each required device. It prints explicit serial diagnostics:

```text
screen: waiting for framebuffer /dev/fb0
screen: framebuffer detected: 240x320, 16 bpp
screen: waiting for STMPE811 input device
screen: touchscreen detected: stmpe-ts (/dev/input/eventN), X=0..4095 Y=0..4095
screen: shell started: /bin/sh -i (PID N)
```

Timeouts, unsupported framebuffer depth, mmap, evdev capability, PTY, mount,
and shell-launch errors have a `screen: failed` or operation-specific message
on the serial console. Stop autostart before foreground testing:

```sh
screen-auto stop
screen
```

## Build and flash

Build inside the devcontainer as `vscode`; host builds create ownership
problems in Buildroot output:

```sh
cd /workspace
make configure
make menuconfig
make build_all
stat -c 'xipImage: %s bytes' buildroot/output/images/xipImage
make flash
```

The flash script treats Screen as a display hardware provider. A Screen-only
build selects `stm32f429disco-display.dtb`; combinations retain the existing
`usbserialdevice-display`, `display-w5500`, `display-sdcard`, `display-spinand`,
and `display-spinor` naming and suffix ordering.

The Screen executable is a roughly 70 KiB static BFLT before initramfs
packing. Input/STMPE/PTY kernel code also adds image bytes, while removal of
unneeded networking and generic input classes recovers much of that cost. The
exact `xipImage` size varies with other selected packages; `flash.sh` reports
it and refuses anything above the 2,048,000-byte XIP region.

## Troubleshooting

The serial console is the safest place to stop Screen and inspect the system:

```sh
screen-auto status
screen-auto stop
ls -l /dev/fb0 /sys/class/graphics/fb0
cat /sys/class/graphics/fb0/modes
cat /sys/class/graphics/fb0/mode
ls -l /dev/input/event* /sys/class/input
for e in /sys/class/input/event*; do echo "$e: $(cat "$e/device/name")"; done
cat /proc/bus/input/devices
dmesg | grep -i -e stmpe -e touch -e i2c -e ltdc -e drm -e ili9341
```

Expected kernel evidence includes STMPE811 on I2C3, an input device named
`stmpe-ts`, an event node with absolute X/Y and `BTN_TOUCH`, STM32 LTDC DRM,
the ILI9341 panel, and `/dev/fb0`. If fb0 exists but is blank, write the first
line from `modes` into `mode` or run `screen`, which performs that modeset.

If no touch event exists, confirm the correct display DTB was flashed, inspect
PA15 interrupt counts, check address `0x41`, and verify that the STMPE options
are `=y` in the resolved kernel config. A detected device with mirrored taps
needs one of the coordinate flags above. Offset or compressed edges require
measurements from the physical unit before changing calibration. If the LCD
works until another SPI5 peripheral is attached, recheck unique CS wiring and
whether the external device releases MISO. None of these checks require
disabling the USART recovery shell.
