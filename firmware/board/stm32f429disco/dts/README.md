# STM32F429Discovery device tree

The project builds one board tree:

```text
stm32f429disco-unified.dts
```

It includes the common board, display and touch controller, USB CDC data port,
W5500, find-me unique ID and EEPROM, ADC1 input, SPI-NAND, SD card, SPI-NOR,
and SPI-FRAM descriptions. The resulting image is
`buildroot/output/images/stm32f429disco-unified.dtb`.

Package selection controls the kernel configuration. A peripheral described in
the DTB is inactive when its package driver is not selected. This keeps one
hardware description for every example without adding unused driver code to
minimal images.

AFBOOT copies the DTB to the top 32 KiB of external SDRAM before entering
Linux. It updates `/chosen/bootloader,user-button` with the PA0 user-button
state sampled during boot. Linux reserves the passed DTB and exposes the value
through firmware device-tree sysfs.

## Source files

| File | Hardware |
|---|---|
| `stm32f429disco-custom.dts` | Board memory, clocks, console, GPIO, ADC1_IN13, SPI5 base, timers, PWM, and UART5 |
| `stm32f429disco-screen.dts` | LTDC panel and STMPE811 touch controller |
| `stm32f429disco-usb-cdc.dtsi` | USB USER connector in peripheral mode |
| `stm32f429disco-networking-w5500.dtsi` | W5500 Ethernet controller |
| `stm32f429disco-find-me.dtsi` | STM32 factory unique-ID NVMEM cell |
| `stm32f429disco-find-me-eeprom.dtsi` | 24LC16B EEPROM |
| `stm32f429disco-spinand.dtsi` | W25N02KV SPI-NAND and 32 MiB UBI partition |
| `stm32f429disco-sdcard.dtsi` | SPI4 SD-card slot |
| `stm32f429disco-spinor.dtsi` | W25Q128FV SPI-NOR partitions |
| `stm32f429disco-fram.dtsi` | CY15B256Q SPI-FRAM |

## SPI wiring

SPI5 shares PF7 SCK, PF8 MISO, and PF9 MOSI. Each device has a separate
active-low chip select:

| SPI5 CS | GPIO | Device |
|---:|---|---|
| 0 | PC1 | Onboard gyroscope |
| 1 | PC2 | LCD controller |
| 2 | PD5 | W5500 |
| 3 | PG3 | W25N02KV SPI-NAND |
| 4 | PG2 | W25Q128FV SPI-NOR |
| 5 | PC11, P1 pin 42 | CY15B256Q SPI-FRAM |

The SD card is the only SPI4 device and uses PE2 SCK, PE5 MISO, PE6 MOSI, and
PE4 CS. Moving FRAM to PC11 removes its former chip-select conflict with
SPI-NOR; both can now be selected together.

## Analog input

The `hwtools` ADC example uses the STM32's internal ADC1 channel 13 on PC3,
exposed at P2 pin 13. It does not use a chip select. PC3 is free from the
enabled board wiring and may be used with the SPI devices above. The input
range is 0-3.3 V; do not apply 5 V.

The unified DTB must remain below the 32 KiB flash slot beginning at
`0x08004000`. `make flash` checks this limit before programming the board.
