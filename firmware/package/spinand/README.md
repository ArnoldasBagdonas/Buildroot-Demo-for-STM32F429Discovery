# W25N02KV SPI-NAND with a 32 MiB UBIFS volume

The `BR2_PACKAGE_SPINAND` package adds an external Winbond W25N02KV on SPI5.
It exposes the first 32 MiB of the chip as an MTD partition named `ubi`, enables
the Linux UBI and UBIFS layers, installs compact BusyBox management tools, and
builds a `-spinand.dtb` that `make flash` selects automatically.

The package never formats the NAND automatically. Formatting is an explicit
one-time operation, because it destroys everything in the 32 MiB partition.
Its options menu controls gzip initramfs compression and automatic mounting.
Both default to enabled. After initialization, the boot helper attaches and
mounts the `data` volume at `/mnt/spinand` before dependent services start. If
the chip is detected but is not initialized, the console suggests the guarded
format command without erasing anything automatically.

The standalone SPI-NAND image removes the unused network stack to conserve the
STM32F429's 2 MiB internal XIP flash. When Find My Device is also selected, its
IPv4/W5500 fragment is reapplied and the combined `-w5500-spinand.dtb` is
built. The SPI-NAND gzip-initramfs option is enabled by default for that
size-heavy composition; `make flash` still verifies every resulting image.
Standalone SD, Gallery SD support, Display, and USB serial have no pin conflict
with this wiring and are not excluded merely because a combination may be too
large. The flash command reports the actual size and refuses only the built
image that exceeds the internal-flash limit.

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
optional W5500, and either CY15B256Q FRAM or W25Q128FV SPI-NOR. Their active-low
chip selects are PC1 (`reg = <0>`), PC2 (`reg = <1>`), PD5 (`reg = <2>`), PG3
(`reg = <3>`), and PG2 (`reg = <4>`) respectively. PG3 is free I/O on P2 pin
61 and is not used by the other examples. The SD card is the only device on
SPI4, so a non-tristating SD adapter cannot interfere with SPI-NAND, W5500, or
FRAM/SPI-NOR MISO.

The Linux SPI core serializes SPI5 messages, changes the clock for each device,
and prevents simultaneous chip selection. A NAND erase/program or read can
delay W5500 servicing, while heavy Ethernet traffic can reduce NAND throughput.
The LCD primarily uses SPI5 for controller setup; framebuffer pixels travel
through LTDC. Current configured maxima are 10 MHz for NAND and FRAM/SPI-NOR, 4 MHz for
SD, and 1 MHz for W5500.

| Feature | SPI-NAND coexistence | Pins |
|---------|----------------------|------|
| W5500 | Yes when the image fits | SPI5 shared, NAND CS PG3, W5500 CS PD5 and IRQ PE3 |
| CY15B256Q FRAM | Yes with Find My Device | SPI5 shared, FRAM CS PG2, NAND CS PG3 |
| W25Q128FV SPI-NOR | Yes; alternative to FRAM | SPI5 shared, NOR CS PG2, NAND CS PG3 |
| SD card | Yes | Separate SPI4 PE2/PE5/PE6, SD CS PE4 |
| USB CDC | Yes | PB12/PB14/PB15 |
| SPI5 gyroscope | Yes | SPI5 shared; gyro CS PC1 |
| I²C3 | Yes | PA8/PC9 |
| PWM | Yes | PB4 |
| USART1/UART5 | Yes | PA9/PA10 and PC12/PD2 |
| LCD display / Gallery | Yes | LTDC; LCD, NAND, W5500, and FRAM/NOR share SPI5 with separate CS lines |
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
    W25N02KV SPI-NAND options
        [*] Compress the initramfs with gzip
        [*] Mount an initialized UBIFS volume automatically at boot
        [ ] Use UBI fastmap checkpoints (experimental)
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

The STM32 internal flash has a strict XIP kernel limit. `make flash` prints the
actual and maximum sizes before selecting or programming a DTB, and refuses an
image greater than 2,048,000 bytes.

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

With automount enabled, the same check is a boot prerequisite. An unformatted
device produces this actionable suggestion, and boot then continues without
erasing it:

```text
SPI-NAND was detected, but it has no usable UBI 'data' volume.
Inspect it without changing data: spinand-ubi status
To erase and initialize it once: spinand-ubi format --yes
```

The helper prints when boot-time detection, UBI attachment, and UBIFS mounting
begin, followed by an explicit success or failure. These operations are
synchronous and complete before Find My Device starts, preventing a storage
backend race. This minimal kernel has no block layer; SPI-NAND management uses
the UBI character control device and mounts the native `ubi0:data` source, so
it does not depend on `/dev/mtdblockN`.

## Boot-time performance

UBI attach scans eraseblock metadata and UBIFS then replays only the journal
work needed for a consistent mount. This example limits UBI to 32 MiB, or 256
128-KiB eraseblocks, rather than scanning the complete 256 MiB chip. Boot waits
for the attach and mount attempt to finish before starting dependent services,
so Find My Device always sees the final storage state. A missing MTD device adds
at most the two-second detection timeout; mounting an initialized volume also
includes UBI scan and UBIFS journal-replay time.

The default-off `Use UBI fastmap checkpoints (experimental)` option enables
`CONFIG_MTD_UBI_FASTMAP` and requests `fm_autoconvert` before `ubiattach`.
The first attach still performs a full scan and creates a checkpoint. Later
attaches search the first 64 eraseblocks for the fastmap anchor instead of
scanning metadata from all 256 eraseblocks. This improves only UBI attach;
initramfs decompression and UBIFS journal replay are unchanged.

Linux 6.6 labels the fastmap on-flash format experimental. It reserves space
for two checkpoint copies and adds occasional checkpoint updates. Back up data
before enabling it on an existing volume. This package normally creates the
`data` volume with `ubimkvol -m`, so an old maximum-size volume may not leave
enough free eraseblocks for conversion. If conversion cannot reserve them,
restore the previous configuration to recover the data, back it up, then
enable fastmap and use the guarded format command before restoring the files.

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
`spinand-ubi auto` during boot. Confirm that the file survived:

```sh
spinand-ubi status
cat /mnt/spinand/test.txt
```

Manual management commands are:

```sh
spinand-ubi status
spinand-ubi mount
spinand-ubi auto
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
- `firmware/board/stm32f429disco/linux-spinand-fastmap.config` enables the
  experimental kernel fastmap implementation only when its menu option is
  selected.
- `firmware/board/stm32f429disco/busybox-spinand.config` enables the compact
  erase and UBI management applets.
- `spinand-ubi` provides guarded initialization and normal mount/status
  operations.

The Linux SPI-NAND driver identifies W25N02KV from its JEDEC ID; no chip-name
compatible string is placed in the device tree. The generic DT compatible is
`spi-nand`.
