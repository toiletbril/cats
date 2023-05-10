/*
    cats 1.0

    Strips BOMs and carriage returns from files and concatenates them to
    standard output.

    Practically equivalent to "cat <...> | dos2unix", but Windows doesn't have
    that :c.
    (c) toiletbril <https://github.com/toiletbril>
*/
d
#include <ctype.h>
#include <locale.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NAME "cats"
#define VERSION "1.0"
#define GITHUB "<https://github.com/toiletbril>"

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#define DIR_CHAR '\\'
#else
#define DIR_CHAR '/'
#endif

#ifdef __BORLANDC__
#define _setmode setmode
#endif

#define CONTROL_CHARS_LENGTH 32
static const char* control_chars[] = {
    "^@", "^A", "^B", "^C", "^D", "^E", "^F",  "^G", "^H", "^I", "$",
    "^K", "^L", "^M", "^N", "^O", "^P", "^Q",  "^R", "^S", "^T", "^U",
    "^V", "^W", "^X", "^Y", "^Z", "^[", "^\\", "^]", "^^", "^_"};

static const char utf8[] = {0xEF, 0xBB, 0xBF};
static const char utf16be[] = {0xFE, 0xFF};
static const char utf16le[] = {0xFF, 0xFE};

#define BOMS_LENGTH 3
static const int bom_byte_count[] = {3, 2, 2};
static const char* bom_bytes[] = {utf8, utf16be, utf16le};
static const char* bom_names[] = {"UTF-8", "UTF-16BE", "UTF-16LE"};

static char found_bom_name[16];

#define BUFFER_SIZE 1024
#define BUFFER_TYPE _IOFBF

static bool suppress_blank = false;
static bool line_numbers = false;
static bool show_control = false;
static bool use_stdin = false;
static bool unbuffered = false;
static bool verbose = false;

static void usage(void)
{
    printf("Usage: %s [-options] <file> [file2, file3, ...]\n", NAME);
    printf(
        "Concatenate file(s) to standard output, stripping BOMs "
        "and CRs.\n");
#ifdef _WIN32
    printf(
        "\nPlease note that PowerShell adds BOM when "
        "redirecting output,\n"
        "and you should probably use cmd.exe instead. You will still get CRs "
        "that way.\n");
#endif
    printf(
        "\nOptions:\n"
        "\t-v\t\tOutput summary.\n"
        "\t-n\t\tOutput line numbers.\n"
        "\t-A\t\tReplace control characters with their sequences.\n"
        "\t-s\t\tSuppress all blank lines.\n"
        "\t-u\t\tDon't buffer output.\n"
        "\t    --help\tDisplay this message.\n"
        "\t    --version\tDisplay version.\n");

    exit(0);
}

static void put_errno(const char* filename)
{
    char error[128];
    strerror_s(error, 128, errno);

    fprintf(stderr, "%s: %s: %s\n", NAME, filename, error);
    exit(1);
}

static bool bytescmp(char* bytes, size_t bytes_length, const char* bytes2)
{
    for (size_t i = 0; i < bytes_length; ++i) {
        if (bytes[i] != bytes2[i])
            return false;
    }

    return true;
}

static int get_bom_length(char bytes[3])
{
    for (int i = 0; i < BOMS_LENGTH; ++i) {
        if (bytescmp(bytes, bom_byte_count[i], bom_bytes[i])) {
            strcpy_s(found_bom_name, 16, bom_names[i]);
            return bom_byte_count[i];
        }
    }

    return 0;
}

static bool set_flag(const char* str)
{
    if (str[0] != '-')
        return false;

    int len = strlen(str);

    for (int i = 1; i < len; ++i) {
        switch (str[i]) {
            case 'v': {
                verbose = true;
            } break;

            case 'n': {
                line_numbers = true;
            } break;

            case 'A': {
                show_control = true;
            } break;

            case 's': {
                suppress_blank = true;
            } break;

            case 'u': {
                unbuffered = true;
            } break;

            // Long arguments go here.
            case '-': {
                if (strcmp(str, "--help") == 0) {
                    usage();
                    exit(0);
                } else if (strcmp(str, "--version") == 0) {
                    printf(
                        "stripping cat %s\n"
                        "(c) toiletbril %s\n",
                        VERSION, GITHUB);
                    exit(0);
                } else {
                    fprintf(stderr,
                            "%s: Unknown option %s\n"
                            "Try 'cats --help'.\n",
                            NAME, str);
                    exit(1);
                }
            } break;

            default: {
                fprintf(stderr,
                        "%s: Unknown option -%c\n"
                        "Try 'cats --help'.\n",
                        NAME, str[i]);
                exit(1);
            }
        }
    }

    return true;
}

