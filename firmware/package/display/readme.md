# Display

`BR2_PACKAGE_DISPLAY` enables the STM32F429Discovery LCD and installs a
framebuffer slideshow with bundled PNG, JPEG, and GIF images. BMP support is
also available through `fbv`.

Gzip initramfs compression and boot autostart are enabled by default. The
service starts the slideshow from `/usr/share/display` without taking over the
serial console.

## Commands

```sh
display [--autoplay] [delay-seconds] [image-directory]
display-pattern [pattern] [write|mmap]
/etc/init.d/S30display {start|stop|restart|status}
```

`display-pattern` accepts `bars`, `shapes`, `checker`, `random`, `white`,
`red`, `green`, `blue`, or `black`. Display cannot be selected with Gallery or
Screen because those packages own the same framebuffer application paths.

The LCD is onboard and requires no external wiring. See
[the package overview](../readme.md) for the common build and flash procedure.
