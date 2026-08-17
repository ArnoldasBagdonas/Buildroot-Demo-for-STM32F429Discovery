# SPI SD card adapter with FAT storage

The `BR2_PACKAGE_SDCARD` package connects a removable SD or SDHC card to SPI4,
enables Linux's `mmc_spi` block driver and VFAT filesystem, installs the
`sdcard` helper, and builds a `-sdcard.dtb` that `make flash` selects
automatically. Its options independently control a boot-time mount attempt and
continued periodic insertion/removal detection. Both behaviors default to
enabled for compatibility with the original example. The package never
partitions or formats a card.

The chosen SPI4 pins do not overlap the STM32F429 Discovery LCD. The standalone
package remains independent of Display; select `BR2_PACKAGE_GALLERY` for the
supported Gallery image. It can be selected with Find My Device: the card
stays on SPI4 and W5500 uses SPI5, so the external modules do not share MISO.
The package remains mutually exclusive with the separate
SPI-NAND storage image because of the internal-flash image-size limit.

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

`sdcard.mk` always compiles and installs a standalone `/usr/sbin/sdcard`; it
does not inspect the Display selection, depend on Display, or install a link to
another package's executable. When boot automounting is selected, it also
installs `/usr/sbin/sdcard-auto` as a link to that executable. Gallery is a
separate package owner that links the same `sdcard_main()` applet into its own
executable. Gallery does not consume these standalone-package options and
retains its existing `sdcard watch` startup policy.

Selecting the package also makes the rest of the storage stack available:

- `linux-sdcard.config` enables the SPI MMC/SD block driver and FAT filesystem
  support in the kernel.
- `sdcard.mk` adds the matching `-sdcard` device tree to the kernel build. The
  device tree describes the SPI4 bus, chip-select, and MMC-over-SPI device.
- The root filesystem `init` script starts `/usr/sbin/sdcard-auto &` only when
  the package installed that optional link. The selected build mode makes it
  perform either one mount attempt or the continuous watcher.

At runtime, the kernel creates block devices such as `/dev/mmcblk0` after it
detects a card. A mount operation uses partition 1, for example
`/dev/mmcblk0p1`, when that block device exists; otherwise it uses the whole
card, `/dev/mmcblk0`. It calls the kernel mount interface directly to mount the
selected device as `vfat` at `/mnt/sdcard`. In periodic mode, the background
watcher checks once per second for a base MMC device under `/sys/class/block`.

The helper supports these commands:

| Command | Behavior |
|---------|----------|
| `sdcard` or `sdcard status` | Reports the automount state, detected device, size in 512-byte sectors, selected mount device, and mount status. |
| `sdcard mount` | Enables automount and immediately mounts the detected card. |
| `sdcard unmount` | Disables automount, calls `sync`, and safely unmounts the card if it is mounted. |
| `sdcard watch` | Manually runs the same continuous automount loop selected by periodic boot detection. |
| `sdcard-auto` | Internal boot applet link. It mounts once or watches continuously according to the package configuration. |

When the periodic watcher is running, its automount setting is stored in the
volatile file `/run/sdcard-automount`. A manual `sdcard unmount` keeps
automount disabled so the watcher does not immediately remount the same card.
Removing the card resets the state to enabled, allowing the next insertion to
mount automatically. If a mounted card is removed without first being
unmounted, the watcher lazily detaches the stale mount, but this cannot prevent
FAT filesystem corruption caused by unsafe removal.

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
polls the slot. The DT uses a conservative SPI clock of up to 4 MHz.

## Hardware compatibility

The documented SD-card wiring is compatible with the Display example. The SD
card uses SPI4 on PE2/PE4/PE5/PE6, while the LCD panel uses the LTDC signals
and SPI5, so their pins and controllers do not overlap. Keep the SD card on
these SPI4 pins when using the display; an SPI1 SD-card wiring on PA4–PA7 would
conflict with the LCD signals on PA4 and PA6.

The W5500 Find My Device example uses SPI5 on PF7, PF8, and PF9, with PD5 chip
select and PE3 interrupt. It does not share SPI4 or the SD adapter's MISO
signal. This is deliberate: some low-cost SD adapters keep MISO driven through
their level-shifter circuit even while PE4 chip select is high. Separate chip
selects cannot repair that electrical behavior, whereas separate controllers
and data pins do. Selecting both packages builds a combined
`-w5500-sdcard.dtb`; Find My Device then stores persistent state under
`/mnt/sdcard/find-my-device` when the card mounts successfully.

