# W25Q128FV SPI-NOR with a JFFS2 data volume

This standalone Buildroot package connects a 16 MiB Winbond W25Q128FV to
SPI5, exposes it through Linux MTD, and mounts a JFFS2 data filesystem at
`/mnt/spinor`. It is independent of Find My Device, SD card, and SPI-NAND.
When Find My Device's W25Q option is selected, it uses the reserved raw state
partition; otherwise Find My Device can still discover the mounted JFFS2
volume at runtime.

JFFS2 works directly on raw MTD NOR flash. Unlike the SPI-NAND example, this
package does not use UBI, UBIFS, `ubiattach`, or UBI volumes.

## Wiring

Power off the Discovery board before changing wiring. W25Q128FV I/O is 3.3 V
only.

| W25Q128FV pin/label | STM32 signal | Discovery connector |
|---|---|---|
| 1 `CS#` | PG2, active low | P2 pin 62 |
| 2 `DO` / `IO1` / `MISO` | PF8 / SPI5_MISO | P2 pin 5 |
| 3 `WP#` / `IO2` | pull up to 3.3 V | — |
| 4 `GND` | ground | P2 pin 11 or pin 29 |
| 5 `DI` / `IO0` / `MOSI` | PF9 / SPI5_MOSI | P2 pin 8 |
| 6 `CLK` | PF7 / SPI5_SCK | P2 pin 6 |
| 7 `HOLD#` / `IO3` | pull up to 3.3 V | — |
| 8 `VCC` | 3.3 V | P2 pin 1 or pin 2 |

Use approximately 10 kΩ pull-ups on `CS#`, `WP#`, and `HOLD#`, and place a
100 nF ceramic capacitor close to VCC/GND. The device tree limits SPI to
10 MHz for jumper-wire operation.

## Hardware compatibility

SPI5 clock and data are shared safely by devices with independently controlled
and correctly tri-stating chip selects:

| Device | SPI5 chip select |
|---|---|
| Onboard gyroscope | PC1 / CS0 |
| LCD controller | PC2 / CS1 |
| W5500 | PD5 / CS2 |
| W25N02KV SPI-NAND | PG3 / CS3 |
| W25Q128FV SPI-NOR | PG2 / CS4 |

Linux serializes access to the shared SPI controller. Concurrent Ethernet,
NAND, NOR, and LCD-control traffic can reduce throughput but does not create a
pin conflict. The SD card remains isolated on SPI4 and may be selected at the
same time.

The optional CY15B256Q FRAM also uses PG2/CS4. FRAM and W25Q128FV therefore
cannot be connected or selected together. The Kconfig dependencies enforce
that restriction.

## Capacity and partition layout

W25Q128FV contains 16 MiB (16,777,216 bytes). The standalone package always
preserves the minimum two 4 KiB erase sectors for power-loss-safe Find My
Device A/B records. JFFS2 receives the remaining 16,769,024 bytes
(16,376 KiB):

```text
0x000000..0x001FFF  find-my-device-state    8 KiB (two 4 KiB sectors)
0x002000..0xFFFFFF  spinor-jffs2        16,376 KiB
```

The reservation is stable whether or not Find My Device is selected, so
toggling that example never moves an existing JFFS2 filesystem. The raw state
partition is not mounted or formatted. Find My Device accesses it directly
through `/dev/mtdX` only when its W25Q128FV option is enabled.

## Configuration

The external-options menu contains:

```text
[*] W25Q128FV SPI-NOR: JFFS2 data volume
    W25Q128FV SPI-NOR options  --->
        [*] Compress the initramfs with gzip
        [*] Mount an initialized JFFS2 volume automatically at boot
        [ ] Enable JFFS2 summary nodes for faster mounts
```

Compression is enabled by default because MTD and JFFS2 add kernel code while
the final `xipImage` must remain below the project's 2,048,000-byte limit.
JFFS2 summary support is optional: it adds summary metadata that can reduce
later mount scanning, at the cost of some kernel and on-flash space.

The Find My Device raw-state option is independent of this package. It enables
only the raw MTD driver, DT node, and record backend; it does not install or
mount a filesystem. Selecting both packages makes the same chip provide raw
A/B state plus the independent JFFS2 volume.

## First use

Build and flash from the devcontainer:

```sh
make menuconfig
make build_all
make flash
```

Inspect detection without modifying the chip:

```sh
spinor-jffs2 status
cat /proc/mtd
```

Formatting is an explicit, destructive, one-time operation:

```sh
spinor-jffs2 format --yes
```

This erases only the `spinor-jffs2` partition. Its separate first 8 KiB raw
state partition is not erased.

Test persistence:

```sh
echo 'W25Q128FV persistence test' > /mnt/spinor/test.txt
sync
cat /mnt/spinor/test.txt
df /mnt/spinor
```

After a reset or power cycle:

```sh
spinor-jffs2 status
cat /mnt/spinor/test.txt
```

The helper supports:

```text
spinor-jffs2 status
spinor-jffs2 mount
spinor-jffs2 auto
spinor-jffs2 unmount
spinor-jffs2 format --yes
```

Automatic boot handling only attempts a non-destructive mount. If detection or
mounting fails, it prints the manual format suggestion and continues booting.
The helper prints when synchronous detection/mounting begins and an explicit
success or failure when it ends. The minimal init script waits for that result
before starting Find My Device, so backend selection cannot race the mount.

If `mount` reports `No such device` even though `/proc/mtd` lists
`spinor-jffs2`, inspect `/proc/filesystems`. The kernel must list `jffs2`; this
error describes a missing filesystem driver rather than missing NOR hardware.
After changing a kernel configuration fragment, use `make linux-reconfigure`
before rebuilding so an older generated kernel configuration is not reused.

This project disables the Linux block layer to conserve internal flash. JFFS2
is therefore mounted through the native MTD source name
`mtd:spinor-jffs2`, not through `/dev/mtd1` or `/dev/mtdblock1`. The helper
handles this distinction; `/dev/mtd1` remains correct for erase operations.

## Integration

- `linux-spinor.config` enables SPI-NOR MTD and JFFS2 while the package is
  selected.
- `linux-spinor-summary.config` conditionally enables JFFS2 summaries.
- `busybox-spinor.config` enables the small on-target erase/status utilities.
- `stm32f429disco-spinor.dtsi` creates the fixed 8 KiB raw-state reservation
  followed by the JFFS2 partition.
- `stm32f429disco-spinor-state.dtsi` exposes only the raw reservation when
  Find My Device enables W25Q128FV without the standalone package.
- `spinor-jffs2` owns formatting, status, mounting, and boot detection.
- The minimal init script resolves SPI-NOR and SPI-NAND mounts before starting
  Find My Device, preventing storage-selection races.
