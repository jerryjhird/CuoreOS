#!/bin/bash
# colorpicker.sh - return cuoreterm hex ANSI color code thing fuck we need to name this

if [ "$#" -ne 3 ]; then
    echo "usage: $0 <red> <green> <blue>\n reset: \x1b[0m"
    exit 1
fi

printf '\\x1b[#%02X%02X%02Xm\n' "$1" "$2" "$3"