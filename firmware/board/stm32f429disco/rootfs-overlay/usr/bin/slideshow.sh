#!/bin/sh

echo "Starting photo frame slideshow..."

IMAGE_DIR="/usr/share/images"
DELAY=5  # seconds per image

# Check if fbv is installed
if command -v fbv >/dev/null 2>&1; then
  USE_FBV=true
  echo "fbv found: using it to display images."
else
  USE_FBV=false
  echo "fbv not found: falling back to printing filenames."
fi

while true; do
  for img in "$IMAGE_DIR"/*; do
    [ -f "$img" ] || continue
    case "$img" in
      *.jpg|*.jpeg|*.png|*.bmp)
        if [ "$USE_FBV" = true ]; then
          echo "Displaying image: $img"
          fbv -f "$img"
        else
          echo "Image: $img"
        fi
        sleep "$DELAY"
        ;;
      *)
        # skip non-image files
        ;;
    esac
  done
done
