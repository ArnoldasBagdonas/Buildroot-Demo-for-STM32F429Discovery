# W25N02KV SPI-NAND with a 32 MiB UBIFS volume

The `BR2_PACKAGE_SPINAND` package adds an external Winbond W25N02KV on SPI1.
It exposes the first 32 MiB of the chip as an MTD partition named `ubi`, enables
the Linux UBI and UBIFS layers, installs compact BusyBox management tools, and
builds a `-spinand.dtb` that `make flash` selects automatically.

The package never formats the NAND automatically. Formatting is an explicit
one-time operation, because it destroys everything in the 32 MiB partition.
After initialization, the image automatically attaches and mounts the `data`
volume at `/mnt/spinand` on every boot.

This is a standalone storage image. It cannot be selected together with
`BR2_PACKAGE_FIND_MY_DEVICE`: Linux 6.6 with both UBIFS and the IPv4/W5500
stack exceeds the STM32F429's 2 MiB internal XIP-flash limit. The SPI-NAND
fragment therefore removes the network stack, and Kconfig prevents the unsafe
combination. The display example is also unavailable because its LTDC signals
use PA4 and PA6. USB serial may still be selected, but its resulting image must
independently pass the size check below.

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

| W25N02KV signal | STM32 signal                     | Board connector        |
|-----------------|----------------------------------|------------------------|
| `CS#`           | PA4 / SPI1 chip select           | pin 1                  |
| `CLK`           | PA5 / SPI1_SCK                   | pin 6                  |
| `DO/IO1`        | PA6 / SPI1_MISO                  | pin 2                  |
| `DI/IO0`        | PA7 / SPI1_MOSI                  | pin 5                  |
| `VCC`           | 3.3 V                            | pin 8                  |
| `GND`           | GND                              | pin 4                  |
| `WP#/IO2`       | pull up to 3.3 V through 10 kOhm | not connected to STM32 |
| `HOLD#/IO3`     | pull up to 3.3 V through 10 kOhm | not connected to STM32 |

Many SPI-NAND modules already pull up `WP#/IO2` and `HOLD#/IO3`; check the
module schematic before adding duplicate resistors. Bare WSON devices require
the pull-ups for the single-data-line mode used here. Keep CLK and data wires
short. The DT starts conservatively at 10 MHz.

Power off the board before changing any connection. Double-check VCC and GND
with the module's own pinout; breakout-board pin order is not standardized.

## Hardware compatibility

The current prohibition against selecting SPI-NAND together with the W5500
Find My Device example is a software/image-size restriction, not a hardware
conflict. The two devices use independent SPI controllers and non-overlapping
pins.

| Feature | SPI-NAND coexistence | Pins |
|---------|----------------------|------|
| W5500 | Yes | SPI4: PE2–PE6 |
| USB CDC | Yes | PB12/PB14/PB15 |
| SPI5 gyroscope | Yes | PF7/PF8/PF9 |
| I²C3 | Yes | PA8/PC9 |
| PWM | Yes | PB4 |
| USART1/UART5 | Yes | PA9/PA10 and PC12/PD2 |
| LCD display | No with current wiring | LCD conflicts on PA4 and PA6 |
| DCMI/internal RMII Ethernet | Conflicts if enabled | Uses some PA4–PA7 pins |

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

If USB serial is also selected, the prefix changes to match that feature but
still ends in `-spinand.dtb`.

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
  SPI1 pins, the flash node, and the fixed 32 MiB partition.
- `firmware/board/stm32f429disco/linux-spinand.config` enables MTD, SPI-NAND,
  UBI, and UBIFS only while this package is selected.
- `firmware/board/stm32f429disco/busybox-spinand.config` enables the compact
  erase and UBI management applets.
- `spinand-ubi` provides guarded initialization and normal mount/status
  operations.

The Linux SPI-NAND driver identifies W25N02KV from its JEDEC ID; no chip-name
compatible string is placed in the device tree. The generic DT compatible is
`spi-nand`.
