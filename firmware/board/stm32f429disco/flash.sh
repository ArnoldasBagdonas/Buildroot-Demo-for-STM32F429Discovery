#!/bin/bash

set -euo pipefail

OUTPUT_DIR=${1:-}
#BOARD_NAME=${2:-stm32f429discovery}
BOARD_NAME=stm32f429discovery  # Override here, hardcoded
OPENOCD_ADAPTER_SPEED_KHZ=${OPENOCD_ADAPTER_SPEED_KHZ:-1000}
INTERNAL_FLASH_SIZE=$((0x200000))
BOOTLOADER_OFFSET=$((0x0000))
DTB_OFFSET=$((0x4000))
XIP_OFFSET=$((0xc000))
BOOTLOADER_MAX_SIZE=$((DTB_OFFSET - BOOTLOADER_OFFSET))
DTB_MAX_SIZE=$((XIP_OFFSET - DTB_OFFSET))
XIP_MAX_SIZE=$((INTERNAL_FLASH_SIZE - XIP_OFFSET))
PRE_XIP_RESERVED_SIZE=$((BOOTLOADER_MAX_SIZE + DTB_MAX_SIZE))
BOOTLOADER_ADDRESS=0x08000000
DTB_ADDRESS=0x08004000
XIP_ADDRESS=0x0800C000


if ! test -d "${OUTPUT_DIR}" ; then
    echo "ERROR: no output directory specified."
    echo "Usage: $0 OUTPUT_DIR BOARD_NAME"
    echo ""
    echo "Arguments:"
    echo "    OUTPUT_DIR    The Buildroot output directory."
    echo "    BOARD_NAME    One of the available boards among:"
    echo "                  stm32f429discovery, stm32f429disc1"
    exit 1
fi

# ${OUTPUT_DIR}/host/bin/openocd -f board/${BOARD_NAME}.cfg \
#   -c "init" \
#   -c "reset init" \
#   -c "flash probe 0" \
#   -c "flash info 0" \
#   -c "flash write_image erase ${OUTPUT_DIR}/images/stm32f429i-disco.bin 0x08000000" \
#   -c "flash write_image erase ${OUTPUT_DIR}/images/stm32f429-disco.dtb 0x08004000" \
#   -c "flash write_image erase ${OUTPUT_DIR}/images/xipImage 0x0800C000" \
#   -c "reset run" \
#   -c "shutdown"

CONFIG_FILE=${OUTPUT_DIR}/../.config
if [ -f "${OUTPUT_DIR}/.config" ]; then
  # Also support a Buildroot out-of-tree output directory.
  CONFIG_FILE=${OUTPUT_DIR}/.config
fi
DISPLAY_ENABLED=false
USBSERIALDEVICE_ENABLED=false
FIND_MY_DEVICE_ENABLED=false
SPINAND_ENABLED=false
SPINOR_ENABLED=false
SDCARD_ENABLED=false
if [ -f "${CONFIG_FILE}" ] && \
   grep -q '^BR2_PACKAGE_DISPLAY=y$' "${CONFIG_FILE}"; then
  DISPLAY_ENABLED=true
fi
if [ -f "${CONFIG_FILE}" ] && \
   grep -q '^BR2_PACKAGE_GALLERY=y$' "${CONFIG_FILE}"; then
  DISPLAY_ENABLED=true
fi
if [ -f "${CONFIG_FILE}" ] && \
   grep -q '^BR2_PACKAGE_FIRMWARE_SCREEN=y$' "${CONFIG_FILE}"; then
  DISPLAY_ENABLED=true
fi
if [ -f "${CONFIG_FILE}" ] && \
   grep -q '^BR2_PACKAGE_GALLERY_SDCARD=y$' "${CONFIG_FILE}"; then
  SDCARD_ENABLED=true
fi
if [ -f "${CONFIG_FILE}" ] && \
   grep -q '^BR2_PACKAGE_USBSERIALDEVICE=y$' "${CONFIG_FILE}"; then
  USBSERIALDEVICE_ENABLED=true
fi
if [ -f "${CONFIG_FILE}" ] && \
   grep -q '^BR2_PACKAGE_FIND_MY_DEVICE=y$' "${CONFIG_FILE}"; then
  FIND_MY_DEVICE_ENABLED=true
fi
if [ -f "${CONFIG_FILE}" ] && \
   grep -q '^BR2_PACKAGE_SPINAND=y$' "${CONFIG_FILE}"; then
  SPINAND_ENABLED=true
fi
if [ -f "${CONFIG_FILE}" ] && \
   grep -q '^BR2_PACKAGE_SPINOR=y$' "${CONFIG_FILE}"; then
  SPINOR_ENABLED=true
fi
if [ -f "${CONFIG_FILE}" ] && \
   grep -q '^BR2_PACKAGE_SDCARD=y$' "${CONFIG_FILE}"; then
  SDCARD_ENABLED=true
fi

XIP_FILE=${OUTPUT_DIR}/images/xipImage
if [ ! -f "${XIP_FILE}" ]; then
  echo "ERROR: XIP image does not exist: ${XIP_FILE}" >&2
  echo "Build the current menuconfig selection with 'make build_all' first." >&2
  exit 1
