#!/bin/sh

mkimage -T script -C none -A arm -n 'Landshark Boot Script' -d boot.sh boot.scr
