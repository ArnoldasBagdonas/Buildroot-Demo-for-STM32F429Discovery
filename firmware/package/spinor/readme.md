# W25Q128FV SPI-NOR

`BR2_PACKAGE_SPINOR` exposes a W25Q128FV through Linux MTD. The first two 4 KiB
sectors are reserved for find-me A/B state records; the remaining 16,376 KiB
forms a JFFS2 volume mounted at `/mnt/spinor`.

Gzip initramfs compression and mounting of an initialized filesystem at boot
are enabled by default. JFFS2 summary nodes are optional and disabled by
default.

## Commands

```sh
spinor-jffs2 status
spinor-jffs2 mount
spinor-jffs2 unmount
spinor-jffs2 format --yes
```

`format --yes` erases and recreates only the JFFS2 data partition. The two
reserved find-me sectors are outside that partition.

## Wiring

| W25Q128FV signal | STM32 signal | Connector |
|---|---|---|
| CS# | PG2, active low | P2 pin 62 |
| CLK | PF7 / SPI5_SCK | P2 pin 6 |
| DO/IO1 | PF8 / SPI5_MISO | P2 pin 5 |
| DI/IO0 | PF9 / SPI5_MOSI | P2 pin 8 |
| VCC | 3.3 V | P2 pin 1 or 2 |
| GND | Ground | P2 pin 11 or 29 |
| WP#/IO2 | Pull up to 3.3 V | — |
| HOLD#/IO3 | Pull up to 3.3 V | — |

SPI-NOR uses SPI5 CS4 and can coexist with FRAM on PC11/CS5, SPI-NAND on
PG3/CS3, W5500 on PD5/CS2, the LCD on PC2/CS1, and the onboard gyroscope on
PC1/CS0.
