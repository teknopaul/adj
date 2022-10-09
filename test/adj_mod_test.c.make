#!/bin/bash

cd $(dirname $0)

#prof="-fprofile-arcs -ftest-coverage"

test=adj_mod_test

gcc $prof -Wall -Werror -Wno-unused-function -g -O0 -I/usr/include/libusb-1.0 \
    $test.c -lusb-1.0 \
    -o $test \
    && ./$test \
    && rm $test \
    && rm $test.c
