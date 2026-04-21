#!/bin/env bash

TARGET_DIR="${1:-.}"

process_file() {
    FILE="$1"
    TMP_FILE=$(mktemp)

    # convert 4 spaces to tabs
    # strip trailing whitespace
    # CRLF to LF
    # remove trailing empty lines
    # ensure file ends with a single newline
    sed 's/    /\t/g; s/[[:space:]]*$//; s/\r$//' "$FILE" > "$TMP_FILE"

    truncate -s "$(printf "%s" "$(cat "$TMP_FILE")" | wc -c)" "$TMP_FILE"
    echo "" >> "$TMP_FILE"

    if ! cmp -s "$FILE" "$TMP_FILE"; then
        mv "$TMP_FILE" "$FILE"
        echo "updated: $FILE"
    else
        rm -f "$TMP_FILE"
    fi
}

export -f process_file

find "$TARGET_DIR" -type f \( -name "*.c" -o -name "*.h" -o -name "*.S" \) -print0 | \
    xargs -0 -n 1 -P 0 bash -c 'process_file "$0"'
