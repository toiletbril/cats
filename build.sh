#!/bin/bash

CC=clang
CATS=cats.c
FLAGS="-O2 -Wall -Wextra -pedantic -std=c17"
OUT=bin

mkdir -p $OUT

set -x

$CC $FLAGS $CATS -o $OUT/cats
