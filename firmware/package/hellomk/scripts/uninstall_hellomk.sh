#!/bin/bash
echo "Removing hellomk from target..."
rm -f $(TARGET_DIR)/usr/bin/hellomk
rm -f $(TARGET_DIR)/etc/hellomk.ini