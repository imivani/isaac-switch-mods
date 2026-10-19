#!/bin/sh
set -eu
ulimit -c 0
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$script_dir/.build"
g++ -std=c++17 -O2 -Wall -Wextra -Wpedantic -Werror "$script_dir/test_state.cpp" -o "$script_dir/.build/test_state_plain"
timeout 15s "$script_dir/.build/test_state_plain"
g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror -fsanitize=undefined -fno-sanitize-recover=all "$script_dir/test_state.cpp" -o "$script_dir/.build/test_state_ubsan"
timeout 15s "$script_dir/.build/test_state_ubsan"
if [ "${1:-}" = "--asan" ]; then
    # A non-PIE binary avoids the WSL image's observed intermittent ASan startup
    # signal loop. Bound both run time and output if its runtime still fails.
    g++ -std=c++17 -O1 -g -Wall -Wextra -Wpedantic -Werror -no-pie -fsanitize=address,undefined -fno-omit-frame-pointer "$script_dir/test_state.cpp" -o "$script_dir/.build/test_state_asan"
    result=0
    (ulimit -f 128; ASAN_OPTIONS=abort_on_error=1:handle_segv=0 timeout 15s "$script_dir/.build/test_state_asan") > "$script_dir/.build/asan.log" 2>&1 || result=$?
    tail -n 20 "$script_dir/.build/asan.log"
    exit "$result"
fi
