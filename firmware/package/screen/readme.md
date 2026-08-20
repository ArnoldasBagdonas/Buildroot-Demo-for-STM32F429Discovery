# Screen touch console

`BR2_PACKAGE_FIRMWARE_SCREEN` turns the onboard LCD and STMPE811 resistive
touchscreen into a small terminal with an on-screen keyboard. It opens
`/dev/fb0`, reads touchscreen events, creates a private pseudo-terminal, and
runs an interactive BusyBox shell.

Gzip initramfs compression and boot autostart are enabled by default. USART1
remains available as the recovery and diagnostic console.

## Commands

```sh
screen
/etc/init.d/S30screen {start|stop|restart|status}
```

The package is unrelated to GNU Screen. Kconfig prevents both packages from
owning `/usr/bin/screen`. It also cannot be selected with Display or Gallery
because each application requires exclusive use of the framebuffer.

The LCD and touchscreen are onboard and require no external wiring. See
[the package overview](../readme.md) for common build steps.
