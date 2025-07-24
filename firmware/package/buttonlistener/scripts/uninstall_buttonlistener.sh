#!/bin/bash
echo "Removing buttonlistener from target..."
rm -f $(TARGET_DIR)/usr/bin/buttonlistener
rm -f $(TARGET_DIR)/etc/buttonlistener.ini