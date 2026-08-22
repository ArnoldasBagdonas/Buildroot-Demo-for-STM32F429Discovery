# Networking

`BR2_PACKAGE_NETWORKING` enables the Linux and BusyBox networking stacks. Its
W5500 Ethernet-controller option is enabled by default and is selected
automatically by `find-me`.

The package installs no dedicated example executable. Use the standard BusyBox
commands:

```sh
ifconfig -a
route -n
udhcpc -i eth0
```

The W5500 shares SPI5 SCK/MISO/MOSI on PF7/PF8/PF9 with other SPI5 devices. It
uses PD5 as its active-low chip select and PE3 as its interrupt input. The SD
card remains on the separate SPI4 controller.

Keep every signal at 3.3 V logic and connect the W5500 reset input to board
NRST when the module permits it.

## Why the W5500 driver is patched

Linux 6.1.27 carries the first three fixes below; Linux 6.6.151 carries all
four. The patches change only the W5500 path in the W5100-family driver.

1. **PHY carrier polling:** the SPI device-tree binding supplies no legacy
   link GPIO, so the unmodified driver reports carrier permanently up. The
   patch reads `PHYCFGR.LNK` and keeps netdev and ethtool carrier state aligned
   with the PHY. See
   [`0001-net-wiznet-w5100-poll-w5500-phy-link.patch`](../../board/stm32f429disco/linux-patches/linux-6.6.151/0001-net-wiznet-w5100-poll-w5500-phy-link.patch).
2. **TX completion ownership:** RX processing temporarily masks the W5500
   socket interrupt output. A `SEND_OK` occurring in that interval did not
   wake the stopped Linux TX queue, which caused lost DHCP REQUEST packets and
   a watchdog restart. The patch routes receive events to `INTn`, makes the
   ordered SPI worker clear, poll, and acknowledge `SEND_OK`, and restarts on a
   bounded timeout. See
   [`0002-net-wiznet-w5100-poll-w5500-tx-completion.patch`](../../board/stm32f429disco/linux-patches/linux-6.6.151/0002-net-wiznet-w5100-poll-w5500-tx-completion.patch).
3. **Stable receive size:** `Sn_RX_RSR` changes asynchronously. A torn 16-bit
   read produced an apparent 64 KiB MACRAW frame during repeated HTTP traffic,
   advanced the ring incorrectly, and stopped receive while carrier stayed up.
   The patch accepts two equal consecutive reads and rejects a frame length
   outside both the reported data and configured RX buffer. See
   [`0003-net-wiznet-w5100-stabilize-w5500-rx-size.patch`](../../board/stm32f429disco/linux-patches/linux-6.6.151/0003-net-wiznet-w5100-stabilize-w5500-rx-size.patch).
4. **RX safety poll (Linux 6.6):** W5500 `INTn` is level-active-low. If an RX
   condition becomes pending while the socket output is masked, STM32 EXTI may
   see no new falling edge when it is re-enabled. The normal IRQ remains the
   fast path, while the ordered link worker queues the coalescing RX worker
   every 100 ms as bounded recovery. See
   [`0005-net-wiznet-w5100-add-w5500-rx-safety-poll.patch`](../../board/stm32f429disco/linux-patches/linux-6.6.151/0005-net-wiznet-w5100-add-w5500-rx-safety-poll.patch).

The manufacturer's [W5500 datasheet](https://docs.wiznet.io/img/products/w5500/w5500_ds_v109e.pdf)
defines `PHYCFGR.LNK`, requires `Sn_RX_RSR` to be read repeatedly until two
successive values agree, defines `SEND_OK` as completion of `SEND` and as
write-one-to-clear, and documents `Sn_IMR`/`Sn_IR` gating of active-low
`INTn`. Those requirements are the basis for fixes 1--3. Fix 4 is a recovery
for the measured interaction between the documented level signal, interrupt
masking, and the STM32/Linux edge path; WIZnet does not publish it as a W5500
erratum.
