#!/bin/sh

set -u
set -e

if [ -e ${TARGET_DIR}/etc/inittab ]; then
    grep -qE '^tty1::' ${TARGET_DIR}/etc/inittab || \
	sed -i '/GENERIC_SERIAL/a\
tty1::respawn:/usr/bin/env SDL_VIDEODRIVER=kmsdrm /usr/bin/typewrite # Typewrite typewriter' ${TARGET_DIR}/etc/inittab
fi
