# Linux version change procedure

This procedure upgrades the Linux kernel independently of the project examples.
It produces and tests three general STM32F429 Discovery images in order: the
current 6.1.27 baseline, the fixed 6.6.151 LTS target, and the newest stable
kernel available from kernel.org at build time. Enable and test examples only
after each general image passes its size and boot checks.

Run every build command inside the project devcontainer, with `/workspace` as
the working directory. Use the host only for the serial console.

## Rebuild after changing a kernel patch

Buildroot applies kernel patches only while preparing a newly extracted kernel
source tree. After adding, removing, renaming, reordering, or editing a file in
`firmware/board/stm32f429disco/linux-patches/linux-<version>/`, remove the
existing kernel build directory before rebuilding:

```sh
cd /workspace
make linux-dirclean
make build_all
```

Use the following sequence when the patch application itself should be checked
separately from compilation:

```sh
cd /workspace
make linux-dirclean
make -C buildroot BR2_EXTERNAL=/workspace/firmware linux-patch V=1
make build_all
```

Inspect the `linux-patch` output for failed or rejected hunks. After it
succeeds, `make build_all` continues from the patched source tree and completes
the normal build.

Do not use `make linux-rebuild` for this purpose. It recompiles the existing
kernel source tree but does not extract a clean tree and reapply the modified
patch series. A full project `make distclean` is also unnecessary when only a
kernel patch changed.

## Flash layout and mandatory size limit

The XIP kernel occupies the remainder of the STM32F429's 2 MiB internal flash:

```text
xipImage start: 0x0800c000
flash end:      0x08200000
maximum size:   0x001f4000 = 2,048,000 bytes
```

Never flash an image larger than 2,048,000 bytes. OpenOCD may erase and program
the complete region even when the input file is too large, so a successful
OpenOCD exit is not a substitute for checking the file size.

## 1. Build the current general image

Load the tracked Buildroot configuration:

```sh
cd /workspace
make configure
```

Confirm that none of the external example packages is selected:

```sh
grep -E '^BR2_PACKAGE_(DISPLAY|DISPLAYDEBUG|FIND_MY_DEVICE|HELLOMK|HELLOMKCPP|IOEXAMPLE[1-8]|PERIPHERY|SLEEPEXAMPLE|USBSERIALDEVICE)=y$' \
	buildroot/.config
```

The command should print nothing. If it prints an enabled example, run
`make menuconfig`, disable that example, and save the configuration. Run
`make savedefconfig` only when the tracked defconfig should also be updated.

Build the current kernel and root filesystem:

```sh
make build_all
```

Record the baseline version, size, and checksum:

```sh
grep '^BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE=' buildroot/.config
stat -c 'xipImage: %s bytes' buildroot/output/images/xipImage
sha256sum buildroot/output/images/xipImage
```

Apply the mandatory size gate:

```sh
stat -c 'xipImage size: %s bytes' buildroot/output/images/xipImage
```

The printed size must not exceed 2,048,000 bytes. Do not flash a larger image.

Flash only after the size check succeeds:

```sh
make flash
```

Open the console on the host:

```sh
picocom -b 115200 /dev/ttyACM0
```

Confirm that the board reaches an interactive shell before changing the kernel
version.

## 2. Select the two upgrade targets

Check the maintained releases at <https://www.kernel.org/>. Prefer a long-term
kernel over a mainline or release-candidate kernel.

W25N02KV support was introduced in Linux 6.2 and remains present in Linux
6.6.151 and current kernels; it has not been removed.

Use Linux 6.6.151 as the fixed first target so its results remain reproducible.
Do not select the latest-stable version yet. Resolve it immediately before
starting the separate latest-stable procedure in section 8, after Linux
6.6.151 has passed its build and boot tests.

## 3. Create version-specific patches and back up the kernel configuration

When starting from a tree that contains only the Linux 6.1.27 patch directory,
create a copy named for Linux 6.6.151:

```sh
cd /workspace
cp -a firmware/board/stm32f429disco/linux-patches/linux-6.1.27 \
	firmware/board/stm32f429disco/linux-patches/linux-6.6.151
```

Do not run this copy command if `linux-patches/linux-6.6.151` already exists.
After creating it, Linux 6.1.27 continues to use
`linux-patches/linux-6.1.27`, while Linux 6.6.151 uses the new
`linux-patches/linux-6.6.151` directory.

Back up the old kernel configuration, but keep the active file at its
version-neutral name. The Makefile and defconfig continue to use
`firmware/board/stm32f429disco/linux.config` for every kernel version:

```sh
cp -a firmware/board/stm32f429disco/linux.config \
	firmware/board/stm32f429disco/linux-6.1.27.config
```

The snapshot is for comparison and rollback only. Do not rename or remove the
active `linux.config` file.