static bool get_control_seq(char* buf, const unsigned char c)
{
    if (c >= CONTROL_CHARS_LENGTH)
        return false;

    strcpy_s(buf, 3, control_chars[c]);

    return true;
}

static void set_binary_mode(FILE* stream)
{
#ifdef _WIN32
    int err = _setmode(_fileno(stream), _O_BINARY);
    if (err == -1) {
        perror("_setmode failed");
    }
#endif
}

static void cats(FILE* f, const char* filename)
{
    static char c = '\0';
    static int current_line = 0;

    static bool prev_is_lf = true;

    char maybe_bom[3];

    // TODO:
    // Undefined behavior when C^C with -i
    // and haven't inputted 4 chars
    fread_s(maybe_bom, 3, sizeof(char), 3, f);
    int bom_len = get_bom_length(maybe_bom);

    // If using stdin and there is no bom,
    // put back characters read in stdout.
    // fseek() does not work for stdin.
    if (use_stdin && !bom_len) {
        if (line_numbers) {
            printf("%6d\t", ++current_line);
        }
        prev_is_lf = false;
        fputs(maybe_bom, stdout);
    }

    // If not using stdin, use fseek().
    else if (!use_stdin) {
        fseek(f, bom_len, SEEK_SET);
    }

    bool found_cr = false;

    while (!feof(f)) {
        if (c == '\n') {
            if (unbuffered)
                fflush(stdout);
            prev_is_lf = true;
        }

        c = fgetc(f);

        if (!found_cr && c == '\r') {
            found_cr = true;
        }

        if (suppress_blank && prev_is_lf && (c == '\r' || c == '\n')) {
            continue;
        }

        if (c == EOF) {
            continue;
        }

        if (line_numbers && prev_is_lf) {
            printf("%6d\t", ++current_line);
        }

        if (iscntrl(c)) {
            // This will replace control characters with their sequences,
            // except \n because it looks better
            if (show_control) {
                char buf[3];
                get_control_seq(buf, c);
                fputs(buf, stdout);

                if (c != '\n') {
                    prev_is_lf = false;
                    continue;
                }
            }

            if (c == '\r') {
                prev_is_lf = false;
                continue;
            }
        } else {
            prev_is_lf = false;
        }

        fputc(c, stdout);
    }

    fflush(stdout);

    // C^C with verbose when using stdin is undefined behavior (?).
    if (verbose) {
        if (!prev_is_lf)
            fputc('\n', stderr);

        fprintf(stderr, "%s: %s: ", NAME, filename);

        if (found_cr)
            fputs("Stripped CRLF line endings", stderr);
        else
            fputs("No CRs found", stderr);

        if (bom_len)
            fprintf(stderr, ", removed %s mark", found_bom_name);
        else
            fputs(", no BOM found", stderr);

        fputs(".\n", stderr);
    }
}

int main(int argv, char** argc)
{
    set_binary_mode(stdout);
    setlocale(LC_ALL, "");

    bool has_files = false;

    for (int i = 1; i < argv; ++i) {
        has_files |= !set_flag(argc[i]);
    }

    if (!has_files)
        use_stdin = true;

    if (!unbuffered) {
        int err = setvbuf(stdout, NULL, BUFFER_TYPE, BUFFER_SIZE);

        if (err) {
            fprintf(stderr,
                    "%s: Could not allocate line buffer of size %d.\nTry "
                    "running with -u option.\n",
                    NAME, BUFFER_SIZE);
            exit(1);
        }
    }

    if (use_stdin) {
        set_binary_mode(stdin);
        cats(stdin, "STDIN");
        return 0;
    }

    for (int i = 1; i < argv; ++i) {
        const char* filename = argc[i];

        if (set_flag(&filename[0])) {
            continue;
        }

        FILE* f;
        errno_t err = fopen_s(&f, filename, "rb");

        if (err) {
            put_errno(filename);
            exit(1);
        }

        cats(f, filename);
        fclose(f);
    }

    return 0;
}
