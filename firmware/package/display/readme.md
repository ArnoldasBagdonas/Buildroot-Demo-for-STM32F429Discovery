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

## Why `fbv` is patched

Display and Gallery share three patches under
[`board/stm32f429disco/patches/fbv`](../../board/stm32f429disco/patches/fbv/).
They correct application behavior; they are not STM32 silicon errata.

1. **Serial input ownership.** Upstream `fbv` polls file descriptor 0 during a
   timed slideshow. When init starts it on the same console as the shell, it
   consumes shell keystrokes. The `--noinput` patch uses a timer-only wait and
   skips terminal setup, leaving the serial console exclusively to the shell.
   See
   [`0001-add-noninteractive-slideshow-mode.patch`](../../board/stm32f429disco/patches/fbv/0001-add-noninteractive-slideshow-mode.patch).
2. **Peak JPEG memory.** Upstream `fbv` decodes a full-resolution JPEG before
   resizing it. A multi-megapixel RGB image can exhaust this no-MMU target even
   though the LCD is only 240 x 320. The patch requests libjpeg's native 1/2,
   1/4, or 1/8 IDCT reduction first, then performs the exact final fit. See
   [`0002-downsample-large-jpegs-during-decode.patch`](../../board/stm32f429disco/patches/fbv/0002-downsample-large-jpegs-during-decode.patch).
3. **Stale border pixels.** `fbv` overwrites only the new image rectangle, so a
   portrait slide following a landscape slide retains pixels outside the new
   rectangle. Clearing the mapped framebuffer before every blit produces clean
   letterbox or pillarbox bars without clearing the serial terminal. See
   [`0003-clear-framebuffer-before-each-image.patch`](../../board/stm32f429disco/patches/fbv/0003-clear-framebuffer-before-each-image.patch).

The [libjpeg-turbo documentation](https://github.com/libjpeg-turbo/libjpeg-turbo/blob/main/doc/usage.txt)
documents decompression scaling for images larger than the screen, and the
[STM32F429I-DISC1 board manual, UM1670](https://www.st.com/resource/en/user_manual/um1670-discovery-kit-with-stm32f429zi-mcu-stmicroelectronics.pdf)
specifies the onboard LCD as QVGA, 240 x 320. The console-input and stale-pixel
failures follow directly from `fbv` 1.0b's source and were reproduced by the
boot slideshow; no manufacturer manual applies to those two application bugs.
