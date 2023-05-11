@echo off

set CATS=cats.c
set FLAGS=-O2 -Wall -Wextra -pedantic
set OUT=bin

if not exist %OUT% mkdir %OUT%

@echo on
cc %FLAGS% %CATS% -o %OUT%/cats.exe
