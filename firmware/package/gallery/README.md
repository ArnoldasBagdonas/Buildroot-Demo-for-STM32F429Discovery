# Gallery composite example

`BR2_PACKAGE_GALLERY` provides the LCD slideshow as one BusyBox-style
executable. By default it includes the Display and compact-image kernel
fragments, `fbv` and its image decoders, embedded test images, and the display
device tree. It has no SD-card runtime or storage-stack dependency in this
basic configuration.

The default-off `BR2_PACKAGE_GALLERY_SDCARD` suboption demonstrates optional
composition. It links the SD automounter into the same executable, enables the
MMC-over-SPI/FAT stack and gzip initramfs compression, and leaves the slideshow
on embedded images. Its nested `BR2_PACKAGE_GALLERY_SDCARD_IMAGES` option
separately enables runtime selection between `/mnt/sdcard` and embedded
`/usr/share/display` images.

## Package ownership

Gallery owns one regular file and several symbolic links:

```text
/usr/bin/gallery
/usr/bin/display         -> gallery
/usr/bin/display-pattern -> gallery
/usr/bin/display-auto    -> gallery  # when autostart is enabled
/usr/bin/gallery-auto    -> gallery  # when autostart is enabled
/usr/sbin/sdcard         -> ../bin/gallery  # when SD support is enabled
```

A normal command invocation supplies the invoked link name in `argv[0]`.
`gallery.c` takes its basename and dispatches to the matching applet entry
point, which is the same basic design used by BusyBox.

[`gallery.mk`](gallery.mk) always compiles `gallery.c`, `display.c`, and
`display-pattern.c` in one link operation. The SD suboption adds `sdcard.c` to
that same link command. `DISPLAY_MULTICALL`, and conditionally
`SDCARD_MULTICALL`, suppress the module-local `main()` functions so Gallery's
dispatcher remains the only entry point. No runtime shared library or second
statically linked C runtime is installed.

The source modules expose normal C entry points:

```c
int display_applet_main(int argc, char **argv);
int display_auto_applet_main(int argc, char **argv);
int display_pattern_main(int argc, char **argv);
int sdcard_main(int argc, char **argv); /* SD suboption */
```

Display also exposes a source-provider callback through
[`display.h`](../display/display.h). Its standalone executable leaves the
callback unset and therefore uses only embedded images. Gallery also leaves it
unset by default. The nested SD-image option registers a provider that checks
the live `/proc/mounts` state and offers the SD-card root; the Display applet
accepts it only when it contains supported images. This keeps all knowledge of
SD-card mount policy out of the Display package.

## Configuration pattern

Gallery and standalone Display install the same command names, so
`BR2_PACKAGE_DISPLAY` depends on `!BR2_PACKAGE_GALLERY`. Standalone SD card may
coexist with Gallery while built-in SD support is off; that arrangement has
two executables and two independent package owners. Enabling
`BR2_PACKAGE_GALLERY_SDCARD` makes standalone SD unavailable and keeps
`/usr/sbin/sdcard` owned by Gallery's single multicall executable.

Gallery directly selects `fbv` and its PNG, JPEG, and GIF support rather than
selecting the standalone Display or SD-card packages. Its makefile likewise
declares `fbv` as a build dependency. This distinction teaches two separate
Buildroot responsibilities: Kconfig controls the feature selection presented
to the user, while the package dependency establishes build order.

Selecting Gallery reveals a `Gallery options` menu containing:

- `BR2_PACKAGE_GALLERY_AUTOSTART` defaults to enabled and installs the
  `display-auto` and descriptive `gallery-auto` aliases.
- `BR2_PACKAGE_GALLERY_SDCARD` defaults to disabled and adds the `sdcard`
  applet, automount support, storage kernel fragment, gzip initramfs
  compression, and combined device tree.

Selecting built-in SD support reveals one more option at the same menu level:

- `BR2_PACKAGE_GALLERY_SDCARD_IMAGES` defaults to disabled and enables runtime
  source selection between `/mnt/sdcard` and embedded images.

The menu hierarchy is therefore:

```text
[*] Gallery: PNG/JPEG/GIF/BMP slideshow
    Gallery options  --->
        [*] Start the gallery automatically at boot
        [ ] Build SD-card support into the Gallery executable
        [ ] Use images from an automatically mounted SD card  # appears when selected
```

