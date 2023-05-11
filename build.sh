#!/bin/bash

CATS=cats.c
FLAGS="-O2 -Wall -Wextra -pedantic"
OUT=bin

mkdir -p $OUT

set -x

cc $FLAGS $CATS -o $OUT/cats
