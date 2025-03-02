#!/usr/bin/env sh
set -xe

CFLAGS="-Wall -Wextra -pedantic -std=c11 -ggdb `pkg-config --cflags raylib`"
CLIBS="`pkg-config --libs raylib`"

gcc $CFLAGS -o shogi main.c $CLIBS
