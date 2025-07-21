#!/bin/bash
echo "Removing gpiodevent from target..."
rm -f $(TARGET_DIR)/usr/bin/gpiodevent
rm -f $(TARGET_DIR)/etc/gpiodevent.ini