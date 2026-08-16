# SPI SD card adapter with FAT storage

The `BR2_PACKAGE_SDCARD` package connects a removable SD or SDHC card to SPI4,
enables Linux's `mmc_spi` block driver and VFAT filesystem, installs the
`sdcard` helper, and builds a `-sdcard.dtb` that `make flash` selects
automatically. A detected FAT16/FAT32 card is mounted at `/mnt/sdcard` during
boot and mounted again after a later removal and insertion. The package never
partitions or formats a card.

The chosen SPI4 pins do not overlap the STM32F429 Discovery LCD, so
`BR2_PACKAGE_SDCARD` and `BR2_PACKAGE_DISPLAY` can be selected together.
The package is mutually exclusive with Find My Device because its W5500 uses
the same SPI4 signals, and with the separate SPI-NAND storage image.

## How the helper is integrated

The file `package/sdcard/sdcard.c` is the source for a native program that runs
on the STM32 target; it is not a host-side image-building or flashing tool.
When `BR2_PACKAGE_SDCARD=y`, Buildroot treats this directory as a local package,
cross-compiles the source with the no-MMU ARM toolchain, installs the target
command as `/usr/sbin/sdcard`, and creates `/mnt/sdcard`.

The implementation uses Linux system calls for mounting, unmounting, syncing,
sleeping, signals, and device inspection. This keeps the long-running watcher
independent of optional BusyBox applets and demonstrates when a Buildroot
package should install a target program rather than a shell script.

When Display and SD card are selected together, `display.mk` compiles the SD
helper into the Display multicall executable and `sdcard.mk` installs
`/usr/sbin/sdcard` as a symbolic link to it. The commands remain independent,
but the combined firmware stores the static C runtime only once. With SD card
selected by itself, `sdcard.mk` builds `sdcard.c` as a standalone executable.
The command line and automount behavior are the same in both builds.

Selecting the package also makes the rest of the storage stack available:

- `linux-sdcard.config` enables the SPI MMC/SD block driver and FAT filesystem
  support in the kernel.
- `sdcard.mk` adds the matching `-sdcard` device tree to the kernel build. The
  device tree describes the SPI4 bus, chip-select, and MMC-over-SPI device.
- The root filesystem `init` script starts `/usr/sbin/sdcard watch &` when the
  helper is present, before opening the interactive shell.

At runtime, the kernel creates block devices such as `/dev/mmcblk0` after it
detects a card. The background `sdcard watch` process checks once per second
for a base MMC device under `/sys/class/block`. It uses partition 1, for
example `/dev/mmcblk0p1`, when that block device exists; otherwise it uses the
whole card, `/dev/mmcblk0`. It calls the kernel mount interface directly to
mount the selected device as `vfat` at `/mnt/sdcard`.

The helper supports these commands:

| Command | Behavior |
|---------|----------|
| `sdcard` or `sdcard status` | Reports the automount state, detected device, size in 512-byte sectors, selected mount device, and mount status. |
| `sdcard mount` | Enables automount and immediately mounts the detected card. |
| `sdcard unmount` | Disables automount, calls `sync`, and safely unmounts the card if it is mounted. |
| `sdcard watch` | Runs the continuous automount loop used by the boot-time `init` script. |

The automount setting is stored in the volatile file
`/run/sdcard-automount`. A manual `sdcard unmount` keeps automount disabled so
the watcher does not immediately remount the same card. Removing the card
resets the state to enabled, allowing the next insertion to mount
automatically. If a mounted card is removed without first being unmounted,
the watcher lazily detaches the stale mount, but this cannot prevent FAT
filesystem corruption caused by unsafe removal.

The helper only detects and mounts block devices. It does not configure the
SPI hardware, load the correct device tree, partition a card, create a
filesystem, or repair a damaged filesystem. Those responsibilities belong to
the build configuration, flashed `-sdcard.dtb`, and card-preparation tools.

## Wiring

Power the board off before changing connections. Keep all signal voltages at
3.3 V and keep the SPI wires short.

| SD adapter | STM32 signal | Expansion header |
|------------|--------------|------------------|
| `CS` | PE4, active low | P1 pin 13 |
| `SCK` / `CLK` | PE2 / SPI4_SCK | P1 pin 15 |
| `MISO` / `DO` | PE5 / SPI4_MISO | P1 pin 14 |
| `MOSI` / `DI` | PE6 / SPI4_MOSI | P1 pin 11 |
| `GND` | ground | P2 pin 11 or 29 |

