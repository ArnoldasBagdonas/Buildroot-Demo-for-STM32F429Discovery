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
- on-board USER-button confirmation and green status-LED controls.

## W5500 wiring

Power the board off before wiring and keep every signal at 3.3 V logic. A bare
W5500 or 3.3 V-only breakout must use the board's 3 V rail. A module with an
onboard 3.3 V regulator may instead require 5 V on its separately labelled
`5V` or `VIN` input; follow that module's schematic and never apply 5 V to a
signal pin.

| W5500 label | STM32 signal | Discovery connector |
|---|---|---|
| `SCLK` / `SCK` | PF7 / SPI5_SCK | P2 pin 6 |
| `MISO` | PF8 / SPI5_MISO | P2 pin 5 |
| `MOSI` | PF9 / SPI5_MOSI | P2 pin 8 |
| `SCS` / `CS` / `nSS` | PD5, active low | P2 pin 37 |
| `INT` / `nINT` | PE3, active low | P1 pin 16 |
| `RST` / `nRST` | board NRST | P2 pin 12 |
| `3V3` / `VCC` | 3 V rail | P2 pin 1 or pin 2 |
| `GND` | ground | P2 pin 11 or pin 29 |

`RST` is recommended: it resets the W5500 whenever the target MCU resets. If
the module already has a reliable reset pull-up, it can be left disconnected;
the Linux driver also issues a W5500 software reset during probe. The DTS uses
a conservative 1 MHz maximum SPI clock. Keep shared SPI wiring short (ideally
under 10 cm), route a ground wire beside it, and do not use 5 V level signals.

The connector numbers and power pins come from ST's UM1670 board manual. The
SPI5 alternate functions are PF7/PF8/PF9 in the STM32F429 datasheet.

## Hardware compatibility

The W5500 shares SPI5 clock and data with the correctly tri-stating onboard
gyroscope, LCD controller, and optional SPI-NAND. Each device has a different
active-low chip select: gyroscope PC1 (`reg = <0>`), LCD PC2 (`reg = <1>`),
W5500 PD5 (`reg = <2>`), and SPI-NAND PG3 (`reg = <3>`). W5500 interrupt
remains on PE3. Linux serializes their SPI5 messages, so Display or NAND
activity can add network latency, but chip selection remains electrically
valid.

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
| LCD, gyroscope, SPI-NAND | SPI5 PF7/PF8/PF9 shared; CS PC1/PC2/PG3, W5500 CS PD5 |
| SD card | Separate SPI4 PE2/PE5/PE6 bus |
| USB USER CDC | PB12/PB14/PB15, OTG HS embedded FS PHY |
| Console | USART1 PA9/PA10 |
| UART examples | UART5 PC12/PD2 and RS-485 DE PD4 |
| I2C example | I2C3 PA8/PC9 |
| PWM example | PB4 |
| GPIO examples | PG14 (PA0 and PG13 are reserved by this example) |

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
cycle. When standalone `BR2_PACKAGE_SDCARD=y` is also selected, Find My Device
explicitly requests a card mount during its own startup, waits up to ten
seconds, and stores state at:

```text
/mnt/sdcard/find-my-device/state
```

Startup prints the chosen path and `backend=sdcard` or `backend=ram`. If the
card is missing or cannot be mounted, Find My Device remains usable with
temporary RAM state. Do not unmount or remove the card while the daemon is
running. State updates are written through a temporary file, synchronized,
and renamed into place before success is reported.

## Configuration

The **Find My Device options** menu contains **Enable console button and LED
debug controls**. It is disabled by default so production images do not carry
test controls or their console messages. When selected, use:

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
port, initial name/model, state path, SD mount wait, or optional GPIOs. The
default `FMD_STATE_FILE=auto` selects SD-backed state only when the standalone
SD helper is installed and `/mnt/sdcard` is mounted. Set an explicit absolute
path to override that behavior. Files under `/etc` on a running board are
RAM-backed and reset to the built image on reboot.
