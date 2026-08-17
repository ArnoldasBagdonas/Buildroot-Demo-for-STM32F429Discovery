# W25N02KV SPI-NAND with a 32 MiB UBIFS volume

The `BR2_PACKAGE_SPINAND` package adds an external Winbond W25N02KV on SPI5.
It exposes the first 32 MiB of the chip as an MTD partition named `ubi`, enables
the Linux UBI and UBIFS layers, installs compact BusyBox management tools, and
builds a `-spinand.dtb` that `make flash` selects automatically.

The package never formats the NAND automatically. Formatting is an explicit
one-time operation, because it destroys everything in the 32 MiB partition.
After initialization, the image automatically attaches and mounts the `data`
volume at `/mnt/spinand` on every boot.

The standalone SPI-NAND image removes the unused network stack to conserve the
STM32F429's 2 MiB internal XIP flash. When Find My Device is also selected, its
IPv4/W5500 fragment is reapplied and the combined `-w5500-spinand.dtb` is
built. Enable Find My Device's gzip-initramfs option for that size-heavy
composition, and still verify the final image against the size check below.
Standalone SD card and Gallery's built-in SD support remain excluded by
Kconfig for image size, not wiring. Display, embedded-image Gallery, and USB
serial have no pin conflict with this wiring, but every resulting image must
independently pass the size check.

## Capacity used by this image

Despite its name, W25N02KV is a 2-Gbit (256 MiB) SPI-NAND device. This initial
configuration intentionally manages only the first 32 MiB:

```text
W25N02KV total:       0x10000000 = 256 MiB
UBI MTD partition:    0x02000000 =  32 MiB, starting at offset 0
Reserved/unexposed:   0x0e000000 = 224 MiB
```

UBI consumes some eraseblocks for its layout, bad-block handling, and
wear-leveling, so the mounted filesystem reports less than 32 MiB usable.
Changing the partition size later requires backing up the data and recreating
the UBI volume; do not just change the DTS size on a populated chip.

## Hardware and wiring

Use only 3.3 V. Never connect the flash VCC or an I/O pin to a 5 V header pin.
Connect the board and flash grounds together. Place a 100 nF decoupling
capacitor directly between the flash VCC and GND if the breakout board does not
already include one.

| W25N02KV signal | STM32 signal                     | Discovery connector    |
|-----------------|----------------------------------|------------------------|
| `CS#`           | PG3, active low                  | P2 pin 61              |
| `CLK`           | PF7 / SPI5_SCK                   | P2 pin 6               |
| `DO/IO1`        | PF8 / SPI5_MISO                  | P2 pin 5               |
| `DI/IO0`        | PF9 / SPI5_MOSI                  | P2 pin 8               |
| `VCC`           | 3.3 V                            | P2 pin 1 or pin 2      |
| `GND`           | GND                              | P2 pin 11 or pin 29    |
| `WP#/IO2`       | pull up to 3.3 V through 10 kOhm | not connected to STM32 |
| `HOLD#/IO3`     | pull up to 3.3 V through 10 kOhm | not connected to STM32 |

Many SPI-NAND modules already pull up `WP#/IO2` and `HOLD#/IO3`; check the
module schematic before adding duplicate resistors. Bare WSON devices require
the pull-ups for the single-data-line mode used here. A pull-up on `CS#` is
also recommended so the flash remains deselected while the GPIO is being
initialized. Keep CLK and data wires short. The DT starts conservatively at
10 MHz.

Power off the board before changing any connection. Double-check VCC and GND
with the module's own pinout; breakout-board pin order is not standardized.

## Hardware compatibility

SPI-NAND shares SPI5 clock and data with the onboard gyroscope, LCD controller,
and optional W5500. Their active-low chip selects are PC1 (`reg = <0>`), PC2
(`reg = <1>`), PD5 (`reg = <2>`), and PG3 (`reg = <3>`) respectively. PG3 is
free I/O on P2 pin 61 and is not used by the current examples. The SD card is
the only device on SPI4, so a non-tristating SD adapter cannot interfere with
SPI-NAND or W5500 MISO.

The Linux SPI core serializes SPI5 messages, changes the clock for each device,
and prevents simultaneous chip selection. A NAND erase/program or read can
delay W5500 servicing, while heavy Ethernet traffic can reduce NAND throughput.
The LCD primarily uses SPI5 for controller setup; framebuffer pixels travel
through LTDC. Current configured maxima are 10 MHz for NAND, 4 MHz for SD, and
1 MHz for W5500.