Check the adapter itself before connecting its power pin. A bare 3.3 V socket
or a module explicitly rated for 3.3 V uses the board's 3 V rail (P2 pin 1 or
2). Some Arduino modules expect 5 V on `VCC` because they put a 3.3 V regulator
and input level shifters on the board; powering those modules from 3.3 V can
undervolt the card. If such a module is powered from 5 V, first verify that its
`MISO` output remains 3.3 V and never drives 5 V into the STM32. The signal
pins listed above must remain 3.3 V logic in every case.

There is no card-detect pin on the four-signal adapter. The kernel therefore
polls the slot. The DT starts conservatively at 4 MHz for jumper-wire use.

## Hardware compatibility

The documented SD-card wiring is compatible with the Display example. The SD
card uses SPI4 on PE2/PE4/PE5/PE6, while the LCD panel uses the LTDC signals
and SPI5, so their pins and controllers do not overlap. Keep the SD card on
these SPI4 pins when using the display; an SPI1 SD-card wiring on PA4–PA7 would
conflict with the LCD signals on PA4 and PA6.

The W5500 Find My Device example has a real hardware conflict with the SD card:
both devices use SPI4 on PE2, PE5, and PE6, and both current wiring definitions
use PE4 as the active-low chip select. They therefore cannot be connected or
selected together as currently defined. Sharing the SPI bus would require a
different chip-select pin for one device and a combined device tree, neither
of which this project provides.

The current prohibition against selecting the SD card together with SPI-NAND
is instead a software/image-size restriction, not a hardware conflict. The two
storage devices use independent SPI controllers and non-overlapping pins.

| Feature | SD-card coexistence | Pins |
|---------|---------------------|------|
| W5500 | No with current wiring | SPI4 PE2/PE5/PE6 and chip select PE4 are shared |
| SPI-NAND | Yes in hardware; unavailable in Kconfig | SPI1: PA4–PA7 |
| USB CDC | Yes | PB12/PB14/PB15 |
| SPI5 gyroscope | Yes | PF7/PF8/PF9 |
| I²C3 | Yes | PA8/PC9 |
| PWM | Yes | PB4 |
| USART1/UART5 | Yes | PA9/PA10 and PC12/PD2 |
| LCD display | Yes | LTDC does not use PE2/PE4/PE5/PE6 |
| DCMI | No with current pinctrl | DCMI data input D7 conflicts on PE6 |
| Internal RMII Ethernet | Pins do not conflict; networking is disabled in this image | RMII does not use PE2/PE4/PE5/PE6 |

## Build with the display example

Run this inside the devcontainer, where the repository is `/workspace`:

```sh
cd /workspace
make distclean
make configure
make menuconfig
```

Enable both of these symbols and save the configuration:

```text
BR2_PACKAGE_DISPLAY
BR2_PACKAGE_SDCARD
```

Then build without running `make configure` again:

```sh
make build_all
test -s buildroot/output/images/stm32f429disco-display-sdcard.dtb
stat -c 'xipImage size: %s bytes' buildroot/output/images/xipImage
```

Do not flash if `xipImage` exceeds 2,048,000 bytes. `make flash` enforces this
limit, reads the selection, and automatically uses
`stm32f429disco-display-sdcard.dtb`.

With the Display example's default autostart option enabled, boot starts the
SD automounter first and then starts the slideshow supervisor. Supported PNG,
JPEG, GIF, and BMP files placed directly in the card's root are selected
automatically. Images are scaled to fit the LCD while preserving aspect ratio;
large JPEGs are first reduced by libjpeg during decoding to limit peak RAM use.
If the card is absent or contains no supported root-level images, the slideshow
uses its bundled test images instead. Inspect the live choice with:

```sh
display-auto status
```

## Prepare and use a card

Use a card containing either a normal MBR first partition formatted as
FAT16/FAT32, or a whole-card FAT16/FAT32 filesystem. Insert it before boot.
The image mounts the first partition (normally `/dev/mmcblk0p1`) when present;
otherwise it tries the whole card (`/dev/mmcblk0`). exFAT is not enabled.

At the target shell:

```sh
sdcard status
ls -l /mnt/sdcard
echo 'STM32 SD persistence test' > /mnt/sdcard/test.txt
sync
cat /mnt/sdcard/test.txt
```

Before removing the card while powered, disable automount and flush pending
writes:

```sh
sdcard unmount
```

Automount remains disabled while that card is still present, so it will not
immediately undo the manual unmount. Removing the card re-arms automount; its
next insertion is detected by the kernel poller and mounted within a few
seconds. Pulling a mounted card without `sdcard unmount` can corrupt its FAT
filesystem.

To mount it again:

```sh
sdcard mount
```

If no `/dev/mmcblk0` appears, confirm that the flashed DTB ends in
`-sdcard.dtb`, then recheck CS, swapped MOSI/MISO, the common ground, adapter
power requirements, and card seating.
