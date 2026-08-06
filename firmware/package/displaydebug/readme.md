# Reusable Debug Package

`BR2_PACKAGE_DISPLAYDEBUG` is independent of every example. Select it
temporarily beside any example that needs investigation, then disable it for
the smaller normal image.

It installs `/usr/bin/displaydebug` and conditionally enables:

- the kernel's general debugging switch, procfs, and debugfs;
- timestamped kernel logging and a 16 KiB kernel log buffer;
- BusyBox `dmesg`, `grep`, `find`, `readlink`, `devmem`, and `dd`.

The command reports system information, memory, interrupts, device nodes, the
clock tree, platform devices, and kernel messages. If the STM32F429 LTDC driver
is bound, it additionally reports the LCD clocks, RCC registers, LTDC timing
and scan position, and layer registers. Display register reads are skipped in
all other images.

## Build and run

From the repository root in the devcontainer:

```bash
make distclean
make configure
make menuconfig
```

Select the example being tested and also select `BR2_PACKAGE_DISPLAYDEBUG`.
Save, exit, build, and flash:

```bash
make build_all
make flash
```

On the serial console, run:

```sh
displaydebug
```

For LCD diagnosis, activate the display first so the report can observe live
clocks, registers, and interrupts:

```sh
fbpattern bars write
displaydebug
```

After diagnosis, repeat the clean configuration without
`BR2_PACKAGE_DISPLAYDEBUG`. Its kernel fragment, BusyBox fragment, and command
will then be absent from the image.
