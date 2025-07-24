#!/bin/bash
echo "Removing ledbrightness from target..."
rm -f $(TARGET_DIR)/usr/bin/ledbrightness
rm -f $(TARGET_DIR)/etc/ledbrightness.ini