Keep the version-dependent selection in `firmware/external.mk`:

```make
LINUX_PATCHES += $(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-patches/linux-$(BR2_LINUX_KERNEL_VERSION)
```

Each copied patch must be reviewed against the new kernel. If a fix is already
upstream, remove that patch from the new directory only after confirming the
upstream code provides the same behavior. If context changed, adjust only the
new copy. Never alter the proven old-version series as part of an upgrade.

## 4. Select Linux 6.6.151 and apply its patches

Before changing Buildroot configuration, remove the currently selected kernel
build directory. This is also required if Linux 6.6 was already extracted or
built before a new version-specific patch was added:

```sh
make linux-dirclean
```

Edit only the kernel version in
`firmware/configs/stm32f429disco.defconfig`:

```text
BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE="6.6.151"
```

Keep the active kernel configuration path version-neutral:

```text
BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE="/workspace/firmware/board/stm32f429disco/linux.config"
```

Load the edited defconfig:

```sh
make configure
```

Confirm both effective values:

```sh
grep -E '^BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE=|^BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE=' \
	buildroot/.config
```

Linux 6.1 stores the STM32 device-tree files directly in:

```text
arch/arm/boot/dts/
```

Linux 6.6 stores them in:

```text
arch/arm/boot/dts/st/
```

Keep the custom board DTS version-independent. Its includes remain:

```dts
#include "stm32f429.dtsi"
#include "stm32f429-pinctrl.dtsi"
```

Do not add `st/` to this file. Linux 6.6 compatibility is provided by this
version-specific patch:

```text
firmware/board/stm32f429disco/linux-patches/linux-6.6.151/0004-arm-dts-stm32f429-add-flat-include-compatibility.patch
```

The patch creates two forwarding files in the extracted Linux 6.6 source:

```text
arch/arm/boot/dts/stm32f429.dtsi
arch/arm/boot/dts/stm32f429-pinctrl.dtsi
```

Those files forward the old paths to Linux 6.6's `st/` directory. The custom
DTS path in `firmware/configs/stm32f429disco.defconfig` remains unchanged:

```text
BR2_LINUX_KERNEL_CUSTOM_DTS_PATH="/workspace/firmware/board/stm32f429disco/dts/stm32f429disco-custom.dts"
```

This arrangement lets Linux 6.1 use its native flat files while Linux 6.6 gets
the compatibility files from its own patch directory. Switching versions does
not require editing the custom DTS.

Download, extract, and apply the version-specific patches before the full
build. This makes patch failures easier to distinguish from configuration or
link failures:

```sh
make -C buildroot BR2_EXTERNAL=/workspace/firmware linux-patch V=1
```

The output must show patch `0004` being applied without rejected hunks. Confirm
that it created both compatibility files:

```sh
ls buildroot/output/build/linux-6.6.151/arch/arm/boot/dts/stm32f429.dtsi
ls buildroot/output/build/linux-6.6.151/arch/arm/boot/dts/stm32f429-pinctrl.dtsi
```

Review or adjust patches in the new `linux-patches/linux-6.6.151` directory
only; do not modify the working Linux 6.1.27 patch directory.

## 5. Resolve kernel configuration changes

Resolve the Linux 6.1 configuration against Linux 6.6 before attempting the
first full build:

```sh
make linux-menuconfig
```

Review removed settings and new defaults in the menu. In particular,
`CONFIG_SLOB` no longer exists in Linux 6.6. For a small system, use the
allocator supported by Linux 6.6, normally `CONFIG_SLUB_TINY=y`. Options such
as `CONFIG_EXPERT=y` and `CONFIG_BASE_SMALL=y` can help keep unrelated defaults
out of the image, but enable them only after reviewing their effects.

Save and exit the menu. Then save the reviewed Linux configuration to the
active version-neutral file:

```sh
make linux-savedefconfig
```

This updates:

```text
firmware/board/stm32f429disco/linux.config
```

Do not rename this active file. The versioned configuration is created only
after the image has built and passed its boot test.

## 6. Build the Linux 6.6.151 general image

Remove the temporary kernel build directory created by patching and
menuconfig. The reviewed configuration is already saved in
`firmware/board/stm32f429disco/linux.config`:

```sh
make linux-dirclean
make build_all
```

Confirm that the DTB was built:

```text
buildroot/output/images/stm32f429disco-custom.dtb
```

Do not continue to flashing if the build fails or the expected DTB is missing.

## 7. Final checks before flashing the new kernel

```sh
test -s buildroot/output/images/xipImage
test -s buildroot/output/images/stm32f429disco-custom.dtb
stat -c 'xipImage size: %s bytes' buildroot/output/images/xipImage
```

The printed size must not exceed 2,048,000 bytes. Keep the result with the test
notes. A general image that barely fits may not leave enough space for later
feature configurations.

