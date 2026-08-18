# Find My Device package (W5500 + mDNS)

This external Buildroot package is the Linux/STM32F429 counterpart of the
FreeRTOS `find-my-device` project next to this repository. It provides:

- an in-kernel W5500 Ethernet interface on SPI5;
- DHCP with deterministic IPv4 link-local fallback;
- a stable 64-bit device ID and locally administered MAC derived from the
  STM32 factory UID;
- `_device-setup._tcp.local` mDNS announcements and query responses;
- API v3 device information, OAuth authorization-code + PKCE, physical
  confirmation, refresh-token rotation, renaming, mobile registration, and
  authenticated WebSocket status events;
- optional power-loss-safe state storage in CY15B256Q SPI FRAM;
- optional power-loss-safe state storage in 24LC16B I2C EEPROM;
- optional alternating raw-MTD records on the W25Q128FV SPI-NOR;
- runtime use of mounted SD, SPI-NOR/JFFS2, and SPI-NAND/UBIFS storage;
- on-board USER-button confirmation and green status-LED controls.

## W5500 wiring

Power the board off before wiring and keep every signal at 3.3 V logic. A bare
W5500 or 3.3 V-only breakout must use the board's 3 V rail. A module with an
onboard 3.3 V regulator may instead require 5 V on its separately labelled
`5V` or `VIN` input; follow that module's schematic and never apply 5 V to a
signal pin.

| W5500 label    | STM32 signal    |
|----------------|-----------------|
| `SCLK` / `SCK` | PF7 / SPI5_SCK  |
| `MISO`         | PF8 / SPI5_MISO |
| `MOSI`         | PF9 / SPI5_MOSI |
| `SCS` / `CS`   | PD5, active low |
| `INT`          | PE3, active low |
| `RST`          | board NRST      |

`RST` is recommended: it resets the W5500 whenever the target MCU resets. If
the module already has a reliable reset pull-up, it can be left disconnected;
the Linux driver also issues a W5500 software reset during probe. The DTS uses
a conservative 1 MHz maximum SPI clock. Keep shared SPI wiring short (ideally
under 10 cm), route a ground wire beside it, and do not use 5 V level signals.

The connector numbers and power pins come from ST's UM1670 board manual. The
SPI5 alternate functions are PF7/PF8/PF9 in the STM32F429 datasheet.

## Optional CY15B256Q FRAM wiring

Select **Find My Device options → Enable CY15B256Q SPI-FRAM** to add the Linux
AT25/NVMEM driver and the SPI5 FRAM node. Power the board off before connecting
or disconnecting the device. CY15B256Q signals are 3.3 V only.

| CY15B256Q label | SOIC8 pin | STM32 signal     |
|-----------------|-----------|------------------|
| `SCK`           | 6         | PF7 / SPI5_SCK   |
| `SO` / `MISO`   | 2         | PF8 / SPI5_MISO  |
| `SI` / `MOSI`   | 5         | PF9 / SPI5_MOSI  |
| `CS#`           | 1         | PG2, active low  |
| `WP#`           | 3         | pull up to 3.3 V |
| `HOLD#`         | 7         | pull up to 3.3 V |
| `VDD`           | 8         | 3.3 V            |
| `VSS` / `GND`   | 4         | ground           |

Add approximately 10 kΩ pull-ups from `CS#`, `WP#`, and `HOLD#` to 3.3 V so
the device remains deselected and writable while MCU pins are being configured.
Place a 100 nF ceramic decoupling capacitor close to a bare FRAM IC's VDD/VSS
pins. Breakout boards may already provide these parts; check their schematic.
The device-tree limit is 10 MHz.

The alternative W25Q128FV reuses the same SPI5 signals and PG2 chip select.
Select **Enable W25Q128FV SPI-NOR** under Find My Device to enable two raw 4 KiB
A/B state sectors without a filesystem. The option does not require the
standalone SPI-NOR package. If that package is also selected, the rest of the
chip becomes the JFFS2 volume documented in
[`package/spinor/README.md`](../spinor/README.md). CY15B256Q and W25Q128FV
cannot be selected or wired simultaneously because they share PG2/CS4.

## Optional 24LC16B EEPROM wiring