fi
XIP_SIZE=$(stat -c %s "${XIP_FILE}")

if ${USBSERIALDEVICE_ENABLED} && ${DISPLAY_ENABLED}; then
  DTB_FILE=${OUTPUT_DIR}/images/stm32f429disco-usbserialdevice-display.dtb
elif ${USBSERIALDEVICE_ENABLED}; then
  DTB_FILE=${OUTPUT_DIR}/images/stm32f429disco-usbserialdevice.dtb
elif ${DISPLAY_ENABLED}; then
  DTB_FILE=${OUTPUT_DIR}/images/stm32f429disco-display.dtb
else
  DTB_FILE=${OUTPUT_DIR}/images/stm32f429disco-custom.dtb
fi

if ${FIND_MY_DEVICE_ENABLED}; then
  DTB_FILE=${DTB_FILE%.dtb}-w5500.dtb
fi

if ${FIND_MY_DEVICE_ENABLED}; then
  # Find My Device's generated composition include carries SPI-NOR, while
  # its existing suffix continues to identify SPI-NAND or SD-card wiring.
  if ${SPINAND_ENABLED}; then
    DTB_FILE=${DTB_FILE%.dtb}-spinand.dtb
  fi
  if ${SDCARD_ENABLED}; then
    DTB_FILE=${DTB_FILE%.dtb}-sdcard.dtb
  fi
elif ${SPINOR_ENABLED}; then
  # The -spinor DTB composition also includes selected SPI-NAND or SD wiring.
  DTB_FILE=${DTB_FILE%.dtb}-spinor.dtb
else
  if ${SPINAND_ENABLED}; then
    DTB_FILE=${DTB_FILE%.dtb}-spinand.dtb
  fi
  if ${SDCARD_ENABLED}; then
    DTB_FILE=${DTB_FILE%.dtb}-sdcard.dtb
  fi
fi

if [ ! -f "${DTB_FILE}" ]; then
  echo "ERROR: selected DTB does not exist: ${DTB_FILE}" >&2
  echo "Build the current menuconfig selection with 'make build_all' first." >&2
  exit 1
fi

BOOTLOADER_FILE=${OUTPUT_DIR}/images/stm32f429i-disco.bin
if [ ! -f "${BOOTLOADER_FILE}" ]; then
  echo "ERROR: bootloader image does not exist: ${BOOTLOADER_FILE}" >&2
  exit 1
fi

BOOTLOADER_SIZE=$(stat -c %s "${BOOTLOADER_FILE}")
DTB_SIZE=$(stat -c %s "${DTB_FILE}")
echo "Internal flash map (2,097,152 bytes total):"
echo "  bootloader ${BOOTLOADER_ADDRESS}: ${BOOTLOADER_SIZE}/${BOOTLOADER_MAX_SIZE} bytes"
echo "  device tree ${DTB_ADDRESS}: ${DTB_SIZE}/${DTB_MAX_SIZE} bytes"
echo "  XIP kernel  ${XIP_ADDRESS}: ${XIP_SIZE}/${XIP_MAX_SIZE} bytes"
echo "  fixed reservation before XIP: ${PRE_XIP_RESERVED_SIZE} bytes; XIP headroom: $((XIP_MAX_SIZE - XIP_SIZE)) bytes"

if (( BOOTLOADER_SIZE > BOOTLOADER_MAX_SIZE )); then
  echo "ERROR: bootloader overlaps the DTB slot by $((BOOTLOADER_SIZE - BOOTLOADER_MAX_SIZE)) bytes." >&2
  exit 1
fi
if (( DTB_SIZE > DTB_MAX_SIZE )); then
  echo "ERROR: selected DTB overlaps the XIP slot by $((DTB_SIZE - DTB_MAX_SIZE)) bytes." >&2
  exit 1
fi
if (( XIP_SIZE > XIP_MAX_SIZE )); then
  echo "ERROR: xipImage exceeds the STM32F429 internal-flash region by $((XIP_SIZE - XIP_MAX_SIZE)) bytes." >&2
  echo "Enable an initramfs-compression option or deselect features, rebuild, and retry." >&2
  echo "Refusing to overwrite past the STM32F429 internal-flash region." >&2
  exit 1
fi

echo "Flashing DTB: $DTB_FILE"
echo "ST-LINK adapter speed: ${OPENOCD_ADAPTER_SPEED_KHZ} kHz"

${OUTPUT_DIR}/host/bin/openocd -f board/${BOARD_NAME}.cfg \
  -c "adapter speed ${OPENOCD_ADAPTER_SPEED_KHZ}" \
  -c "init" \
  -c "reset init" \
  -c "adapter speed ${OPENOCD_ADAPTER_SPEED_KHZ}" \
  -c "flash probe 0" \
  -c "flash info 0" \
  -c "flash write_image erase ${BOOTLOADER_FILE} ${BOOTLOADER_ADDRESS}" \
  -c "flash write_image erase ${DTB_FILE} ${DTB_ADDRESS}" \
  -c "flash write_image erase ${XIP_FILE} ${XIP_ADDRESS}" \
  -c "reset run" \
  -c "shutdown"