Flash only after the image and DTB checks succeed:

```sh
make flash
```

Open the serial console on the host and reset the board:

```sh
picocom -b 115200 /dev/ttyACM0
```

Confirm that Linux reaches the interactive shell. After the 6.6.151 image has
passed this boot test, save both active neutral files as the Linux 6.6.151
snapshots:

```sh
make linux-6.6.151-savedefconfig
```

This copies `firmware/board/stm32f429disco/linux.config` and
`firmware/configs/stm32f429disco.defconfig` to their versioned 6.6.151
snapshot files. It does not regenerate either neutral file. If kernel
menuconfig changes must be saved first, run `make linux-savedefconfig`; if
Buildroot menuconfig changes must be saved first, run `make savedefconfig`.

After the general image boots successfully, enable and test project examples
separately from the kernel-version migration.

## 8. Repeat the procedure with the latest stable kernel

Start this build only after the 6.6.151 general image has passed its serial boot
test. Print the latest stable version published by kernel.org:

```sh
cd /workspace
curl -fsSL https://www.kernel.org/releases.json | jq -r '.latest_stable.version'
```

Write down the printed version and use that exact number for the complete
build. The commands below use `7.1.8` as an example. If kernel.org printed a
different version, replace `7.1.8` with that version.

Clean the active 6.6.151 kernel before changing Buildroot configuration:

```sh
make linux-dirclean
```

Create a copy of the reviewed Linux 6.6.151 patch directory. For example, when
the latest version is 7.1.8:

```sh
cp -a firmware/board/stm32f429disco/linux-patches/linux-6.6.151 \
	firmware/board/stm32f429disco/linux-patches/linux-7.1.8
```

Do not modify `linux-patches/linux-6.6.151` while adapting patches for the
latest kernel.

Edit the version in `firmware/configs/stm32f429disco.defconfig`. For example:

```text
BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE="7.1.8"
BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE="/workspace/firmware/board/stm32f429disco/linux.config"
```

Do not change this toolchain declaration merely because the runtime kernel is
newer:

```text
BR2_TOOLCHAIN_EXTERNAL_HEADERS_6_1=y
```

It describes the headers used to build the existing C library and SDK, not the
kernel being compiled. Changing it without rebuilding the toolchain would make
the Buildroot configuration inaccurate.

Load the new defconfig and apply the new version-specific patch series:

```sh
make configure
make -C buildroot BR2_EXTERNAL=/workspace/firmware linux-patch V=1
```

Review every patch. Drop a patch from the latest-version directory only when
the equivalent fix is demonstrably present upstream; otherwise adjust its new
copy for changed context.

Keep the custom board DTS unchanged. The copied patch series contains the DTS
compatibility patch from section 4. Confirm that it still applies to the latest
kernel and still points to the correct vendor-directory files.

Resolve the reviewed Linux 6.6 configuration against the latest kernel before
the first full build:

```sh
make linux-menuconfig
```

Review the new and removed settings as described in section 5, save and exit
the menu, and then save the latest configuration:

```sh
make linux-savedefconfig
```

Only after resolving and saving the configuration, clean and perform the full
build:

```sh
make linux-dirclean
make build_all
```

Verify the resolved source version and DTB:

```sh
grep '^BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE=' buildroot/.config
test -s buildroot/output/images/stm32f429disco-custom.dtb
```

Repeat the image-size gate from section 7. Do not flash an oversized latest
kernel. A latest kernel that cannot retain enough headroom for the intended
application is not a suitable target merely because it builds successfully.
If the image fits, repeat the flashing and host serial-console test from
section 7.

After the latest image passes its serial boot test, save a versioned copy of
the active configuration. For example:

```sh
cp -a firmware/board/stm32f429disco/linux.config \
	firmware/board/stm32f429disco/linux-7.1.8.config
```

## 9. Roll back

### Roll back from Linux 6.6.151 to Linux 6.1.27

Restore both Linux 6.1.27 snapshots to their active version-neutral names,
reload the Buildroot configuration, and then remove any existing Linux 6.1.27
build before rebuilding it:

```sh
make linux-6.1.27-configure
make linux-dirclean
make build_all
```

`make linux-6.1.27-configure` validates the two snapshot files before copying
them and confirms that the versioned project defconfig selects Linux 6.1.27
while still referring to the active `linux.config` filename.

The old version-specific patch directory remains unchanged and will be selected
automatically by `firmware/external.mk`. The custom board DTS is unchanged;
Linux 6.1 uses its native flat DTSI files and does not apply the Linux 6.6
compatibility patch.

### Roll back from the latest kernel to Linux 6.6.151

Use the same sequence with `make linux-6.6.151-configure`. It restores both
6.6.151 snapshots and lets the version-specific Linux 6.6.151 patch directory
provide the DTS compatibility files.
