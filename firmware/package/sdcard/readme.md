# SPI SD card

`BR2_PACKAGE_SDCARD` enables an SD or SDHC card adapter on SPI4 and mounts the
first FAT16 or FAT32 partition at `/mnt/sdcard`. Boot mounting and periodic
insertion/removal detection are enabled by default.

The helper uses an existing filesystem. Card preparation and filesystem repair
are performed on another system.

## Commands

```sh
sdcard status
sdcard mount
sdcard unmount
sdcard watch
```

`unmount` disables remounting until the card is removed or `mount` is called.
Unmount before removing a card that contains writable data.

## Wiring

All signal pins use 3.3 V logic.

| SD adapter | STM32 signal | Connector |
|---|---|---|
| CS | PE4, active low | P1 pin 13 |
| SCK | PE2 / SPI4_SCK | P1 pin 15 |
| MISO | PE5 / SPI4_MISO | P1 pin 14 |
| MOSI | PE6 / SPI4_MOSI | P1 pin 11 |
| GND | Ground | P2 pin 11 or 29 |

The adapter's power input depends on its design: bare sockets use 3.3 V, while
some regulator-equipped modules expect 5 V at `VIN`. Verify that every signal
presented to the STM32 remains 3.3 V. The separate SPI4 bus allows this package
to coexist with W5500, SPI-NAND, SPI-NOR, and FRAM devices on SPI5.
