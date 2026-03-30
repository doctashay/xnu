#!/bin/bash

set -euo pipefail

CPU_LIMIT_ARG="cpus=1"
CURRENT_BOOT_ARGS="$(nvram -p 2>/dev/null | awk '$1 == "boot-args" { $1=""; sub(/^ /, ""); print; exit }')"

BOOT_ARGS_VALUE="$(printf '%s\n' "$CURRENT_BOOT_ARGS" | awk -v token="$CPU_LIMIT_ARG" '
    {
        for (i = 1; i <= NF; i++) {
            if ($i != token) {
                args[++count] = $i
            }
        }
    }
    END {
        for (i = 1; i <= count; i++) {
            printf "%s%s", args[i], (i < count ? OFS : "")
        }
    }
')"

echo "Current NVRAM boot settings:"
nvram boot-device boot-file boot-args 2>/dev/null || true
echo

sudo nvram boot-file="" boot-args="$BOOT_ARGS_VALUE"

echo "reset to default boot settings"
nvram boot-file boot-args 2>/dev/null || true
