#!/bin/sh
out_dir=$(dirname $0)/include/vsm
flatc --cpp --gen-mutable --gen-object-api --scoped-enums --gen-name-strings -o $out_dir msg_types.fbs 
