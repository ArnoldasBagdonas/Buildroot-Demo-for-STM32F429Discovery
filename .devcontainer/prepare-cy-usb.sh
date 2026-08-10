#!/usr/bin/env bash

set -euo pipefail

# This host-side workaround is only needed for the Cypress 04b4:0002 USB
# serial adapter.  The same ID is claimed by Linux's cytherm driver.  Keep the
# devcontainer usable without hardware: do not request privileges or require a
# /dev/ttyACM node unless that exact adapter is connected.

readonly CYPRESS_VENDOR_ID="04b4"
readonly CYPRESS_PRODUCT_ID="0002"
readonly SYSFS_USB_ROOT="${CYPRESS_SYSFS_USB_ROOT:-/sys/bus/usb/devices}"
readonly TTY_DEVICE_ROOT="${CYPRESS_TTY_DEVICE_ROOT:-/dev}"

find_cypress_devices() {
    local sysfs_device vendor_id product_id

    for sysfs_device in "$SYSFS_USB_ROOT"/*; do
        [[ -f "$sysfs_device/idVendor" && -f "$sysfs_device/idProduct" ]] || continue
        read -r vendor_id < "$sysfs_device/idVendor"
        read -r product_id < "$sysfs_device/idProduct"
        if [[ "${vendor_id,,}" == "$CYPRESS_VENDOR_ID" &&
              "${product_id,,}" == "$CYPRESS_PRODUCT_ID" ]]; then
            printf '%s\n' "$sysfs_device"
        fi
    done
}

find_acm_nodes() {
    local sysfs_device device_name interface_path tty_path tty_name

    for sysfs_device in "${cypress_devices[@]}"; do
        device_name="${sysfs_device##*/}"
        for interface_path in "$SYSFS_USB_ROOT/$device_name":*; do
            [[ -e "$interface_path" ]] || continue
            for tty_path in "$interface_path"/tty/ttyACM* "$interface_path"/ttyACM*; do
                [[ -e "$tty_path" ]] || continue
                tty_name="${tty_path##*/}"
                [[ -e "$TTY_DEVICE_ROOT/$tty_name" ]] && printf '%s\n' "$TTY_DEVICE_ROOT/$tty_name"
            done
        done
    done
}

run_privileged() {
    if (( EUID == 0 )); then
        "$@"
    elif command -v sudo >/dev/null 2>&1 && sudo -n "$@" 2>/dev/null; then
        :
    elif [[ -t 0 ]] && command -v sudo >/dev/null 2>&1; then
        echo "[DevContainer USB] Host authentication is required for Cypress adapter setup."
        sudo "$@"
    elif command -v pkexec >/dev/null 2>&1; then
        echo "[DevContainer USB] Requesting host authorization for Cypress adapter setup..."
        pkexec "$@"
    else
        echo "[DevContainer USB] ERROR: Cannot rebind the Cypress USB-serial adapter." >&2
        echo "Run this command in a host terminal, then recreate the devcontainer:" >&2
        printf '  sudo' >&2
        printf ' %q' "$@" >&2
        printf '\n' >&2
        return 1
    fi
}

mapfile -t cypress_devices < <(find_cypress_devices)

if (( ${#cypress_devices[@]} == 0 )); then
    echo "[DevContainer USB] No Cypress 04b4:0002 USB-serial adapter was found."
    echo "[DevContainer USB] Continuing without the optional external serial console."
    exit 0
fi

mapfile -t acm_nodes < <(find_acm_nodes)
if (( ${#acm_nodes[@]} > 0 )); then
    printf '[DevContainer USB] Cypress serial adapter is ready: %s\n' "${acm_nodes[@]}"
    exit 0
fi

# Remove the driver's overly broad claim, then let the adapter's CDC ACM
# interface bind to the correct driver.  modprobe -r is harmless when cytherm
# has no users other than this connected adapter.
run_privileged modprobe -r cytherm
run_privileged modprobe cdc_acm

# Give udev a chance to finish creating the character device without making
# udevadm a hard dependency of the host environment.
if command -v udevadm >/dev/null 2>&1; then
    udevadm settle --timeout=5 || true
fi

mapfile -t acm_nodes < <(find_acm_nodes)
if (( ${#acm_nodes[@]} == 0 )); then
    echo "[DevContainer USB] WARNING: Cypress adapter is connected, but no /dev/ttyACM node appeared." >&2
    echo "[DevContainer USB] Container creation will continue; reconnect the adapter and rerun this script." >&2
    exit 0
fi

printf '[DevContainer USB] Cypress serial adapter is ready: %s\n' "${acm_nodes[@]}"
