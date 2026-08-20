# c-periphery library

`periphery` is a hidden dependency used by selected `hwtools` applets. It
builds the local c-periphery sources as `libperiphery.a` and installs the
library and headers into Buildroot's staging directory.

It provides GPIO, I2C, SPI, PWM, serial, MMIO, LED, and version APIs. The build
detects the GPIO character-device interfaces available in the target kernel
headers.

This package installs no target command and does not produce a standalone
example image. Static-library objects are copied into `hw` only when a selected
applet references them.
