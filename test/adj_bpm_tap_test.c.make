#!/bin/bash

cd $(dirname $0)

#prof="-fprofile-arcs -ftest-coverage"

test=adj_bpm_tap_test

gcc $prof -Wall -Werror -Wno-unused-function -g -O0 \
    $test.c \
    -o $test \
    && ./$test \
    && rm $test \
    && rm $test.c
