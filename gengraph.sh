#!/bin/sh
find . -type f -name '*.gv' | xargs -P0 neato -Tsvg -O 