The shared `linux-compact.config` contains size-oriented feature and timer
settings, but does not choose an initramfs compression format. Basic Gallery
therefore keeps the initramfs uncompressed. Enabling built-in SD support also
applies `linux-initramfs-gzip.config`, which stores the embedded root filesystem
as a gzip archive to recover flash headroom for the storage stack. The kernel
remains XIP: kernel text executes from flash and Linux expands only the root
filesystem into RAM during boot. Networking has its own conditional fragment
so Find My Device remains compatible with embedded-image Gallery. MMC, block,
partition, FAT, and NLS support remain in the separate `linux-sdcard.config`
fragment and enter Gallery only through the SD suboption.

With built-in SD support, the minimal init script recognizes Gallery's
`/usr/sbin/sdcard` symlink and starts `sdcard watch`. When Gallery autostart is
also enabled, it starts `display-auto` afterward. They are separate processes
even though both names resolve to the same executable. Gallery keeps its
existing periodic watcher policy; the standalone SD-card package owns the
configurable boot-mount options.

## Runtime behavior

Without built-in SD support, every automatic and interactive slideshow uses
`/usr/share/display`. With built-in support, the SD watcher operates once per
second and mounts the first partition, or the whole card when no partition
block device exists, as VFAT at `/mnt/sdcard`.

The slideshow still uses embedded images unless
`BR2_PACKAGE_GALLERY_SDCARD_IMAGES` is selected. With that policy enabled, it
selects its source before each complete pass:

1. `/mnt/sdcard` when it is mounted and contains a supported root-level image;
2. `/usr/share/display` otherwise.

Supported extensions are PNG, JPEG, GIF, and BMP, matched without regard to
case. Directories are not searched recursively. A card inserted during a pass
is selected when the next pass begins. Use `sdcard unmount` before removal;
removing and reinserting the card re-enables automount.

Images are fitted within the 240x320 framebuffer while preserving aspect
ratio. Unused letterbox or pillarbox space is cleared to black before every
slide. Large JPEGs use libjpeg's decoder scaling before the exact framebuffer
fit to reduce peak memory use. See the
[`Display documentation`](../display/readme.md) for the renderer details and
project fbv patches.

Useful commands are:

```sh
gallery
gallery-auto status
gallery-auto stop
gallery-auto start 5
# With BR2_PACKAGE_GALLERY_SDCARD:
sdcard status
sdcard unmount
display-pattern shapes mmap
```

`gallery` and `display` are aliases for the same interactive slideshow applet.
Starting an interactive slideshow stops the background supervisor first.

## Build and flash

From the devcontainer:

```sh
cd /workspace
make distclean
make configure
make menuconfig
```

Enable Gallery and enter its `Gallery options` menu. Autostart is enabled by
default, while built-in SD support and SD-card image selection are disabled:

```text
BR2_PACKAGE_GALLERY
# BR2_PACKAGE_GALLERY_SDCARD is not set
# BR2_PACKAGE_GALLERY_SDCARD_IMAGES is not set
```

Enable `BR2_PACKAGE_GALLERY_SDCARD` to build the automounter into Gallery; this
automatically disables standalone `BR2_PACKAGE_SDCARD`. Enable the newly
visible `BR2_PACKAGE_GALLERY_SDCARD_IMAGES` option only when the slideshow
should prefer card-backed images. Build and inspect the selected variant:

```sh
make build_all
grep -E '^BR2_PACKAGE_(GALLERY|DISPLAY|SDCARD)' buildroot/.config
ls -l buildroot/output/target/usr/bin/gallery \
      buildroot/output/target/usr/bin/display
test ! -e buildroot/output/target/usr/sbin/sdcard  # default variant
stat -c 'xipImage size: %s bytes' buildroot/output/images/xipImage
make flash
```

`make flash` selects `stm32f429disco-display.dtb` by default and
`stm32f429disco-display-sdcard.dtb` when the SD suboption is enabled. Matching
USB-serial variants are selected when that independent package is enabled. The
flash script refuses an `xipImage` larger than 2,048,000 bytes.

With the SD suboption, the combined hardware uses the LCD on LTDC/SPI5 and the
SD adapter on SPI4 PE2/PE4/PE5/PE6. Wiring, voltage, filesystem preparation,
and safe-removal details are in the
[`SD-card documentation`](../sdcard/README.md).
