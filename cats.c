/*
    cats 1.5

    Strips BOMs and carriage returns from files and concatenates them to
    standard output.

    Practically equivalent to "cat <...> | dos2unix", but Windows doesn't have
    that :c.
    (c) toiletbril <https://github.com/toiletbril>
*/

#ifdef _WIN32
    #define _CRT_SECURE_NO_WARNINGS
    #include <fcntl.h>
    #include <io.h>
    #define DIR_CHAR '\\'
#else
    #include <unistd.h>
    #define DIR_CHAR '/'
#endif

#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define NAME "cats"
#define VERSION "1.6"
#define GITHUB "<https://github.com/toiletbril>"

#ifdef __BORLANDC__
    #define _setmode setmode
#endif

#define CONTROL_CHARS_LENGTH 32
static const char *control_chars[] = {
    "^@", "^A", "^B", "^C", "^D", "^E", "^F", "^G", "^H", "^I", "$",
    "^K", "^L", "^M", "^N", "^O", "^P", "^Q", "^R", "^S", "^T", "^U",
    "^V", "^W", "^X", "^Y", "^Z", "^[", "^\\", "^]", "^^", "^_"};

static const int bom_byte_count[] = {3, 2, 2};

static const char utf8[]    = {0xEF, 0xBB, 0xBF};
static const char utf16be[] = {0xFE, 0xFF};
static const char utf16le[] = {0xFF, 0xFE};

#define BOMS_LENGTH 3
static const char *bom_bytes[] = {utf8, utf16be, utf16le};
static const char *bom_names[] = {"UTF-8", "UTF-16BE", "UTF-16LE"};

#define BUFFER_SIZE 1024
#define BUFFER_TYPE _IOFBF

static bool suppress_blank = false;
static bool line_numbers   = false;
static bool show_control   = false;
static bool use_stdin      = false;
static bool unbuffered     = false;
static bool verbose        = false;
static bool convert        = false;
static bool overwrite      = false;

static void usage(void)
{
    printf("USAGE: %s [-options] <file> [file2, file3, ...]\n", NAME);
    printf("Concatenate file(s) to standard output, stripping BOMs "
           "and CRs.\n");
#ifdef _WIN32
    printf(
        "\nPlease note that PowerShell adds BOM when "
        "redirecting output,\n"
        "and you should probably use cmd.exe instead. You will still get CRs "
        "that way.\n");
#endif
    printf("\nOPTIONS:\n"
           "  -v              \tOutput summary.\n"
           "  -n              \tOutput line numbers.\n"
           "  -A              \tReplace control characters with their sequences.\n"
           "  -s              \tSuppress all blank lines.\n"
           "  -u              \tDon't buffer output.\n"
           "  -c, --convert   \tConvert UTF-16 to UTF-8.\n"
           "  -o, --overwrite \tDon't output, overwrite files instead.\n"
           "      --help      \tDisplay this message.\n"
           "      --version   \tDisplay version.\n");
    exit(0);
}

void puterror(const char *filename)
{
    fprintf(stderr, "%s: ", NAME);
    perror(filename);
    exit(1);
}

static bool bytescmp(char *bytes, size_t bytes_length, const char *bytes2)
{
    for (size_t i = 0; i < bytes_length; ++i) {
        if (bytes[i] != bytes2[i])
            return false;
    }

    return true;
}

static int get_bom_length(char bytes[3], int *bom_index)
{
    for (int i = 0; i < BOMS_LENGTH; ++i) {
        if (bytescmp(bytes, bom_byte_count[i], bom_bytes[i])) {
            *bom_index = i;
            return bom_byte_count[i];
        }
    }

    return 0;
}

