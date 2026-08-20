# W25N02KV SPI-NAND

`BR2_PACKAGE_SPINAND` exposes the first 32 MiB of a W25N02KV as an UBI-managed
UBIFS data volume mounted at `/mnt/spinand`. The remainder of the 256 MiB chip
is outside the current partition.

Gzip initramfs compression and mounting of an initialized volume at boot are
enabled by default. Optional UBI fastmap support is disabled by default.

## Commands

```sh
spinand-ubi status
spinand-ubi mount
spinand-ubi unmount
spinand-ubi format --yes
```

`format --yes` erases and recreates the configured UBI partition. Use it once
for new media or after intentionally discarding the existing data.

## Wiring

| W25N02KV signal | STM32 signal | Connector |
|---|---|---|
| CS# | PG3, active low | P2 pin 61 |
| CLK | PF7 / SPI5_SCK | P2 pin 6 |
| DO/IO1 | PF8 / SPI5_MISO | P2 pin 5 |
| DI/IO0 | PF9 / SPI5_MOSI | P2 pin 8 |
| VCC | 3.3 V | P2 pin 1 or 2 |
| GND | Ground | P2 pin 11 or 29 |
| WP#/IO2 | Pull up to 3.3 V | — |
| HOLD#/IO3 | Pull up to 3.3 V | — |

Use short wires, a local 100 nF decoupling capacitor, and approximately 10 kΩ
pull-ups when the module does not provide them. SPI5 is shared with the LCD,
gyroscope, W5500, SPI-NOR, and FRAM; each device has a separate chip select.
