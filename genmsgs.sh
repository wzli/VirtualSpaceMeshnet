#!/bin/sh
out_dir=$(dirname $0)/include/vsm
flatc --cpp --gen-mutable --scoped-enums --gen-name-strings -o $out_dir msg_types.fbs 
