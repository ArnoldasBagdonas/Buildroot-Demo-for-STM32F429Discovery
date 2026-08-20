# USB CDC ACM

`BR2_PACKAGE_USB_CDC` configures the board's USB USER connector as an
independent CDC ACM data port. USART1 remains the Linux console.

| Side | Device |
|---|---|
| STM32 target | `/dev/ttyGS0` |
| Linux host | Usually `/dev/ttyACM0` or `/dev/serial/by-id/usb-Linux_*` |

Start the included echo example on the target:

```sh
usb-cdc [device]
```

The device argument defaults to `/dev/ttyGS0`. Connect the host to the USB USER
micro-AB socket; the ST-LINK socket is used for power, flashing, and debugging
and is not this CDC port.

The USB USER socket uses the OTG HS controller with its internal full-speed PHY
on PB12, PB14, and PB15. The package applies the required DWC2 STM32F4 fixes and
uses PIO transfers for the gadget connection.

For a manual loopback test, run `usb-cdc` on the target and open the host gadget
device with `picocom -b 115200 DEVICE`. Text returned to the host has travelled
through the complete USB path.
