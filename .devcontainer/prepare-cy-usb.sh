#!/usr/bin/env bash

set -euo pipefail

# The Cypress USB-Serial adapter can be claimed by the cytherm driver because
# its USB ID is also associated with the CY7C63x0x thermometer. Rebind it to the
# CDC ACM serial driver before the devcontainer starts.
sudo modprobe -r cytherm
sudo modprobe cdc_acm
ls -l /dev/ttyACM*
