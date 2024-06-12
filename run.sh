#!/bin/sh

set -e

XEPHYR=$(command -v Xephyr) # Absolute path of Xephyr's bin
xinit ./xinitrc -- \
    "$XEPHYR" \
        :100 \
        -ac \
        -screen 1920x1080\
        -host-cursor

