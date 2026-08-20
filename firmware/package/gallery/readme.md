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
