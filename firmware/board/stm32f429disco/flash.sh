#!/bin/bash

set -euo pipefail

OUTPUT_DIR=${1:-}
#BOARD_NAME=${2:-stm32f429discovery}
BOARD_NAME=stm32f429discovery  # Override here, hardcoded
OPENOCD_ADAPTER_SPEED_KHZ=${OPENOCD_ADAPTER_SPEED_KHZ:-1000}


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
if [ -f "${CONFIG_FILE}" ] && \
   grep -q '^BR2_PACKAGE_DISPLAYEXAMPLE=y$' "${CONFIG_FILE}"; then
  DISPLAY_ENABLED=true
fi
if [ -f "${CONFIG_FILE}" ] && \
   grep -q '^BR2_PACKAGE_USBSERIALDEVICE=y$' "${CONFIG_FILE}"; then
  USBSERIALDEVICE_ENABLED=true
fi
if [ -f "${CONFIG_FILE}" ] && \
   grep -q '^BR2_PACKAGE_FIND_MY_DEVICE=y$' "${CONFIG_FILE}"; then
  FIND_MY_DEVICE_ENABLED=true
fi

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

if [ ! -f "${DTB_FILE}" ]; then
  echo "ERROR: selected DTB does not exist: ${DTB_FILE}" >&2
  echo "Build the current menuconfig selection with 'make build_all' first." >&2
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
  -c "flash write_image erase ${OUTPUT_DIR}/images/stm32f429i-disco.bin 0x08000000" \
  -c "flash write_image erase ${DTB_FILE} 0x08004000" \
  -c "flash write_image erase ${OUTPUT_DIR}/images/xipImage 0x0800C000" \
  -c "reset run" \
  -c "shutdown"
