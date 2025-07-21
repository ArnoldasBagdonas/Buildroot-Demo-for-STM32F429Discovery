#!/bin/bash
echo "Removing gpiodio from target..."
rm -f $(TARGET_DIR)/usr/bin/gpiodio
rm -f $(TARGET_DIR)/etc/gpiodio.ini