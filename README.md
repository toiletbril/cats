# cats

Strips BOMs and carriage returns from files and concatenates them to
standard output.

Practically equivalent to `cat <...> | dos2unix`, but Windows doesn't have
that :c.

## Usage

```console
$ cats --help
Usage: cats [-options] <file> [file2, file3, ...]
Concatenate file(s) to standard output, stripping BOMs and CRs.

Please note that PowerShell adds BOM when redirecting output, and you should probably use cmd.exe instead. You will still get CRs that way.

Options:
        -v              Output summary.
        -n              Output line numbers.
        -A              Replace control characters with their sequences.
        -s              Suppress all blank lines.
        -u              Buffer by lines.
            --help      Display this message.
            --version   Display version.
```

```console
$ cat bom.txt | ./readlines.py
['\ufeffhiiiii\n', ':3\n', 'how are you\n', 'this is utf-8 file with crlf and bom :c\n']

$ cats bom.txt -v | ./readlines.py
cats: bom.txt: Stripped CRLF line endings, removed UTF-8 mark.
['hiiiii\n', ':3\n', 'how are you\n', 'this is utf-8 file with crlf and bom :c\n']
```

## Building

```console
cc -O2 -Wall -Wextra -pedantic cats.c -o bin/cats.exe
```
