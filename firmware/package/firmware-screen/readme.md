# Screen Buildroot integration

This directory contains the Buildroot package makefile for the Screen example.
The selectable symbol is `BR2_PACKAGE_FIRMWARE_SCREEN`; its source code,
license, service script, and user documentation are in
[`../screen`](../screen/).

The separate directory avoids a name collision with Buildroot's GNU Screen
package while still installing the target command as `/usr/bin/screen`. It is
an internal integration directory, not an additional example or target
command.
