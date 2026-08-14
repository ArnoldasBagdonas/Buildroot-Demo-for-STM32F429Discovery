# Linux version change procedure

This procedure upgrades the Linux kernel independently of the project examples.
It produces and tests three general STM32F429 Discovery images in order: the
current 6.1.27 baseline, the fixed 6.6.151 LTS target, and the newest stable
kernel available from kernel.org at build time. Enable and test examples only
after each general image passes its size and boot checks.

Run every build command inside the project devcontainer, with `/workspace` as
the working directory. Use the host only for the serial console.

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
if grep -E '^BR2_PACKAGE_(DISPLAYDEBUG|DISPLAYEXAMPLE|FIND_MY_DEVICE|HELLOMK|HELLOMKCPP|IOEXAMPLE[1-8]|PERIPHERY|SLEEPEXAMPLE|USBSERIALDEVICE)=y$' \
	buildroot/.config; then
	echo 'Disable the listed examples before continuing.' >&2
	false
fi
```

If an example is enabled, run `make menuconfig`, disable it, save, and run
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
XIP_BYTES=$(stat -c %s buildroot/output/images/xipImage)
XIP_MAX_BYTES=2048000
echo "xipImage: ${XIP_BYTES}/${XIP_MAX_BYTES} bytes"
test "${XIP_BYTES}" -le "${XIP_MAX_BYTES}"
```

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

For the Winbond W25N02KV:

- Linux 6.1 does not contain its SPI-NAND ID and ECC implementation.
- W25N02KV support is present from Linux 6.2 onward.
- Linux 6.6 LTS contains the driver and is the preferred first upgrade for this
  flash-constrained target.
- Much newer kernels may enable additional subsystems by default and can exceed
  internal flash even when they support the NAND device.

The driver entry can be verified in the selected source at
`drivers/mtd/nand/spi/winbond.c`. It must contain `W25N02KV` with ID bytes
`0xaa, 0x22`.

The first target is fixed so its results remain reproducible:

```sh
LTS_LINUX_VERSION=6.6.151
```

Resolve the second target from kernel.org immediately before starting its
build. `latest_stable` deliberately excludes mainline release candidates:

```sh
LATEST_LINUX_VERSION=$(
	curl -fsSL https://www.kernel.org/releases.json |
	jq -er '.latest_stable.version'
)
echo "Latest stable Linux: ${LATEST_LINUX_VERSION}"
test -n "${LATEST_LINUX_VERSION}"
```

Record the resolved value in the test notes. Do not silently replace it during
an in-progress build; a later release is a separate test target.

## 3. Create version-specific patches and kernel configuration

Do not make a new kernel reuse a directory named for an older kernel. Preserve
the working patch set and create a separately reviewable copy:

```sh
cd /workspace
TARGET_LINUX_VERSION=6.6.151
OLD_PATCH_DIR=firmware/board/stm32f429disco/linux-patches/linux-6.1.27
NEW_PATCH_DIR="firmware/board/stm32f429disco/linux-patches/linux-${TARGET_LINUX_VERSION}"

test -d "${OLD_PATCH_DIR}"
test ! -e "${NEW_PATCH_DIR}"
cp -a "${OLD_PATCH_DIR}" "${NEW_PATCH_DIR}"
```

Preserve the old kernel configuration in the same way. The new kernel must not
overwrite the configuration still used by 6.1.27:

```sh
OLD_KERNEL_CONFIG=firmware/board/stm32f429disco/linux.config
NEW_KERNEL_CONFIG="firmware/board/stm32f429disco/linux-${TARGET_LINUX_VERSION}.config"

test -f "${OLD_KERNEL_CONFIG}"
test ! -e "${NEW_KERNEL_CONFIG}"
cp -a "${OLD_KERNEL_CONFIG}" "${NEW_KERNEL_CONFIG}"
```

Keep the version-dependent selection in `firmware/external.mk`:

```make
LINUX_PATCHES += $(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/linux-patches/linux-$(BR2_LINUX_KERNEL_VERSION)
```

Each copied patch must be reviewed against the new kernel. If a fix is already
upstream, remove that patch from the new directory only after confirming the
upstream code provides the same behavior. If context changed, adjust only the
new copy. Never alter the proven old-version series as part of an upgrade.

## 4. Build the 6.6.151 general image

Before changing Buildroot configuration, remove the currently selected kernel
build directory:

```sh
make linux-dirclean
```

