# USB CDC ACM data-port example

This package keeps the Linux console and interactive shell on USART1
(`/dev/ttySTM0`). Independently, the board's USB USER connector enumerates as
a CDC ACM data port:

- Board side: `/dev/ttyGS0`
- Linux host side: normally `/dev/ttyACM0`

The USB port is deliberately not registered as a kernel console. This avoids
mixing buffered kernel messages with application data and lets programs use
the link as an ordinary bidirectional byte stream.

Enable `BR2_PACKAGE_USBSERIALDEVICE` in `make menuconfig`, then build and
flash:

```sh
# Required when reusing an output tree whose kernel was already configured.
make -C buildroot BR2_EXTERNAL=/workspace/firmware linux-reconfigure
make build_all
make flash
```

The explicit reconfigure is unnecessary for a clean output tree. Connect the
Cypress adapter to USART1 for the Linux console and connect the USB USER
micro-AB socket for the CDC data link. The ST-LINK socket is used for flashing
and board power; it is not the CDC data port.

Use stable host names to distinguish the Cypress UART adapter from the Linux
USB gadget:

```sh
ls -l /dev/serial/by-id/
```

## Automated loopback test

On the USART1 Linux console, start the included program:

```sh
usbserialchat
```

It opens `/dev/ttyGS0` in raw mode and echoes every received byte back to the
host. On the host, open the Linux gadget device (not the Cypress device):

```sh
picocom -b 115200 /dev/serial/by-id/usb-Linux_*Gadget_Serial*-if00
```

Text typed in picocom should be echoed by the STM32 and logged as `USB RX ...`
on the USART1 console. Picocom uses no local echo by default, so visible text
confirms a full host-to-board-to-host round trip. Exit picocom with `Ctrl-A`,
then `Ctrl-X`. Stop `usbserialchat` with `Ctrl-C` on the USART1 console, or
type `q` followed by Enter as a fallback.

The baud-rate argument is accepted for terminal compatibility; USB CDC ACM
does not use a physical UART baud clock.

## Manual one-way tests

Board to host: first run this on the host:

```sh
cat /dev/serial/by-id/usb-Linux_*Gadget_Serial*-if00
```

Then send data from the USART1 console:

```sh
echo 'hello from STM32' > /dev/ttyGS0
```

Host to board: first run this on the USART1 console:

```sh
cat /dev/ttyGS0
```

Then send data from another host terminal:

```sh
printf 'hello from host\n' > /dev/serial/by-id/usb-Linux_*Gadget_Serial*-if00
```

## Two-terminal chat

On the USART1 console, start a background receiver and forward console input
to USB:

```sh
cat /dev/ttyGS0 &
usb_rx_pid=$!
cat > /dev/ttyGS0
```

Open picocom on the host gadget port. Text entered in the USART1 terminal is
sent to picocom, while text entered in picocom appears in the USART1 terminal.
Press `Ctrl-C` on USART1 to stop the foreground `cat`, then stop its background
receiver:

```sh
kill "$usb_rx_pid"
```

## Hardware details

The USB USER socket is wired to the OTG HS controller's internal full-speed
PHY on PB12/PB14/PB15. The device tree therefore enables `usbotg_hs` with the
STM32 full-speed-compatible setup rather than the separate `usbotg_fs`
controller on PA10/PA11/PA12.

The package applies two Linux 6.1 DWC2 fixes. The cold-boot patch selects the
STM32 internal full-speed PHY before resetting the controller, waits for AHB
idle, enables the transceiver, and disables hardware VBUS sensing. A second
STM32F4-specific patch uses PIO and completes OUT requests from the endpoint
transfer-complete interrupt; the controller otherwise reports FIFO OUT-done
one transaction late. The board's AFBOOT patch also leaves the unwired OTG HS
ULPI clock disabled.
