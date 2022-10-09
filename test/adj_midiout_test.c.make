#!/bin/bash

cd $(dirname $0)

#prof="-fprofile-arcs -ftest-coverage"

test=adj_midiout_test

gcc $prof -Wall -Werror -Wno-unused-function -g -O0 \
    $test.c \
    -o $test  -lasound \
    && ./$test \
    && rm $test \
    && rm $test.c
