#!/bin/bash
# lcheck.sh
# checks for license text in source files

LICENSE_TEXT="This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0."
DIR="."
EXCLUDE_FILE="exclude.cloc"

# exclude list
EXCLUDE_LIST=()
if [[ -n "$EXCLUDE_FILE" && -f "$EXCLUDE_FILE" ]]; then
    while IFS= read -r line; do
        [[ -z "$line" || "$line" =~ ^# ]] && continue
        EXCLUDE_LIST+=("${line#./}")
    done < "$EXCLUDE_FILE"
fi

# does file match exclude
is_excluded() {
    local f="$1"
    # make relative to cwd
    local rel="${f#./}"
    for ex in "${EXCLUDE_LIST[@]}"; do
        if [[ "$rel" == "$ex"* ]]; then
            return 0
        fi
    done
    return 1
}

# scan files
find "$DIR" -type f \( -name "*.c" -o -name "*.h" -o -name "*.cpp" -o -name "*.sh" \) | while read -r file; do
    is_excluded "$file" && continue
    if ! grep -qF "$LICENSE_TEXT" "$file"; then
        echo "missing license header: $file"
    fi
done
