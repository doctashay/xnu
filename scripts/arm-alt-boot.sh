#!/bin/bash

set -euo pipefail

ALT_KERNEL_NAME="${1:-mach_kernel.test}"
ROOT_DEV="$(mount | awk '$3 == "/" { print $1; exit }')"
CPU_LIMIT_ARG="cpus=1"

if [[ -z "$ROOT_DEV" ]]; then
    echo "Could not determine root device" >&2
    exit 1
fi

ROOT_SLICE="$(echo "$ROOT_DEV" | sed -E 's#^/dev/disk[0-9]+s([0-9]+)$#\1#')"
if [[ -z "$ROOT_SLICE" || "$ROOT_SLICE" == "$ROOT_DEV" ]]; then
    echo "Could not parse root slice from $ROOT_DEV" >&2
    exit 1
fi

BOOT_FILE_VALUE="hd:${ROOT_SLICE},${ALT_KERNEL_NAME}"
CURRENT_BOOT_ARGS="$(nvram -p 2>/dev/null | awk '$1 == "boot-args" { $1=""; sub(/^ /, ""); print; exit }')"

if [[ -n "$CURRENT_BOOT_ARGS" ]]; then
    BOOT_ARGS_VALUE="$(printf '%s\n' "$CURRENT_BOOT_ARGS" | awk -v token="$CPU_LIMIT_ARG" '
        {
            for (i = 1; i <= NF; i++) {
                if ($i != token) {
                    args[++count] = $i
                }
            }
        }
        END {
            args[++count] = token
            for (i = 1; i <= count; i++) {
                printf "%s%s", args[i], (i < count ? OFS : "")
            }
        }
    ')"
else
    BOOT_ARGS_VALUE="$CPU_LIMIT_ARG"
fi

echo "Root device:   $ROOT_DEV"
echo "Boot setting:  $BOOT_FILE_VALUE"
echo "Boot args:     $BOOT_ARGS_VALUE"

echo "Current NVRAM boot settings:"
nvram boot-device boot-file boot-args 2>/dev/null || true
echo

sudo nvram boot-file="$BOOT_FILE_VALUE" boot-args="$BOOT_ARGS_VALUE"

echo
echo "new boot settings armed"
nvram boot-file boot-args
echo