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

## Why the USB patches are required

The three fixes cover distinct stages of the same embedded-PHY integration.

1. **Bootloader clock handoff.** AFBOOT enabled nearly every AHB1 clock,
   including `OTGHSULPIEN`, although the Discovery board has no external ULPI
   PHY or ULPI clock. Linux probes DWC2 before unused clocks are disabled, and
   the first core reset hung with `GRSTCTL_CSFTRST` set. The
   [AFBOOT patch](../../board/stm32f429disco/patches/afboot-stm32/0004-stm32f429-disco-disable-unwired-otghs-ulpi-clock.patch)
   keeps the OTG HS core clock on but clears the unused ULPI clock gate.
2. **PHY selection before reset.** The generic Linux DWC2 probe reset the core
   before STM32-specific parameters selected its embedded FS PHY. On cold boot
   this produced `dwc2_core_reset: HANG! Soft Reset timeout
   GRSTCTL_CSFTRST`. The
   [first Linux patch](0001-usb-dwc2-stm32f4-select-fs-phy-before-reset.patch)
   selects `GUSBCFG.PHYSEL`, waits for AHB idle, resets the core, powers the
   embedded transceiver, and configures peripheral-only VBUS sensing.
3. **Gadget OUT completion.** In the tested PIO path, STM32F429 reported RX FIFO
   OUT data before endpoint transfer-complete but delayed FIFO `OUTDONE` until
   the next host packet. Generic DWC2 waited for `OUTDONE`, so `/dev/ttyGS0`
   received each host write one transaction late and never received a final
   standalone write. The
   [second Linux patch](0002-usb-dwc2-stm32f4-use-pio-for-gadget-transfers.patch)
   copies FIFO data first, completes a non-control OUT request from endpoint
   `XFRC`, and ignores the later stale `OUTDONE`.

The [STM32F429I-DISC1 manual, UM1670](https://www.st.com/resource/en/user_manual/um1670-discovery-kit-with-stm32f429zi-mcu-stmicroelectronics.pdf)
documents that this board uses the OTG HS controller's internal PHY at
full-speed, not an external ULPI PHY. ST's official
[`USB_CoreInit()` implementation](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/master/Src/stm32f4xx_ll_usb.c)
selects the embedded PHY before core reset and then activates its transceiver,
which is the ordering used by fixes 1 and 2. RM0090 documents separate RX FIFO
states for OUT data and OUT transfer completion, supporting the data-before-
completion handling in fix 3. The one-packet-late `OUTDONE` behavior itself is
a locally reproduced STM32/DWC2 integration result, not a published ST
erratum. A manual echo test changed from one-write latency to immediate,
complete echo after these fixes.

For a manual loopback test, run `usb-cdc` on the target and open the host gadget
device with `screen DEVICE 115200`. Text returned to the host has travelled
through the complete USB path. Exit GNU Screen with Ctrl-A, then `k`, then `y`.