Edit both Linux settings in `firmware/configs/stm32f429disco.defconfig`:

```text
BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE="6.6.151"
BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE="/workspace/firmware/board/stm32f429disco/linux-6.6.151.config"
```

Load the edited defconfig:

```sh
make configure
```

Confirm the effective value:

```sh
grep '^BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE=' buildroot/.config
```

Download, extract, and apply the version-specific patches before the full
build. This makes patch failures easier to distinguish from configuration or
link failures:

```sh
make -C buildroot BR2_EXTERNAL=/workspace/firmware linux-patch V=1
```

All patches must apply without rejected hunks. Then build the general image:

```sh
make build_all
```

Verify that the chosen source contains W25N02KV support:

```sh
TARGET_LINUX_VERSION=6.6.151
grep -n -A8 'W25N02KV' \
	"buildroot/output/build/linux-${TARGET_LINUX_VERSION}/drivers/mtd/nand/spi/winbond.c"
```

Complete the configuration review and final checks in the following sections,
then boot-test this image before starting the latest-stable build.

## 5. Resolve kernel configuration changes

Do not accept new kernel defaults without reviewing them. Inspect warnings from
the build, then use:

```sh
make linux-menuconfig
```

After making intentional changes, save and copy the configuration to the file
selected by the current target. For the 6.6.151 build:

```sh
make -C buildroot BR2_EXTERNAL=/workspace/firmware linux-savedefconfig
cp buildroot/output/build/linux-6.6.151/defconfig \
	firmware/board/stm32f429disco/linux-6.6.151.config
```

Rebuild from the saved configuration:

```sh
make linux-dirclean
make build_all
```

For kernels where `CONFIG_SLOB` no longer exists, select the small-system
allocator supported by that kernel, normally `CONFIG_SLUB_TINY=y`. Modern
kernels may also require `CONFIG_EXPERT=y` and `CONFIG_BASE_SMALL=y` to keep
desktop-oriented defaults out of the image. Review every such change in the
generated `.config`; do not enable options solely to silence a warning.

## 6. Device-tree layout in newer kernels

Linux 6.6 retains the flat STM32 ARM device-tree location used by this project.
Newer kernels place ST device trees below:

```text
arch/arm/boot/dts/st/
```

Buildroot 2024.02 predates that relocation. When upgrading to a kernel using
the `st/` directory, the project must copy its custom DTS files into that
directory and use a kernel-side DTS name such as:

```make
LINUX_DTS_NAME = st/stm32f429disco-custom
```

Treat this as a separate compatibility change. Confirm that the resulting DTB
is installed as:

```text
buildroot/output/images/stm32f429disco-custom.dtb
```

Do not proceed to flashing if the expected DTB is missing.

## 7. Final checks before flashing the new kernel

```sh
test -s buildroot/output/images/xipImage
test -s buildroot/output/images/stm32f429disco-custom.dtb

XIP_BYTES=$(stat -c %s buildroot/output/images/xipImage)
XIP_MAX_BYTES=2048000
XIP_FREE_BYTES=$((XIP_MAX_BYTES - XIP_BYTES))

echo "xipImage: ${XIP_BYTES}/${XIP_MAX_BYTES} bytes"
echo "remaining kernel flash: ${XIP_FREE_BYTES} bytes"
test "${XIP_BYTES}" -le "${XIP_MAX_BYTES}"
```

Keep the size result with the test notes. A general image that barely fits is
not sufficient for later examples or storage filesystems; their kernel code and
initramfs contents require additional headroom.

After the general image boots successfully, enable and test one example at a
time. For SPI NAND, first enable only MTD and SPI-NAND enumeration and verify
the device geometry without erasing or formatting it. Add UBI and UBIFS only
after the non-destructive detection test passes and the complete image still
meets the 2,048,000-byte limit.

## 8. Repeat the procedure with the latest stable kernel

Start this build only after the 6.6.151 general image has passed its serial boot
test. Resolve the version again at the start of the build and keep that exact
value for the complete test:

```sh
cd /workspace
LATEST_LINUX_VERSION=$(
	curl -fsSL https://www.kernel.org/releases.json |
	jq -er '.latest_stable.version'
)
echo "Building latest stable Linux ${LATEST_LINUX_VERSION}"
test -n "${LATEST_LINUX_VERSION}"
```

Clean the active 6.6.151 kernel before changing Buildroot configuration:

```sh
make linux-dirclean
```

