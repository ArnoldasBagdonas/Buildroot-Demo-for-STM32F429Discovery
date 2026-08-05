#!/usr/bin/env bash

set -euo pipefail

# This script is executed on the host by devcontainer.json's initializeCommand.
# Host-side permissions are required for rootless Podman; sudo inside the
# container cannot override permissions on a host-owned USB device node.

readonly STLINK_VENDOR_ID="0483"
readonly SYSFS_USB_ROOT="${STLINK_SYSFS_USB_ROOT:-/sys/bus/usb/devices}"
readonly USB_DEVICE_ROOT="${STLINK_USB_DEVICE_ROOT:-/dev/bus/usb}"

is_stlink_product_id() {
    case "${1,,}" in
        3744|3748|374a|374b|374d|374e|374f|3752|3753|3754)
            return 0
            ;;
        *)
            return 1
            ;;
    esac
}

find_stlink_nodes() {
    local sysfs_device vendor_id product_id bus_number device_number device_node

    for sysfs_device in "$SYSFS_USB_ROOT"/*; do
        [[ -f "$sysfs_device/idVendor" && -f "$sysfs_device/idProduct" ]] || continue

        read -r vendor_id < "$sysfs_device/idVendor"
        read -r product_id < "$sysfs_device/idProduct"
        [[ "${vendor_id,,}" == "$STLINK_VENDOR_ID" ]] || continue
        is_stlink_product_id "$product_id" || continue
        [[ -f "$sysfs_device/busnum" && -f "$sysfs_device/devnum" ]] || continue

        read -r bus_number < "$sysfs_device/busnum"
        read -r device_number < "$sysfs_device/devnum"
        printf -v device_node '%s/%03d/%03d' \
            "$USB_DEVICE_ROOT" "$((10#$bus_number))" "$((10#$device_number))"
        [[ -c "$device_node" ]] && printf '%s\n' "$device_node"
    done
}

mapfile -t stlink_nodes < <(find_stlink_nodes)

if (( ${#stlink_nodes[@]} == 0 )); then
    echo "[DevContainer USB] No connected ST-LINK was found."
    echo "[DevContainer USB] Connect it before creating the container if flashing is required."
    exit 0
fi

nodes_needing_access=()
for device_node in "${stlink_nodes[@]}"; do
    if [[ -r "$device_node" && -w "$device_node" ]]; then
        echo "[DevContainer USB] ST-LINK is already accessible: $device_node"
    else
        nodes_needing_access+=("$device_node")
    fi
done

(( ${#nodes_needing_access[@]} > 0 )) || exit 0

chmod_command="$(command -v chmod)"

if (( EUID == 0 )); then
    "$chmod_command" 0666 "${nodes_needing_access[@]}"
elif command -v sudo >/dev/null 2>&1 && sudo -n "$chmod_command" 0666 "${nodes_needing_access[@]}" 2>/dev/null; then
    :
elif [[ -t 0 ]] && command -v sudo >/dev/null 2>&1; then
    echo "[DevContainer USB] Host authentication is required for ST-LINK access."
    sudo "$chmod_command" 0666 "${nodes_needing_access[@]}"
elif command -v pkexec >/dev/null 2>&1; then
    echo "[DevContainer USB] Requesting host authorization for ST-LINK access..."
    pkexec "$chmod_command" 0666 "${nodes_needing_access[@]}"
else
    echo "[DevContainer USB] ERROR: Cannot change the host ST-LINK permissions." >&2
    echo "Run this command in a host terminal, then recreate the devcontainer:" >&2
    printf '  sudo chmod 0666' >&2
    printf ' %q' "${nodes_needing_access[@]}" >&2
    printf '\n' >&2
    exit 1
fi

for device_node in "${nodes_needing_access[@]}"; do
    if [[ ! -r "$device_node" || ! -w "$device_node" ]]; then
        echo "[DevContainer USB] ERROR: ST-LINK is still inaccessible: $device_node" >&2
        exit 1
    fi
    echo "[DevContainer USB] Granted ST-LINK access: $device_node"
done
