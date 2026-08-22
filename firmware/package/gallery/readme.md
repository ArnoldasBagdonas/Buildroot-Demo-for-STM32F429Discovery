# Gallery

`BR2_PACKAGE_GALLERY` combines the Display slideshow applets in one multicall
executable. The installed `gallery`, `display`, and `display-pattern` commands
all dispatch through `/usr/bin/gallery`.

The default image uses bundled PNG, JPEG, and GIF files, enables gzip initramfs
compression, and starts the slideshow at boot. Optional built-in SD-card
support adds the `sdcard` applet and can use images from `/mnt/sdcard`, falling
back to `/usr/share/display` when none are available.

## Commands

```sh
gallery [--autoplay] [delay-seconds] [image-directory]
display [--autoplay] [delay-seconds] [image-directory]
display-pattern [pattern] [write|mmap]
sdcard {status|mount|unmount|watch}   # optional
/etc/init.d/S30gallery {start|stop|restart|status}
```

Gallery replaces the standalone Display package. Its integrated SD option
also replaces the standalone SD-card command so that each installed path has
one owner. See [the package overview](../readme.md) for common build steps.

## Why `fbv` is patched

Gallery uses the same patched `fbv` 1.0b viewer as Display:

| Issue and root cause | Solution |
|---|---|
| A timed upstream slideshow polls stdin and competes with the serial shell. | `--noinput` waits only for the slide timer and does not configure or read the console. |
| Full-resolution JPEG decode can exhaust no-MMU memory before a large image is resized for the 240 x 320 LCD. | Select libjpeg 1/2, 1/4, or 1/8 decode scaling before allocating the output image. |
| Pixels outside a new image's rectangle retain the previous slide. | Clear the mapped framebuffer before each blit. |

These are application fixes, not MCU errata. See
[Display's patch rationale](../display/readme.md#why-fbv-is-patched) for the
patch directory and primary documentation.
