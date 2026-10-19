#!/bin/sh
set -eu
ulimit -c 0
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_dir=$(CDPATH= cd -- "$script_dir/../.." && pwd)
mkdir -p "$script_dir/.build"
g++ -std=c++23 -O1 -g -Wall -Wextra -Werror -fno-omit-frame-pointer -no-pie \
    -fsanitize=address,undefined "$script_dir/test_adapter.cpp" -o "$script_dir/.build/test_adapter"
ASAN_OPTIONS=abort_on_error=1:handle_segv=0 timeout 30s "$script_dir/.build/test_adapter"
/opt/devkitpro/devkitA64/bin/aarch64-none-elf-g++ -std=gnu++23 -fsyntax-only \
    -march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIC -fvisibility=hidden \
    -Wall -Werror -fno-rtti -fno-exceptions \
    -D__SWITCH__ -D__RTLD_6XX__ -DEXL_LOAD_KIND=Module -DEXL_LOAD_KIND_ENUM=2 \
    -DEXL_PROGRAM_ID=0x010021C000B6A000 \
    -I/opt/devkitpro/libnx/include -I"$project_dir/native/source" \
    -I"$project_dir/native/include" -I"$project_dir/native/source/lib" \
    "$project_dir/native/source/program/instant_break.cpp"
printf '%s\n' 'PASS: isolated Switch compiler syntax check (no shared build)'