Create a new patch directory from the reviewed 6.6.151 series. Never reuse or
modify the 6.6.151 directory in place:

```sh
SOURCE_PATCH_DIR=firmware/board/stm32f429disco/linux-patches/linux-6.6.151
LATEST_PATCH_DIR="firmware/board/stm32f429disco/linux-patches/linux-${LATEST_LINUX_VERSION}"

test -d "${SOURCE_PATCH_DIR}"
test ! -e "${LATEST_PATCH_DIR}"
cp -a "${SOURCE_PATCH_DIR}" "${LATEST_PATCH_DIR}"
```

Create an independent latest-kernel configuration from the reviewed 6.6.151
configuration:

```sh
SOURCE_KERNEL_CONFIG=firmware/board/stm32f429disco/linux-6.6.151.config
LATEST_KERNEL_CONFIG="firmware/board/stm32f429disco/linux-${LATEST_LINUX_VERSION}.config"

test -f "${SOURCE_KERNEL_CONFIG}"
test ! -e "${LATEST_KERNEL_CONFIG}"
cp -a "${SOURCE_KERNEL_CONFIG}" "${LATEST_KERNEL_CONFIG}"
```

Edit `firmware/configs/stm32f429disco.defconfig` and set the exact version and
matching configuration path:

```text
BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE="<LATEST_LINUX_VERSION>"
BR2_LINUX_KERNEL_CUSTOM_CONFIG_FILE="/workspace/firmware/board/stm32f429disco/linux-<LATEST_LINUX_VERSION>.config"
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

For kernels using `arch/arm/boot/dts/st/`, add a latest-kernel compatibility
block to `firmware/external.mk` while building the general image:

```make
LINUX_DTS_NAME := $(filter-out stm32f429disco-custom,$(LINUX_DTS_NAME))
LINUX_DTS_NAME += st/stm32f429disco-custom

define STM32F429_COPY_DTS_TO_ST
	$(INSTALL) -D -m 0644 \
		$(BR2_EXTERNAL_FIRMWARE_PATH)/board/stm32f429disco/dts/stm32f429disco-custom.dts \
		$(LINUX_ARCH_PATH)/boot/dts/st/stm32f429disco-custom.dts
endef
LINUX_PRE_BUILD_HOOKS += STM32F429_COPY_DTS_TO_ST
```

This block is not needed by Linux 6.1 or 6.6. Guard or remove it before
rebuilding those versions.

Build once to expose obsolete symbols and new defaults:

```sh
make build_all
```

If the build or size check fails, review the kernel configuration as described
in section 5. In particular, newer kernels may require replacing
`CONFIG_SLOB=y` with `CONFIG_SLUB_TINY=y`, enabling `CONFIG_EXPERT=y` and
`CONFIG_BASE_SMALL=y`, and disabling unrelated new STM32MP defaults. Save every
intentional change to its version-specific file, then clean and rebuild:

```sh
make -C buildroot BR2_EXTERNAL=/workspace/firmware linux-savedefconfig
cp "buildroot/output/build/linux-${LATEST_LINUX_VERSION}/defconfig" \
	"firmware/board/stm32f429disco/linux-${LATEST_LINUX_VERSION}.config"
make linux-dirclean
make build_all
```

Verify the resolved source version, NAND entry, DTB, and XIP size:

```sh
grep '^BR2_LINUX_KERNEL_CUSTOM_VERSION_VALUE=' buildroot/.config
grep -n -A8 'W25N02KV' \
	"buildroot/output/build/linux-${LATEST_LINUX_VERSION}/drivers/mtd/nand/spi/winbond.c"

test -s buildroot/output/images/stm32f429disco-custom.dtb
XIP_BYTES=$(stat -c %s buildroot/output/images/xipImage)
XIP_MAX_BYTES=2048000
echo "xipImage: ${XIP_BYTES}/${XIP_MAX_BYTES} bytes"
test "${XIP_BYTES}" -le "${XIP_MAX_BYTES}"
```

Do not flash an oversized latest kernel. A latest kernel that cannot retain
enough headroom for the intended application is not a suitable target merely
because it builds successfully.

## 9. Roll back

Clean the currently selected new kernel before changing the version back:

```sh
make linux-dirclean
```

Restore both the previous version and its matching custom kernel configuration
path in `firmware/configs/stm32f429disco.defconfig`, then reload and rebuild:

```sh
make configure
make build_all
```

The old version-specific patch directory remains unchanged and will be selected
automatically by `firmware/external.mk`.
