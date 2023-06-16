# cats

Strips BOMs and carriage returns from files and concatenates them to
standard output.

Almost equivalent to `cat <...> | dos2unix`, but Windows doesn't have
that :c.

Strips UTF-16 BOMs as well, does not convert them to UTF-8 without `-c` flag.

## Usage

```console
$ cats --help
USAGE: cats [-options] <file> [file2, file3, ...]
Concatenate file(s) to standard output, stripping BOMs and CRs.

Please note that PowerShell adds BOM when redirecting output,
and you should probably use cmd.exe instead. You will still get CRs that way.

OPTIONS:
  -v              	Output summary.
  -n              	Output line numbers.
  -A              	Replace control characters with their sequences.
  -s              	Suppress all blank lines.
  -u              	Don't buffer output.
  -c, --convert   	Convert UTF-16 to UTF-8.
  -o, --overwrite 	Don't output, overwrite files instead.
      --help      	Display this message.
      --version   	Display version.
```

```console
$ cat bom.txt | ./readlines.py
['\ufeffhiiiii\n', ':3\n', 'how are you\n', 'this is utf-8 file with crlf and bom :c\n']

$ cats bom.txt -v | ./readlines.py
cats: bom.txt: Stripped CRs from line ends, removed UTF-8 mark.
['hiiiii\n', ':3\n', 'how are you\n', 'this is utf-8 file with crlf and bom :c\n']
```

## Building

To build with Clang, use `build.sh` (POSIX) or `build.bat` (Windows).
Look for executable in `bin/` afterwards.
