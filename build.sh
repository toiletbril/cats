#!/bin/bash

CC=clang
CATS=cats.c
FLAGS="-O2 -Wall -Wextra -pedantic"
OUT=bin

mkdir -p $OUT

set -x

$CC $FLAGS $CATS -o $OUT/cats
