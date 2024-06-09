#!/bin/sh

set -e

XEPHYR=$(command -v Xephyr)

xinit ./xinitrc -- \
	"$XEPHYR" \
	:100 \
	-ac \
	-screen 1920x1080 \
	-host-cursor
