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