Select **Find My Device options → Enable 24LC16B I2C EEPROM** to add the
Linux `at24`/NVMEM driver and a 2 KiB EEPROM on the existing 100 kHz I2C3 bus.
Power the Discovery board off before connecting or disconnecting it. Use the
3.3 V `24LC16B` variant; do not apply 5 V to its supply or signal pins when it
is connected directly to the STM32.

| 24LC16B label    | SOIC8 pin | STM32 signal        | Discovery connector |
|------------------|-----------|---------------------|---------------------|
| `SCL`            | 6         | PA8 / I2C3_SCL      | P1 pin 53           |
| `SDA`            | 5         | PC9 / I2C3_SDA      | P1 pin 54           |
| `WP`             | 7         | ground for write access | P2 pin 11 or pin 29 |
| `A0`, `A1`, `A2` | 1, 2, 3   | not used by 24LC16B | leave open or tie to a known level |
| `VCC`            | 8         | 3.3 V               | P2 pin 1 or pin 2   |
| `VSS` / `GND`    | 4         | ground              | P2 pin 11 or pin 29 |

SDA and SCL are open-drain signals and require pull-ups to 3.3 V. For the
configured 100 kHz bus, 10 kΩ is a typical starting value. Many EEPROM
modules already contain pull-ups; check the module schematic before adding
another pair because parallel pull-ups may become too strong. Place a 100 nF
ceramic decoupling capacitor close to a bare IC's VCC/VSS pins.

