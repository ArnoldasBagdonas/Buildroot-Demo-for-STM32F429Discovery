#!/bin/bash
echo "Removing gpiodscan from target..."
rm -f $(TARGET_DIR)/usr/bin/gpiodscan
rm -f $(TARGET_DIR)/etc/gpiodscan.ini