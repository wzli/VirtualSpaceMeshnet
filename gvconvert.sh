#!/bin/sh
format=${1:-svg}
find . -type f -name '*.gv' | xargs -P0 neato -T$format -O
