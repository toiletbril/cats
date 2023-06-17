@echo off

set CATS=cats.c
set FLAGS=-O2 -Wall -Wextra /std:c17 -Wno-declaration-after-statement -Wno-unreachable-code-break
set OUT=bin

if not exist %OUT% mkdir %OUT%

@echo on
clang-cl %FLAGS% %CATS% /MD -o %OUT%/cats.exe
