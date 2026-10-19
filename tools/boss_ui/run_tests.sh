#!/bin/sh
set -eu
ulimit -c 0
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
project_dir=$(CDPATH= cd -- "$script_dir/../.." && pwd)
mkdir -p "$script_dir/.build"
g++ -std=c++23 -O1 -g -Wall -Wextra -Werror -fno-omit-frame-pointer -no-pie \
    -fsanitize=address,undefined -I"$project_dir/native/source" \
    "$script_dir/test_renderer.cpp" -o "$script_dir/.build/test_renderer"
ASAN_OPTIONS=abort_on_error=1:handle_segv=0 timeout 20s "$script_dir/.build/test_renderer"