static bool set_flag(const char *str)
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

            case 'c': {
                convert = true;
            } break;

            case 'o': {
                overwrite = true;
            } break;

            // Long arguments go here.
            case '-': {
                if (strcmp(str, "--help") == 0) {
                    usage();
                    exit(0);
                }
                if (strcmp(str, "--convert") == 0) {
                    convert = true;
                    return true;
                }
                if (strcmp(str, "--overwrite") == 0) {
                    overwrite = true;
                    return true;
                }
                else if (strcmp(str, "--version") == 0) {
                    printf("stripping cat %s\n"
                           "(c) toiletbril %s\n",
                           VERSION, GITHUB);
                    exit(0);
                }
                else {
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

static bool get_control_seq(char *buf, const unsigned char c)
{
    if (c >= CONTROL_CHARS_LENGTH)
        return false;

    strcpy(buf, control_chars[c]);

    return true;
}

static void set_binary_mode(FILE *stream)
{
#ifdef _WIN32
    int err = _setmode(_fileno(stream), _O_BINARY);
    if (err == -1) {
        puterror("_setmode failed");
    }
#else
    (void)stream;
#endif
}

static int peek_bom(FILE *f, char *buf)
{
    char maybe_bom[3];
    int bom_index = -1;

    fread(maybe_bom, sizeof(char), 3, f);
    int bom_len = get_bom_length(maybe_bom, &bom_index);

    // This is a hack since STDIN can not be ungetc'd
    int j = 0;
    for (int i = bom_len; i < 3; ++i) {
        buf[j++] = maybe_bom[i];
    }
    buf[3] = '\0';

    return bom_index;
}

static void utf8_from_utf16(FILE *utf16_file, const char *filename, FILE *output, bool be)
{
    if (utf16_file == NULL) {
        puterror(filename);
    }

    if (output == NULL) {
        puterror("*output");
    }

    char buffer[2];
    int codepoint;

    while (fread(buffer, 2, 1, utf16_file) == 1) {
        if (be) {
            codepoint = (buffer[1] << 8) | buffer[0];
        } else {
            codepoint = (buffer[0] << 8) | buffer[1];
        }

        if (codepoint == 0x000D)
            continue;

        if (codepoint < 0x80) {
            fputc(codepoint, output);
        }
        else if (codepoint < 0x800) {
            fputc(0xC0 | (codepoint >> 6), output);
            fputc(0x80 | (codepoint & 0x3F), output);
        }
        else {
            fputc(0xE0 | (codepoint >> 12), output);
            fputc(0x80 | ((codepoint >> 6) & 0x3F), output);
            fputc(0x80 | (codepoint & 0x3F), output);
        }
    }

    if (buffer[1] != 0x000a)
        fputc(0x000a, output);
}

static void cats(FILE *f, const char *filename, const char* bom_buf, int bom, FILE* out)
{
    FILE *buf_file = tmpfile();

    if (buf_file == NULL)
        puterror("buf_file");

    fputs(bom_buf, buf_file);
    rewind(buf_file);

    bool buf_end = false;

    static char c           = '\0';
    static int current_line = 0;

    static bool prev_is_lf = true;

    bool found_cr = false;

    while (true) {
        if (c == '\n') {
            if (unbuffered)
                fflush(stdout);
            prev_is_lf = true;
        }

        if (!buf_end) {
            c = fgetc(buf_file);
            buf_end = c == EOF;
        }

        if (buf_end) {
            c = fgetc(f);
        }

        if (c == EOF) {
            break;
        }

        if (!found_cr && c == '\r') {
            found_cr = true;
        }

        if (suppress_blank && prev_is_lf && (c == '\r' || c == '\n')) {
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
        }
        else {
            prev_is_lf = false;
        }

        fputc(c, out);
    }

    fflush(out);

    // C^C with verbose when using stdin is undefined behavior (?).
    if (verbose) {
        if (!prev_is_lf && out == stdout)
            fputc('\n', stderr);

        fprintf(stderr, "%s: %s: ", NAME, filename);

        if (found_cr)
            fputs("Stripped CRs from line ends", stderr);
        else
            fputs("No CRs found", stderr);

        if (bom != -1) {
            fprintf(stderr, ", removed %s mark", bom_names[bom]);
            if (convert && bom > 0)
                fprintf(stderr, ", converted to UTF-8");
        }
        else
            fputs(", no BOM found", stderr);
        if (overwrite)
            fprintf(stderr, ", overwrote file");

        fputs(".\n", stderr);
    }
}

static void copy_file(FILE *source, FILE *dest)
{
    char buffer[1024];
    size_t read_bytes;
    while ((read_bytes = fread(buffer, 1, sizeof(buffer), source)) > 0) {
        fwrite(buffer, 1, read_bytes, dest);
    }
}

static void catstemp(const char *filename, size_t size, char *buf)
{
    memset(buf, 0, size);
    strncat(buf, filename, size);
    strncat(buf, ".catstemp", size);

    if (strncmp(filename, buf, size) == 0) {
        fprintf(stderr, "%s: Filename is too long or there is already a .catstemp file.", NAME);
        exit(1);
    }
}

static void handle_sigint(int sig_n)
{
    signal(sig_n, SIG_IGN);
    fputc('\n', stdout);
    if (verbose)
        fprintf(stderr, "%s: Interrupted.\n", NAME);
    fflush(stdout);
    exit(0);
}

int main(int argv, char **argc)
{
    set_binary_mode(stdout);
    setlocale(LC_ALL, "");
    signal(SIGINT, handle_sigint);

    bool has_files = false;

    for (int i = 1; i < argv; ++i) {
        has_files |= !set_flag(argc[i]);
    }

    if (!has_files)
        use_stdin = true;

    if (!unbuffered && !use_stdin) {
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
        if (overwrite) {
            fprintf(stderr, "%s: Can not overwrite STDIN", NAME);
            exit(1);
        }

        char buf[4] = { '\0' };
        set_binary_mode(stdin);

        cats(stdin, "STDIN", buf, peek_bom(stdin, buf), stdout);
        return 0;
    }

    for (int i = 1; i < argv; ++i) {
        const char *filename = argc[i];

        if (set_flag(&filename[0])) {
            continue;
        }

        struct stat stbuf;
        stat(filename, &stbuf);

        if ((stbuf.st_mode & S_IFMT) == S_IFDIR) {
            fprintf(stderr, "%s: %s: Is a directory\n", NAME, filename);
            exit(1);
        }

        FILE *file = fopen(filename, "rb");
        if (file == NULL) {
            puterror(filename);
        }

        char buf[4] = { '\0' };
        int bom = peek_bom(file, buf);

        if ((convert && bom > 0) || overwrite) {
            char temp_filename[256];
            catstemp(filename, 256, temp_filename);

            if (strcmp(temp_filename, filename) == 0) {
                fprintf(stderr, "%s: Error converting UTF16 to UTF8: Filename is too long", NAME);
                fclose(file);
                exit(1);
            }

            FILE *new_file = fopen(temp_filename, "wb");
            if (new_file == NULL) {
                puterror(temp_filename);
            }

            if (convert && bom > 0) {
                utf8_from_utf16(file, filename, new_file, bom == 1);
            } else {
                copy_file(file, new_file);
            }

            freopen(temp_filename, "rb", new_file);
            if (new_file == NULL) {
                puterror(temp_filename);
            }

            if (overwrite) {
                freopen(filename, "wb", file);

                if (new_file == NULL) {
                    puterror(temp_filename);
                }

                cats(new_file, filename, buf, bom, file);
            } else {
                cats(new_file, filename, buf, bom, stdout);
            }

            fclose(new_file);
            remove(temp_filename);
            fclose(file);
        }
        else {
            cats(file, filename, buf, bom, stdout);
            fclose(file);
        }
    }

    return 0;
}
