# find-me

`BR2_PACKAGE_FIND_ME` provides a small network onboarding service for the
STM32F429Discovery. It selects the Networking package and its W5500 driver,
obtains an IPv4 address with DHCP or link-local fallback, advertises
`_device-setup._tcp.local` over mDNS, and serves the device information,
OAuth, and WebSocket endpoints.

The device ID and locally administered MAC address are derived from the STM32
factory UID. The onboard USER button confirms requests and the green LED shows
status. Gzip initramfs compression and boot autostart are enabled by default.

## Commands

```sh
find-me --help
find-me-service {start|stop|restart|status}
```

`find-me-service` links to `/etc/init.d/S40find-me`. Configuration is read from
`/etc/find-me.conf`. The optional console-debug setting also installs
`find-me-debug` and `find-me-confirm`.

## Network wiring

All signals use 3.3 V logic.

| W5500 signal | STM32 signal |
|---|---|
| SCK | PF7 / SPI5_SCK |
| MISO | PF8 / SPI5_MISO |
| MOSI | PF9 / SPI5_MOSI |
| CS | PD5, active low |
| INT | PE3, active low |
| RST | Board NRST |

## Persistent state

Optional raw state backends are:

| Device | Interface |
|---|---|
| CY15B256Q FRAM | SPI5, PC11/CS5, P1 pin 42 |
| 24LC16B EEPROM | I2C3, PA8/SCL and PC9/SDA |
| W25Q128FV SPI-NOR | SPI5, PG2/CS4, two reserved 4 KiB sectors |

Mounted SD, SPI-NOR/JFFS2, and SPI-NAND/UBIFS filesystems may also store state.
If no persistent backend is available, the service uses the RAM-backed root
filesystem for that boot. See [the package overview](../readme.md) for build
and image-size information.

## W5500 patch dependency

Find-me selects Networking and therefore relies on its W5500 Linux fixes. They
were developed from failures visible in this workload: DHCP transmit could
stall, periodic authorization requests exposed an unstable receive-size read,
and masked level interrupts could leave an RX frame pending. See
[Networking's issue, solution, and manufacturer evidence](../networking/readme.md#why-the-w5500-driver-is-patched).
Find-me itself does not patch third-party source.
