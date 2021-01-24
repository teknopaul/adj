#!/bin/bash

cd $(dirname $0)

#prof="-fprofile-arcs -ftest-coverage"

test=adj_vdj_test

gcc $prof -Wall -Werror -Wno-unused-function -g -O0 -I../src ../target/tui.o \
    $test.c \
    -lcdj -lpthread \
    -o $test \
    && ./$test \
    && rm $test \
    && rm $test.c