| Feature | SPI-NAND coexistence | Pins |
|---------|----------------------|------|
| W5500 | Yes when the image fits | SPI5 shared, NAND CS PG3, W5500 CS PD5 and IRQ PE3 |
| SD card | Yes in hardware; unavailable in Kconfig because of image size | Separate SPI4 PE2/PE5/PE6, SD CS PE4 |
| USB CDC | Yes | PB12/PB14/PB15 |
| SPI5 gyroscope | Yes | SPI5 shared; gyro CS PC1 |
| I²C3 | Yes | PA8/PC9 |
| PWM | Yes | PB4 |
| USART1/UART5 | Yes | PA9/PA10 and PC12/PD2 |
| LCD display / Gallery | Yes | LTDC; LCD, NAND, and W5500 share SPI5 with separate CS lines |
| DCMI | Yes with SPI-NAND alone | DCMI does not use PF7/PF8/PF9/PG3 |
| Internal RMII Ethernet | Pins do not conflict; networking is disabled in this image | RMII does not use PF7/PF8/PF9/PG3 |

## Build the image

Run these commands from the repository root. Inside the devcontainer that path
is `/workspace`.

```sh
make distclean
make configure
make menuconfig
```

In `menuconfig`, press `/`, search for `BR2_PACKAGE_SPINAND`, open the reported
location, and enable:

```text
W25N02KV SPI-NAND: 32 MiB UBIFS data volume
```

Save and exit, then confirm the temporary selection:

```sh
grep '^BR2_PACKAGE_SPINAND=y$' buildroot/.config
```

Build without running `make configure` again, because that would discard the
temporary menu selection:

```sh
make build_all
```

The selected image should contain a dedicated DTB:

```sh
test -s buildroot/output/images/stm32f429disco-custom-spinand.dtb
```

If Display, Gallery, or USB serial is also selected, the prefix changes to
match that feature but still ends in `-spinand.dtb`.

The STM32 internal flash has a strict XIP kernel limit. Check it before every
flash:

```sh
stat -c 'xipImage size: %s bytes' buildroot/output/images/xipImage
```

Do not flash if the size is greater than 2,048,000 bytes.

## Flash and inspect the device

With ST-LINK attached, flash normally. The script reads `buildroot/.config`
and selects the matching `-spinand.dtb` automatically.

```sh
make flash
```

Open the USART1/ST-LINK console at 115200 baud and reset the board. For example,
on the host:

```sh
picocom -b 115200 /dev/ttyACM0
```

At the target shell, inspect the detected NAND without changing its contents:

```sh
spinand-ubi status
```

For this DTS, the important expected values are:

```text
name:      ubi
size:      33554432
erasesize: 131072
writesize: 2048
```

The exact MTD number is discovered by name and may vary. A missing `ubi` MTD
partition normally means that the wrong DTB was flashed or SPI wiring is
incorrect. Recheck `CS#`, swapped `DI`/`DO`, the two pull-ups, ground, and 3.3 V.

## One-time destructive initialization

Only after `spinand-ubi status` reports the expected 32 MiB device, erase and
initialize it:

```sh
spinand-ubi format --yes
```

This command:

1. erases the complete 32 MiB MTD partition while respecting bad blocks;
2. attaches it as `/dev/ubi0`;
3. creates a dynamic UBI volume named `data` using the available space; and
4. mounts it as UBIFS at `/mnt/spinand`.

Do not interrupt power during the initial erase and format. The operation is
intentionally rejected when `--yes` is omitted.

## Persistence test

Write a file, flush it, and inspect the mounted capacity:

```sh
echo 'W25N02KV persistence test' > /mnt/spinand/test.txt
sync
df /mnt/spinand
cat /mnt/spinand/test.txt
```

Reset or power-cycle the board. The image automatically runs
`spinand-ubi mount` during boot. Confirm that the file survived:

```sh
spinand-ubi status
cat /mnt/spinand/test.txt
```

Manual management commands are:

```sh
spinand-ubi status
spinand-ubi mount
spinand-ubi unmount
```

`spinand-ubi format --yes` is destructive and is not a normal mount or repair
command. UBIFS is designed to recover its journal after an unclean shutdown;
do not reformat merely because power was removed without an unmount.

## Relevant implementation files

- `firmware/board/stm32f429disco/dts/stm32f429disco-spinand.dtsi` defines
  shared SPI5 pins, PG3 chip select, the flash node, and the fixed 32 MiB
  partition.
- `firmware/board/stm32f429disco/linux-spinand.config` enables MTD, SPI-NAND,
  UBI, and UBIFS only while this package is selected.
- `firmware/board/stm32f429disco/busybox-spinand.config` enables the compact
  erase and UBI management applets.
- `spinand-ubi` provides guarded initialization and normal mount/status
  operations.

The Linux SPI-NAND driver identifies W25N02KV from its JEDEC ID; no chip-name
compatible string is placed in the device tree. The generic DT compatible is
`spi-nand`.
