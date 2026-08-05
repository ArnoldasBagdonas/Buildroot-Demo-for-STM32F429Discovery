#!/bin/sh

set -eu

# The production-minimal image uses a direct console shell and has no network,
# init scripts, package metadata, or diagnostic databases. Buildroot's device
# table still requires the minimal account files while creating the initramfs.
rm -rf "${TARGET_DIR}/etc/init.d" "${TARGET_DIR}/etc/profile.d"
rm -f \
	"${TARGET_DIR}/etc/group" \
	"${TARGET_DIR}/etc/hostname" \
	"${TARGET_DIR}/etc/issue" \
	"${TARGET_DIR}/etc/mtab" \
	"${TARGET_DIR}/etc/os-release" \
	"${TARGET_DIR}/etc/profile" \
	"${TARGET_DIR}/etc/protocols" \
	"${TARGET_DIR}/etc/resolv.conf" \
	"${TARGET_DIR}/etc/services" \
	"${TARGET_DIR}/etc/shells" \
	"${TARGET_DIR}/usr/lib/os-release"

# Buildroot's target-finalize and STM32 common hook edit these files on every
# make invocation, so leave minimal placeholders to keep repeated builds valid.
: > "${TARGET_DIR}/etc/hosts"
: > "${TARGET_DIR}/etc/fstab"
chmod 0755 "${TARGET_DIR}/init"
