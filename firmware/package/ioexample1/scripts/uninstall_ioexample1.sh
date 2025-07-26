#!/bin/bash
echo "Removing ioexample1 from target..."
rm -f $(TARGET_DIR)/usr/bin/ioexample1
rm -f $(TARGET_DIR)/etc/ioexample1.ini