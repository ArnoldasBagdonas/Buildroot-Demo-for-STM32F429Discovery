#!/bin/bash
echo "Removing sleepexample from target..."
rm -f $(TARGET_DIR)/usr/bin/sleepexample
rm -f $(TARGET_DIR)/etc/sleepexample.ini