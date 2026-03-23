#!/bin/bash

LOG=/var/log/malahit-ids.txt
ID=`/usr/bin/malahit | \
  grep -o "STM-ID: \\K(....-....-....-....-....-....)" | \
  sed -re "s/STM-ID: (....)-(....)-(....)-(....)-(....)-(....)/{ 0x\1, 0x\2, 0x\3, 0x\4, 0x\5, 0x\6 },/"`

# If we have got an ID....
if [ ! -z "$ID" ]; then
    # Create IDs log file if missing
    if [ ! -f "$LOG" ]; then
        touch "$LOG"
    fi

    # Check if we've already seen this ID
    if ! grep -q "$ID" "$LOG"; then
        echo "$ID" >> "$LOG"
    fi
fi