The current prohibition against selecting the SD card together with SPI-NAND
is a software/image-size restriction, not a hardware conflict. SPI-NAND uses
SPI5 with PG3 chip select while SD remains the only device on SPI4. Their
transfers can proceed on separate controllers.

| Feature | SD-card coexistence | Pins |
|---------|---------------------|------|
| W5500 | Yes | Separate SPI5 PF7/PF8/PF9; W5500 CS PD5 and IRQ PE3 |
| SPI-NAND | Yes in hardware; unavailable in Kconfig | Separate SPI5 PF7/PF8/PF9; NAND CS PG3 |
| USB CDC | Yes | PB12/PB14/PB15 |
| SPI5 gyroscope | Yes | SPI5 shared with W5500/SPI-NAND; gyro CS PC1 |
| I²C3 | Yes | PA8/PC9 |
| PWM | Yes | PB4 |
| USART1/UART5 | Yes | PA9/PA10 and PC12/PD2 |
| LCD display | Yes | LTDC plus SPI5 LCD CS PC2; SPI core serializes W5500/NAND traffic |
| DCMI | No with current pinctrl | DCMI data input D7 conflicts on PE6 |
| Internal RMII Ethernet | Pins do not conflict; networking is disabled in this image | RMII does not use PE2/PE4/PE5/PE6 |

## Build the standalone example

Run this inside the devcontainer, where the repository is `/workspace`:

```sh
cd /workspace
make distclean
make configure
make menuconfig
```

Enable this symbol and save the configuration:

```text
BR2_PACKAGE_SDCARD
```

Selecting it reveals an `SD card options` menu:

```text
[ ] Compress the initramfs with gzip
[*] Mount an inserted SD card automatically at boot
[*] Periodically detect and automatically mount SD cards
```

The periodic option appears only when boot automounting is enabled. With both
defaults selected, `sdcard-auto` runs the watcher and also handles a card that
is present during boot. Disable periodic detection for one boot-time attempt,
or disable boot automounting to install only the manual `sdcard` command.

The initramfs compression option is disabled by default; enable it when this
example is combined with enough other packages to approach the internal-flash
limit:

```text
BR2_PACKAGE_SDCARD_COMPRESS_INITRAMFS
```

Then build without running `make configure` again:

```sh
make build_all
test -s buildroot/output/images/stm32f429disco-custom-sdcard.dtb
stat -c 'xipImage size: %s bytes' buildroot/output/images/xipImage
```

Do not flash if `xipImage` exceeds 2,048,000 bytes. `make flash` enforces this
limit, reads the selection, and automatically uses
`stm32f429disco-custom-sdcard.dtb`.

The SD package always applies the shared compact kernel fragment, which removes
unused features but does not choose an initramfs compression format. The
`BR2_PACKAGE_SDCARD_COMPRESS_INITRAMFS` option additionally applies
`linux-initramfs-gzip.config`. With it enabled, the embedded root filesystem is
gzip-compressed in flash and expanded into RAM at boot. The kernel itself
remains XIP and continues executing from flash. With the option disabled, the
embedded initramfs remains uncompressed.

For the single-executable LCD composition, select `BR2_PACKAGE_GALLERY` and
`BR2_PACKAGE_GALLERY_SDCARD`. Gallery's built-in support disables this
standalone package and prevents duplicate ownership of the `sdcard` command.
Its nested `BR2_PACKAGE_GALLERY_SDCARD_IMAGES` option independently decides
whether the slideshow reads card images. Gallery builds the combined DTB and
owns the multicall executable and all applet links. See
[`../gallery/README.md`](../gallery/README.md).

For persistent Find My Device state, enable both standalone symbols:

```text
BR2_PACKAGE_FIND_MY_DEVICE
BR2_PACKAGE_SDCARD
```

This does not make either package select the other. The combination builds the
dual-controller `-w5500-sdcard.dtb`; wire W5500 to SPI5 with PD5 chip select
as documented
in [`../find-my-device/README.md`](../find-my-device/README.md).

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

With periodic detection selected, automount remains disabled while that card
is still present, so it will not immediately undo the manual unmount. Removing
the card re-arms automount; its next insertion is detected and mounted within a
few seconds. Without periodic detection, insertions after boot require the
manual command below. Pulling a mounted card without `sdcard unmount` can
corrupt its FAT filesystem.

To mount it again:

```sh
sdcard mount
```

If no `/dev/mmcblk0` appears, confirm that the flashed DTB ends in
`-sdcard.dtb`, then recheck CS, swapped MOSI/MISO, the common ground, adapter
power requirements, and card seating.