Unlike many I2C EEPROMs, the 24LC16B uses the address bits to select internal
memory blocks. One device therefore occupies the complete `0x50` through
`0x57` address range regardless of how A0/A1/A2 are wired. Do not attach
another device using any of those addresses. The [Microchip 24LC16B data
sheet](https://www.microchip.com/content/dam/mchp/documents/OTH/ProductDocuments/DataSheets/20002213B.pdf)
documents the addressing, 16-byte write pages, write-protect pin, and pull-up
requirements. Connector locations are documented in the [STM32F429I-DISC1
board manual](https://www.st.com/resource/en/user_manual/um1670-discovery-kit-with-stm32f429zi-mcu-stmicroelectronics.pdf).

## Hardware compatibility

The W5500 shares SPI5 clock and data with the correctly tri-stating onboard
gyroscope, LCD controller, optional SPI-NAND, and either FRAM or SPI-NOR. Each device
has a different active-low chip select: gyroscope PC1 (`reg = <0>`), LCD PC2
(`reg = <1>`), W5500 PD5 (`reg = <2>`), SPI-NAND PG3 (`reg = <3>`), and
FRAM/SPI-NOR PG2 (`reg = <4>`). W5500 interrupt remains on PE3. Linux serializes their SPI5
messages, so Display or NAND activity can add network latency, but chip
selection remains electrically valid. The small raw-state transfers have negligible
bus impact.

The SD card stays alone on SPI4 and therefore shares no clock, data, or
chip-select signal with W5500 or SPI-NAND. This separation is important for
inexpensive SD adapter boards whose level-shifter circuit keeps MISO driven
even when SD chip select is high; such an adapter cannot safely share one SPI
data bus merely by assigning another chip select.

PD5 is exposed on P2 pin 37 and is unused by the current examples. In
particular, UART5 uses PC12/PD2 and the RS-485 driver-enable line uses adjacent
PD4, so those examples remain compatible. Existing project functions stay on
their current peripherals:

| Existing function | Pins/peripheral left untouched |
|---|---|
| LCD, gyroscope, SPI-NAND, FRAM/SPI-NOR | SPI5 PF7/PF8/PF9 shared; CS PC1/PC2/PG3/PG2, W5500 CS PD5 |
| SD card | Separate SPI4 PE2/PE5/PE6 bus |
| USB USER CDC | PB12/PB14/PB15, OTG HS embedded FS PHY |
| Console | USART1 PA9/PA10 |
| UART examples | UART5 PC12/PD2 and RS-485 DE PD4 |
| I2C example and optional 24LC16B | I2C3 PA8/PC9 shared; EEPROM owns addresses 0x50–0x57 |
| PWM example | PB4 |
| GPIO examples | PG14 (PA0 and PG13 are reserved by this example) |

The EEPROM has no SPI or GPIO conflict and can coexist with FRAM, SPI-NOR,
SPI-NAND, SD card, Display, and W5500. It shares I2C3 with the bus-scanner
example; the kernel `at24` driver claims `0x50`–`0x57`, so a scanner may report
those addresses as busy rather than opening them directly.

The UART examples can run alongside both the display and W5500. Four W5500
DTBs cover the minimal, display, USB, and USB+display selections. Four matching
`-w5500-sdcard` DTBs compose the SPI5 W5500 and SPI4 SD card when standalone SD
support is also selected; UART5 is present in their shared base device tree.

Physical confirmation and the optional status output are configured as follows:

- Press the on-board blue **USER** button on **PA0** for active-high physical
  confirmation. A pull-down is requested through the GPIO character device.
- The on-board green **LD3** LED on **PG13** provides active-high status and
  identification flashes.
- If neither is connected, set `FMD_GPIO_ENABLED=no` in
  `find-my-device.conf`. The optional console debug controls described below
  can emulate confirmation without GPIO hardware.

The W5500 example reserves PA0 and PG13 while it runs. The red PG14 LED remains
available to other examples.

## Build and host tests

Run all commands in the devcontainer (`/workspace`):

```sh
make configure
make build_all
```

The tracked defconfig already selects `BR2_PACKAGE_FIND_MY_DEVICE=y`. The
resulting files are:

```text
buildroot/output/images/xipImage
buildroot/output/images/stm32f429disco-custom-w5500.dtb
```

`flash.sh` automatically chooses the `-w5500.dtb` matching the enabled display
and USB packages. If `BR2_PACKAGE_SDCARD=y` is also selected, it chooses the
matching `-w5500-sdcard.dtb`; the SD package does not enable Find My Device.
Flash normally with `make flash` after ST-LINK is connected.

Run the hardware-independent protocol suite directly in the devcontainer:

```sh
make -C firmware/package/find-my-device/tests clean check integration
```

The live integration test starts the host daemon on loopback and checks public
and protected API calls, PKCE authorization, signal-based physical
confirmation, code replay rejection, rename and OAuth-state restoration,
mobile registration, WebSocket authentication, and status events. Unit tests
cover the mDNS wire codec and inherited device/OAuth logic.

When a kernel or BusyBox fragment changes in an existing output tree, force its
configuration stamp once before rebuilding:

```sh
make -C buildroot BR2_EXTERNAL=/workspace/firmware linux-reconfigure busybox-reconfigure
make build_all
```

## First hardware boot

1. Connect W5500 as listed above, connect Ethernet, and power the Discovery
   board through its usual ST-LINK USB connector.
2. Build and flash. The service starts in the background while the serial
   console remains interactive.
3. Watch the console for `find_my_device_link=up`, a DHCP lease, and:

   ```text
   device_info_ready port=8080 device_id=<16 hex digits>
   mdns_ready service=FINDER-R01-stm32f429-linux-...
   ```

4. From another device on the same IPv4 LAN, browse
   `_device-setup._tcp.local` with Avahi, Bonjour, or the existing onboarding
   client. You can also open `http://<board-ip>:8080/api/info`.
5. When the authorization page asks for physical confirmation, press the
   on-board blue USER button. With console debug controls enabled, you may
   instead run `find-my-device-debug button` on the serial console.

If DHCP receives no lease after four attempts, startup assigns a deterministic
`169.254.x.y/16` address derived from the stable MAC and prints it. Configure
the test computer on the same link-local subnet for a direct cable test.

## Manual debugging checklist

- No `eth0`: inspect the boot console for `w5100`/`spi` probe messages. Verify
  3.3 V at the module, common ground, PD5 CS idle high, PE3 INT idle high, and
  MOSI/MISO not reversed.
- `eth0` but carrier stays down: check the cable, switch, W5500 link LEDs, and
  crystal/oscillator activity. `cat /sys/class/net/eth0/carrier` should become
  `1`.
- Carrier up but no discovery: run `ifconfig eth0`, use the printed IP for
  `/api/info`, and confirm the client firewall allows UDP 5353 multicast to
  224.0.0.251 and TCP 8080.
- Identity failure: verify a `stm32-romem.../nvmem` file exists under
  `/sys/bus/nvmem/devices`. The daemon falls back to the interface MAC only if
  the UID provider is unavailable.
- GPIO warnings: either attach the optional controls, correct the gpiochip/line
  values in `/etc/find-my-device.conf`, or set `FMD_GPIO_ENABLED=no`.

The root filesystem is an initramfs. With Find My Device alone, renames and
OAuth refresh state survive a daemon restart during one boot but not a power
cycle. With `FMD_STATE_FILE=auto`, startup checks persistent storage at runtime
in this order:

1. CY15B256Q FRAM, when its Find My Device option is enabled and it probes;
2. 24LC16B EEPROM, when its Find My Device option is enabled and it probes;
3. the W25Q128FV raw state partition, when its option is enabled;
4. a mounted standalone SD card;
5. the mounted SPI-NOR JFFS2 volume;
6. the initialized SPI-NAND UBIFS volume; and
7. the RAM-backed root filesystem as a safe fallback.

The persistent paths are:

```text
/sys/bus/spi/devices/<SPI5-device>/fram
/sys/bus/nvmem/devices/find-my-device-eeprom*/nvmem
/dev/mtdX                         (label: find-my-device-state)
/mnt/sdcard/find-my-device/state
/mnt/spinor/find-my-device/state
/mnt/spinand/find-my-device/state
```

This is a runtime policy rather than a Kconfig dependency: Find My Device stays
independent of the storage examples and only uses helpers and mounts present
in the running image. Startup prints the chosen path and `backend=fram`,
`backend=eeprom`, `backend=spinor-raw`, `backend=sdcard`, `backend=spinor`,
`backend=spinand`, or `backend=ram`. If no persistent device
or volume is available, Find My Device remains usable with temporary RAM state.
Do not unmount the selected filesystem volume while the daemon is running.

FRAM and EEPROM are raw byte-addressable NVMEM rather than mounted filesystems.
Find My Device reserves the first 512 bytes of either device as two alternating
256-byte records. Each record contains a format version, generation counter,
and CRC32; a new record is read back and verified before it is accepted, while
the previous record remains available after an interrupted write. A shared
record codec implements this policy for FRAM, EEPROM, and raw SPI-NOR, while
the device-specific code handles byte writes or NOR erase sectors. Filesystem
state updates continue to use a temporary file, `fsync`, and atomic rename.

Raw SPI-NOR state uses the same record format but places each record in a
different 4 KiB erase sector. Before writing the inactive record, the daemon
erases only that record's sector, writes 256 bytes synchronously, and reads it
back for verification. The previous block remains valid across an interrupted
erase or program operation. The adjacent JFFS2 partition is independent.

## Configuration

The **Find My Device options** menu directly contains **Enable CY15B256Q
SPI-FRAM**, **Enable 24LC16B I2C EEPROM**, **Enable W25Q128FV SPI-NOR**, and
**Enable console button and LED debug controls**. All three storage options
operate without a filesystem and are disabled by default. EEPROM is
independent and can coexist with either SPI choice. FRAM and SPI-NOR are
mutually exclusive because both use PG2; selecting SPI-FRAM also disables the
standalone SPI-NOR package. The W25Q option is available without the standalone
SPI-NOR/JFFS2 package. The console option adds test controls and diagnostic
messages. When console controls are selected, use:

```sh
find-my-device-debug button  # simulate one complete USER-button tap
find-my-device-debug led     # flash LD3 three times and log LED transitions
find-my-device-debug net     # print eth0 packet and error counters
```

The debug daemon also prints the network counters every five seconds, so a
failure remains observable when the serial connection is receive-only. Pass an
alternate interface as the second argument to `find-my-device-debug net`.
`find-my-device-confirm` remains available as a compatibility alias only when
the same debug option is enabled. All debug-only handlers and periodic reports
are omitted when the option is disabled.

Edit `find-my-device.conf` in this package and rebuild to change the interface,
port, initial name/model, state paths, storage waits, or optional GPIOs. The
default `FMD_STATE_FILE=auto` applies the FRAM → EEPROM → raw SPI-NOR → SD
→ SPI-NOR/JFFS2 → SPI-NAND/UBIFS → RAM runtime policy.
`FMD_FRAM_STATE_DEVICE=auto` discovers the SPI driver's `fram` sysfs file, and
`FMD_EEPROM_STATE_DEVICE=auto` discovers the labelled NVMEM device. Set either
explicitly only if the system has multiple matching devices. Set an explicit
`FMD_SPINOR_RAW_DEVICE` only when automatic discovery by MTD partition label is
unsuitable. Set an explicit absolute state path to override automatic
selection. Files under `/etc` on a running board are RAM-backed and reset to
the built image on reboot.
