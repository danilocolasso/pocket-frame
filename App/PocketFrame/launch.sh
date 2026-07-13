#!/bin/sh
progdir=$(dirname "$0")
cd "$progdir" || exit 1
export PATH="/mnt/SDCARD/.tmp_update/bin:$PATH"
export LD_LIBRARY_PATH="/mnt/SDCARD/miyoo/lib:$LD_LIBRARY_PATH"
touch /tmp/stay_awake   # Onion's keymon skips sleep/auto-shutdown while this exists
./pocket-frame > ./pocket-frame.log 2>&1
rm -f /tmp/stay_awake
