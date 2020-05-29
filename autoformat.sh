#!/bin/sh
find . -type f -name '*.h' -o -iname '*.c' -o -iname '*.cpp' -o -iname '*.hpp' \
    | grep -v -e build/ -e submodules/ -e _generated.h \
    | xargs -P0 clang-format --verbose -style=file -i -fallback-style=none
