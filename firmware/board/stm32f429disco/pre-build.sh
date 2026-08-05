#!/bin/sh

echo "==> Applying permissions to rootfs-overlay files..."

chmod +x /workspace/firmware/rootfs-overlay/usr/bin/photo-frame.sh
chmod +x /workspace/firmware/rootfs-overlay/etc/init.d/S99photo-frame

echo "   ✔ Permissions applied successfully."
