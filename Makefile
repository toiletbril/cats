.PHONY=main

CC=clang
CATS=cats.c
FLAGS=-O2 -Wall -Wextra -pedantic
OUT=bin/cats.exe

main:
	$(CC) $(FLAGS) $(CATS) -o $(OUT)
