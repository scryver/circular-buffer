#!/bin/bash

code="$PWD"
opts="-O0 -g -ggdb -Wall -Werror -pedantic -Wno-gnu-zero-variadic-macro-arguments -Wno-gnu-anonymous-struct -Wno-nested-anon-types -Wno-missing-braces -Wno-unused-function -Wno-c99-extensions -Wno-writable-strings"

echo "Building circular buffer..."

mkdir -p gebouw

cd gebouw > /dev/null

    g++ -DLINUX_BUILD=1 $opts $code/src/circular.cpp -o circular

cd $code > /dev/null
