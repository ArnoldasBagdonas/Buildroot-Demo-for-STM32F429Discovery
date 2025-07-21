#!/bin/bash

WORKSPACE_DIR="/workspace/firmware/board/stm32f429disco"

echo "[DevContainer Entrypoint] Fixing permissions..."

for script in pre-build.sh post-build.sh post-genext2fs.sh post-image.sh; do
  if [ -f "$WORKSPACE_DIR/$script" ]; then
    chmod +x "$WORKSPACE_DIR/$script" || echo "Failed chmod on $script"
  else
    echo "File $script not found in $WORKSPACE_DIR, skipping chmod"
  fi
done

echo "[DevContainer Entrypoint] Permissions fixed." 