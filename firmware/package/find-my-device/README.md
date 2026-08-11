# Find My Device package (W5500 + mDNS)

This external Buildroot package is the Linux/STM32F429 counterpart of the
FreeRTOS `find-my-device` project next to this repository. It provides:

- an in-kernel W5500 Ethernet interface on SPI4;
- DHCP with deterministic IPv4 link-local fallback;
- a stable 64-bit device ID and locally administered MAC derived from the
  STM32 factory UID;
- `_device-setup._tcp.local` mDNS announcements and query responses;
- API v3 device information, OAuth authorization-code + PKCE, physical
  confirmation, refresh-token rotation, renaming, mobile registration, and
  authenticated WebSocket status events;
- optional external confirmation button and status LED controls.

## W5500 wiring

Power the board off before wiring. Use the W5500 module's **3.3 V input** and
3.3 V logic. Do not connect a bare W5500 or a 3.3 V-only breakout to 5 V.

| W5500 label | STM32 signal | Discovery connector |
|---|---|---|
| `SCLK` / `SCK` | PE2 / SPI4_SCK | P1 pin 15 |
| `MISO` | PE5 / SPI4_MISO | P1 pin 14 |
| `MOSI` | PE6 / SPI4_MOSI | P1 pin 11 |
| `SCS` / `CS` / `nSS` | PE4, active low | P1 pin 13 |
| `INT` / `nINT` | PE3, active low | P1 pin 16 |
| `RST` / `nRST` | board NRST | P2 pin 12 |
| `3V3` / `VCC` | 3 V rail | P2 pin 1 or pin 2 |
| `GND` | ground | P2 pin 11 or pin 29 |

`RST` is recommended: it resets the W5500 whenever the target MCU resets. If
the module already has a reliable reset pull-up, it can be left disconnected;
the Linux driver also issues a W5500 software reset during probe. The DTS uses
12 MHz SPI to leave margin for jumper wires. Keep SPI wiring short (ideally
under 10 cm), route a ground wire beside it, and do not use 5 V level signals.

The connector numbers and power pins come from ST's UM1670 board manual. The
SPI4 alternate functions are PE2/PE5/PE6 in the STM32F429 datasheet.

## Why these pins do not conflict

The W5500 uses only PE2 through PE6 on SPI4. Existing project functions remain
on their current peripherals:

| Existing function | Pins/peripheral left untouched |
|---|---|
| LCD and gyroscope | SPI5 PF7/PF8/PF9, CS PC1/PC2, LTDC pins |
| USB USER CDC | PB12/PB14/PB15, OTG HS embedded FS PHY |
| Console | USART1 PA9/PA10 |
| UART examples | UART5 PC12/PD2 and RS-485 DE PD4 |
| I2C example | I2C3 PA8/PC9 |
| PWM example | PB4 |
| GPIO examples | PA0, PG13, PG14 |

The UART examples now use PC12/PD2, so they can run alongside both the display
and W5500. Four W5500 DTBs cover the minimal, display, USB, and USB+display
selections; UART5 is present in their shared base device tree.

Optional controls also avoid existing examples:

- Connect a normally-open button between **PC13 (P1 pin 12)** and GND for
  active-low physical confirmation. The internal pull-up is enabled.
- Connect **PG9 (P1 pin 33)** through a 680 ohm to 1 kohm resistor to an LED
  anode, with its cathode to GND, for active-high status indication.
- If neither is connected, set `FMD_GPIO_ENABLED=no` in
  `find-my-device.conf`. During an authorization request, run
  `find-my-device-confirm` at the board console to emulate the button.

Do not substitute the on-board USER button or green/red LEDs: those are kept
available for the existing GPIO examples.

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
and USB packages. Flash normally with `make flash` after ST-LINK is connected.

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
   external PC13 button or run `find-my-device-confirm` on the serial console.

If DHCP receives no lease after four attempts, startup assigns a deterministic
`169.254.x.y/16` address derived from the stable MAC and prints it. Configure
the test computer on the same link-local subnet for a direct cable test.

## Manual debugging checklist

- No `eth0`: inspect the boot console for `w5100`/`spi` probe messages. Verify
  3.3 V at the module, common ground, PE4 CS idle high, PE3 INT idle high, and
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

The root filesystem is an initramfs. Renames and OAuth refresh state survive a
daemon restart during one boot, but not a power cycle. True power-cycle
persistence requires a writable storage backend (for example external flash)
that this board configuration does not currently provide; reserving and
programming internal XIP flash from Linux was intentionally avoided until it
can be validated on hardware.

## Configuration

Edit `find-my-device.conf` in this package and rebuild to change the interface,
port, initial name/model, state path, or optional GPIOs. Files under `/etc` on a
running board are RAM-backed and reset to the built image on reboot.
