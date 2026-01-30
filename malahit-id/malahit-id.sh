#!/bin/bash

LOG=/var/log/malahit-ids.txt
ID=`/usr/bin/malahit | grep -oP "STM-ID: \\K(....-....-....-....-....-....)"`

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
