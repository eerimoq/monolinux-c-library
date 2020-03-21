/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019, Erik Moqvist
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * This file is part of the Monolinux C library project.
 */

/* Needed by ftw. */
#define _XOPEN_SOURCE 700
#define _DEFAULT_SOURCE

#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/sysmacros.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/reboot.h>
#include <sys/time.h>
#include <ctype.h>
#include <termios.h>
#include <ftw.h>
#include <unistd.h>
#include <dirent.h>
#include <mntent.h>
#include "ml/ml.h"
#include "internal.h"

#define PROMPT                                  "$ "

#define COMMAND_MAX                              256

#define MAXIMUM_HISTORY_LENGTH                    64

#define TAB                                     '\t'
#define CARRIAGE_RETURN                         '\r'
#define NEWLINE                                 '\n'
#define BACKSPACE                                  8
#define DELETE                                   127
#define CTRL_A                                     1
#define CTRL_E                                     5
#define CTRL_D                                     4
#define CTRL_K                                    11
#define CTRL_T                                    20
#define CTRL_R                                    18
#define CTRL_G                                     7
#define ALT                                       27

struct command_t {
    const char *name_p;
    const char *description_p;
    ml_shell_command_callback_t callback;
};

struct history_elem_t {
    struct history_elem_t *next_p;
    struct history_elem_t *prev_p;
    char buf[1];
};

struct line_t {
    char buf[COMMAND_MAX];
    int length;
    int cursor;
};

struct module_t {
    struct line_t line;
    struct line_t prev_line;
    bool carriage_return_received;
    bool newline_received;
    struct {
        struct history_elem_t *head_p;
        struct history_elem_t *tail_p;
        int length;
        struct history_elem_t *current_p;
        struct line_t pattern;
        struct line_t match;
        /* Command line when first UP was pressed. */
        struct line_t line;
        bool line_valid;
    } history;
    int number_of_commands;
    struct command_t *commands_p;
    pthread_t pthread;
};

static struct module_t module;

static void history_init(void);
static void show_line(void);

static int xgetc(void)
{
    int ch;

    ch = fgetc(stdin);

    if (ch == EOF) {
        printf("error: shell input error");
        exit(1);
    }

    return (ch);
}

#ifndef UNIT_TEST

static void make_stdin_unbuffered(void)
{
    struct termios ctrl;

    tcgetattr(STDIN_FILENO, &ctrl);
    ctrl.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &ctrl);
}

#else

static void make_stdin_unbuffered(void)
{
}

#endif

static void print_prompt(void)
{
    printf(PROMPT);
    fflush(stdout);
}

static int compare_bsearch(const void *key_p, const void *elem_p)
{
    const char *name_p;
    const char *elem_name_p;

    name_p = (const char *)key_p;
    elem_name_p = ((struct command_t *)elem_p)->name_p;

    return (strcmp(name_p, elem_name_p));
}

static int compare_qsort(const void *lelem_p, const void *relem_p)
{
    const char *lname_p;
    const char *rname_p;
    int res;

    lname_p = ((struct command_t *)lelem_p)->name_p;
    rname_p = ((struct command_t *)relem_p)->name_p;

    res = strcmp(lname_p, rname_p);

    if (res == 0) {
        printf("%s: shell commands must be unique", lname_p);
        exit(1);
    }

    return (res);
}

static struct command_t *find_command(const char *name_p)
{
    return (bsearch(name_p,
                    module.commands_p,
                    module.number_of_commands,
                    sizeof(*module.commands_p),
                    compare_bsearch));
}

/**
 * Parse one argument from given string. An argument must be in quotes
 * if it contains spaces.
 */
static char *parse_argument(char *line_p, const char **begin_pp)
{
    bool in_quote;

    in_quote = false;
    *begin_pp = line_p;

    while (*line_p != '\0') {
        if (*line_p == '\\') {
            if (line_p[1] == '\"') {
                /* Remove the \. */
                memmove(line_p, &line_p[1], strlen(&line_p[1]) + 1);
            }
        } else {
            if (in_quote) {
                if (*line_p == '\"') {
                    /* Remove the ". */
                    memmove(line_p, &line_p[1], strlen(&line_p[1]) + 1);
                    in_quote = false;
                    line_p--;
                }
            } else {
                if (*line_p == '\"') {
                    /* Remove the ". */
                    memmove(line_p, &line_p[1], strlen(&line_p[1]) + 1);
                    in_quote = true;
                    line_p--;
                } else if (*line_p == ' ') {
                    *line_p = '\0';
                    line_p++;
                    break;
                }
            }
        }

        line_p++;
    }

    if (in_quote) {
        line_p =  NULL;
    }

    return (line_p);
}

static int parse_command(char *line_p, const char *argv[])
{
    int argc;

    /* Remove white spaces at the beginning and end of the string. */
    line_p = ml_strip(line_p, NULL);
    argc = 0;

    /* Command string missing. */
    if (strlen(line_p) == 0) {
        return (-1);
    }

    while (*line_p != '\0') {
        /* Too many arguemnts? */
        if (argc == 32) {
            return (-1);
        }

        /* Remove white spaces before the next argument. */
        line_p = ml_strip(line_p, NULL);

        if ((line_p = parse_argument(line_p, &argv[argc++])) == NULL) {
            return (-1);
        }
    }

    return (argc);
}

static int execute_command(char *line_p)
{
    struct command_t *command_p;
    const char *name_p;
    int res;
    const char *argv[32];
    int argc;

    argc = parse_command(line_p, &argv[0]);

    if (argc < 1) {
        return (-1);
    }

    name_p = argv[0];

    if (name_p == NULL) {
        name_p = "";
    }

    command_p = find_command(name_p);

    if (command_p != NULL) {
        res = command_p->callback(argc, &argv[0]);
    } else {
        printf("%s: command not found\n", name_p);
        res = -EINVAL;
    }

    return (res);
}

static bool is_comment(const char *line_p)
{
    return (*line_p == '#');
}

static bool is_exit(const char *line_p)
{
    return (strncmp(line_p, "exit", 5) == 0);
}

static void line_init(struct line_t *self_p)
{
    self_p->buf[0] = '\0';
    self_p->cursor = 0;
    self_p->length = 0;
}

static bool line_insert(struct line_t *self_p,
                        int ch)
{
    /* Buffer full? */
    if (self_p->length == COMMAND_MAX - 1) {
        return (false);
    }

    /* Move the string, including the NULL termination, one step to
       the right and insert the new character. */
    memmove(&self_p->buf[self_p->cursor + 1],
            &self_p->buf[self_p->cursor],
            self_p->length - self_p->cursor + 1);
    self_p->buf[self_p->cursor++] = ch;
    self_p->length++;

    return (true);
}

static void line_insert_string(struct line_t *self_p,
                               char *str_p)
{
    while (*str_p != '\0') {
        if (!line_insert(self_p, *str_p)) {
            break;
        }

        str_p++;
    }
}

static void line_delete(struct line_t *self_p)
{
    /* End of buffer? */
    if (self_p->cursor == self_p->length) {
        return;
    }

    /* Move the string, including the NULL termination, one step to
       the left to overwrite the deleted character. */
    memmove(&self_p->buf[self_p->cursor],
            &self_p->buf[self_p->cursor + 1],
            self_p->length - self_p->cursor);
    self_p->length--;
}

static int line_peek(struct line_t *self_p)
{
    return (self_p->buf[self_p->cursor]);
}

static void line_truncate(struct line_t *self_p)
{
    self_p->length = self_p->cursor;
    self_p->buf[self_p->length] = '\0';
}

static bool line_is_empty(struct line_t *self_p)
{
    return (self_p->length == 0);
}

static char *line_get_buf(struct line_t *self_p)
{
    return (self_p->buf);
}

static int line_get_length(struct line_t *self_p)
{
    return (self_p->length);
}

static bool line_seek(struct line_t *self_p, int pos)
{
    if (pos < 0) {
        if ((self_p->cursor + pos) < 0) {
            return (false);
        }
    } else {
        if ((self_p->cursor + pos) > self_p->length) {
            return (false);
        }
    }

    self_p->cursor += pos;

    return (true);
}

static int line_get_cursor(struct line_t *self_p)
{
    return (self_p->cursor);
}

static void line_seek_begin(struct line_t *self_p)
{
    self_p->cursor = 0;
}

static void line_seek_end(struct line_t *self_p)
{
    self_p->cursor = self_p->length;
}

static int command_help(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;

    int i;

    printf("Cursor movement\n"
           "\n"
           "         LEFT   Go left one character.\n"
           "        RIGHT   Go right one character.\n"
           "  HOME/Ctrl+A   Go to the beginning of the line.\n"
           "   END/Ctrl+E   Go to the end of the line.\n"
           "\n"
           "Edit\n"
           "\n"
           "        Alt+D   Delete the word at the cursor.\n"
           "       Ctrl+D   Delete the chracter at the cursor.\n"
           "       Ctrl+K   Cut the line from cursor to end.\n"
           "       Ctrl+T   Swap the last two characters before the cursor "
           "(typo).\n"
           "          TAB   Tab completion for command names.\n"
           "    BACKSPACE   Delete the character before the cursor.\n"
           "\n"
           "History\n"
           "\n"
           "           UP   Previous command.\n"
           "         DOWN   Next command.\n"
           "       Ctrl+R   Recall the last command including the specified "
           "character(s)\n"
           "                searches the command history as you type.\n"
           "       Ctrl+G   Escape from history searching mode.\n"
           "\n"
           "Commands\n"
           "\n");

    for (i = 0; i < module.number_of_commands; i++) {
        printf("%13s   %s\n",
               module.commands_p[i].name_p,
               module.commands_p[i].description_p);
    }

    return (0);
}

static int command_history(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;

    struct history_elem_t *current_p;
    int i;

    current_p = module.history.head_p;
    i = 1;

    while (current_p != NULL) {
        printf("%d: %s\n", i, current_p->buf);
        current_p = current_p->next_p;
        i++;
    }

    return (0);
}

static int command_exit(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;

    return (0);
}

static int command_suicide(int argc, const char *argv[])
{
    int res;
    uint8_t *null_p;

    res = -EINVAL;
    null_p = NULL;

    if (argc == 2) {
        if (strcmp(argv[1], "exit") == 0) {
            exit(1);
        } else if (strcmp(argv[1], "segfault") == 0) {
            *null_p = 0;
        }
    }

    if (res != 0) {
        printf("Usage: suicide {exit,segfault}\n");
    }

    return (res);
}

static int command_ls(int argc, const char *argv[])
{
    int res;
    DIR *dir_p;
    struct dirent *dirent_p;
    const char *path_p;
    struct stat statbuf;
    char buf[512];

    res = 0;

    if (argc == 2) {
        path_p = argv[1];
    } else {
        path_p = ".";
    }

    dir_p = opendir(path_p);

    if (dir_p != NULL) {
        while ((dirent_p = readdir(dir_p)) != NULL) {
            snprintf(&buf[0], sizeof(buf), "%s/%s", path_p, dirent_p->d_name);

            res = stat(&buf[0], &statbuf);

            if (res != 0) {
                break;
            }

            if (S_ISCHR(statbuf.st_mode)) {
                printf("c ");
            } else if (S_ISBLK(statbuf.st_mode)) {
                printf("b ");
            } else if (S_ISDIR(statbuf.st_mode)) {
                printf("d ");
            } else {
                printf("- ");
            }

            if (S_ISCHR(statbuf.st_mode) || S_ISBLK(statbuf.st_mode)) {
                printf("%3d, %3d ", major(statbuf.st_rdev), minor(statbuf.st_rdev));
            }

            if (S_ISDIR(statbuf.st_mode)) {
                printf("%s/\n", dirent_p->d_name);
            } else {
                puts(dirent_p->d_name);
            }
        }

        closedir(dir_p);
    } else {
        res = -EGENERAL;
    }

    return (res);
}

static int command_cat(int argc, const char *argv[])
{
    FILE *file_p;
    int res;
    uint8_t buf[256];
    size_t size;

    res = 0;

    if (argc != 2) {
        printf("Usage: cat <file>\n");

        return (-EINVAL);
    }

    file_p = fopen(argv[1], "rb");

    if (file_p != NULL) {
        while ((size = fread(&buf[0], 1, membersof(buf), file_p)) > 0) {
            if (fwrite(&buf[0], 1, size, stdout) != size) {
                res = -errno;
                break;
            }
        }

        fclose(file_p);
    } else {
        res = -errno;
    }

    return (res);
}

static int hexdump(const char *name_p, size_t offset, ssize_t size)
{
    int res;
    FILE *file_p;

    res = 0;
    file_p = fopen(name_p, "rb");

    if (file_p != NULL) {
        res = ml_hexdump_file(file_p, offset, size);
        fclose(file_p);
    } else {
        res = -EGENERAL;
    }

    return (res);
}

static int command_hexdump(int argc, const char *argv[])
{
    int res;
    ssize_t offset;
    ssize_t size;

    res = -EINVAL;

    if (argc == 2) {
        res = hexdump(argv[1], 0, -1);
    } else if (argc == 3) {
        size = atoi(argv[1]);

        if (size >= 0) {
            res = hexdump(argv[2], 0, size);
        } else {
            res = -EINVAL;
        }
    } else if (argc == 4) {
        offset = atoi(argv[1]);
        size = atoi(argv[2]);

        if ((offset >= 0 ) && (size >= 0)) {
            res = hexdump(argv[3], offset, size);
        } else {
            res = -EINVAL;
        }
    }

    if (res != 0) {
        printf("Usage: hexdump [[<offset>] <size>] <file>\n");
    }

    return (res);
}

static int command_reboot(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;

    return (reboot(RB_AUTOBOOT));
}

static int command_insmod(int argc, const char *argv[])
{
    int res;

    res = -EINVAL;

    if (argc == 2) {
        res = ml_insert_module(argv[1], "");
    } else if (argc == 3) {
        res = ml_insert_module(argv[1], argv[2]);
    }

    if (res != 0) {
        printf("Usage: insmod <file> [<params>]\n");
    }

    return (res);
}

static int command_mknod(int argc, const char *argv[])
{
    int res;
    mode_t mode;
    dev_t dev;

    res = -EINVAL;
    mode = 0666;

    if (argc == 3) {
        if (strcmp(argv[2], "p") == 0) {
            res = ml_mknod(argv[1], S_IFIFO | mode, 0);
        }
    } else if (argc == 5) {
        dev = makedev(atoi(argv[3]), atoi(argv[4]));

        if (strcmp(argv[2], "c") == 0) {
            res = ml_mknod(argv[1], S_IFCHR | mode, dev);
        } else if (strcmp(argv[2], "b") == 0) {
            res = ml_mknod(argv[1], S_IFBLK | mode, dev);
        }
    }

    if (res != 0) {
        printf("Usage: mknod <path> <type> [<major>] [<minor>]\n");
    }

    return (res);
}

static int command_mount(int argc, const char *argv[])
{
    int res;

    res = -EINVAL;

    if (argc == 4) {
        res = ml_mount(argv[1], argv[2], argv[3], 0, NULL);
    } else if (argc == 5) {
        res = ml_mount(argv[1], argv[2], argv[3], 0, argv[4]);
    }

    if (res != 0) {
        printf("Usage: mount [<device> <dir> <type> [<options>]]\n");
    }

    return (res);
}

static int command_df(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;

    return (ml_print_file_systems_space_usage());
}

static int print_info(const char *fpath_p,
                      const struct stat *stat_p,
                      const int tflag,
                      struct FTW *ftwbuf_p)
{
    (void)tflag;
    (void)ftwbuf_p;

    if (S_ISDIR(stat_p->st_mode)) {
        printf("%s/\n", fpath_p);
    } else {
        puts(fpath_p);
    }

    return (0);
}

static int command_find(int argc, const char *argv[])
{
    int res;

    res = -EINVAL;

    if (argc == 1) {
        res = nftw(".", print_info, 20, FTW_PHYS);
    } else if (argc == 2) {
        res = nftw(argv[1], print_info, 20, FTW_PHYS);
    }

    if (res != 0) {
        printf("Usage: find [<path>]\n");
    }

    return (res);
}

static int command_date(int argc, const char *argv[])
{
    int res;
    time_t now;
    struct timespec ts;

    res = -EINVAL;

    if (argc == 1) {
        now = time(NULL);

        if (now != (time_t)(-1)) {
            printf("%s", asctime(gmtime(&now)));
            res = 0;
        }
    } else if (argc == 2) {
        ts.tv_sec = atoi(argv[1]);
        ts.tv_nsec = 0;
        res = clock_settime(CLOCK_REALTIME, &ts);

        if (res == -1) {
            res = -errno;
        }
    } else {
        printf("Usage: date [<unix-time>]\n");
    }

    return (res);
}

static int command_print(int argc, const char *argv[])
{
    FILE *file_p;
    int res;
    size_t length;

    res = 0;

    if (argc != 3) {
        printf("Usage: print <text> <file>\n");

        return (-EINVAL);
    }

    file_p = fopen(argv[2], "w");

    if (file_p != NULL) {
        length = strlen(argv[1]);
        res = fwrite(argv[1], 1, length, file_p);

        if (res == (int)length) {
            res = fwrite("\n", 1, 1, file_p);

            if (res == 1) {
                res = 0;
            } else {
                res = -errno;
            }
        } else {
            res = -errno;
        }

        fclose(file_p);
    } else {
        res = -errno;
    }

    return (res);
}

static int command_ntp_sync(int argc, const char *argv[])
{
    const char *server_p;

    if (argc == 1) {
        server_p = "0.se.pool.ntp.org";
    } else if (argc == 2) {
        server_p = argv[1];
    } else {
        printf("Usage: ntp_sync [<server>]\n");

        return (-EINVAL);
    }

    return (ml_ntp_client_sync(server_p));
}

static void command_dd_summary(size_t total_size,
                               struct timeval *start_time_p,
                               struct timeval *end_time_p)
{
    float time_elapsed_ms;
    float transfer_rate;
    struct timeval elapsed_time;

    timersub(end_time_p, start_time_p, &elapsed_time);
    time_elapsed_ms = ml_timeval_to_ms(&elapsed_time);
    transfer_rate = ((float)total_size / time_elapsed_ms / 1000.0f);

    printf("%lu bytes copied in %.03f ms (%.03f MB/s).\n",
           (unsigned long)total_size,
           time_elapsed_ms,
           transfer_rate);
}

static int command_dd(int argc, const char *argv[])
{
    int res;
    struct timeval start_time;
    struct timeval end_time;
    size_t total_size;
    size_t chunk_size;

    if (argc != 5) {
        printf("Usage: dd <infile> <outfile> <total-size> <chunk-size>\n");

        return (-EINVAL);
    }

    total_size = atoi(argv[3]);
    chunk_size = atoi(argv[4]);
    gettimeofday(&start_time, NULL);

    res = ml_dd(argv[1], argv[2], total_size, chunk_size);

    if (res != 0) {
        return (res);
    }

    gettimeofday(&end_time, NULL);
    command_dd_summary(total_size, &start_time, &end_time);

    return (0);
}

static void print_kernel_message(char *message_p)
{
    unsigned long long secs;
    unsigned long long usecs;
    int text_pos;
    char *text_p;
    char *match_p;

    if (sscanf(message_p, "%*u,%*u,%llu,%*[^;]; %n", &usecs, &text_pos) != 1) {
        return;
    }

    text_p = &message_p[text_pos];
    match_p = strchr(text_p, '\n');

    if (match_p != NULL) {
        *match_p = '\0';
    }

    secs = (usecs / 1000000);
    usecs %= 1000000;

    printf("[%5lld.%06lld] %s\n", secs, usecs, text_p);
}

static int command_dmesg(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;

    char message[1024];
    ssize_t size;
    int fd;

    fd = ml_open("/dev/kmsg", O_RDONLY | O_NONBLOCK);

    if (fd == -1) {
        return (-errno);
    }

    for (;;) {
        size = read(fd, &message[0], sizeof(message) - 1);

        if (size <= 0) {
            break;
        }

        message[size] = '\0';
        print_kernel_message(&message[0]);
    }

    close(fd);

    return (0);
}

static int command_sync(int argc, const char *argv[])
{
    (void)argc;
    (void)argv;

    sync();

    return (0);
}

static int command_status(int argc, const char *argv[])
{
    (void)argv;

    int length;
    struct ml_cpu_stats_t stats[16];
    int i;

    if (argc != 1) {
        printf("Usage: status\n");

        return (-EINVAL);
    }

    length = ml_get_cpus_stats(&stats[0], membersof(stats));

    if (length <= 0) {
        return (-EGENERAL);
    }

    printf("CPU  USER  SYSTEM  IDLE\n");
    printf("all  %3u%%    %3u%%  %3u%%\n",
           stats[0].user,
           stats[0].system,
           stats[0].idle);

    for (i = 1; i < length; i++) {
        printf("%-3d  %3u%%    %3u%%  %3u%%\n",
               i,
               stats[i].user,
               stats[i].system,
               stats[i].idle);
    }

    return (0);
}

static void history_init(void)
{
    module.history.head_p = NULL;
    module.history.tail_p = NULL;
    module.history.length = 0;
    module.history.current_p = NULL;
    line_init(&module.history.line);
    module.history.line_valid = false;
}

static int history_append(char *command_p)
{
    struct history_elem_t *elem_p, *head_p;

    /* Do not append if the command already is at the end of the
       list. */
    if (module.history.tail_p != NULL) {
        if (strcmp(module.history.tail_p->buf, command_p) == 0) {
            return (0);
        }
    }

    if (module.history.length == MAXIMUM_HISTORY_LENGTH) {
        head_p = module.history.head_p;

        /* Remove the head element from the list. */
        if (head_p == module.history.tail_p) {
            module.history.head_p = NULL;
            module.history.tail_p = NULL;
        } else {
            module.history.head_p = head_p->next_p;
            head_p->next_p->prev_p = NULL;
        }

        free(head_p);
        module.history.length--;
    }

    /* Allocate memory. */
    elem_p = malloc(sizeof(*elem_p) + strlen(command_p) + 1);

    if (elem_p != NULL) {
        strcpy(elem_p->buf, command_p);

        /* Append the command to the command history list. */
        elem_p->next_p = NULL;

        if (module.history.head_p == NULL) {
            elem_p->prev_p = NULL;
            module.history.head_p = elem_p;
        } else {
            elem_p->prev_p = module.history.tail_p;
            elem_p->prev_p->next_p = elem_p;
        }

        module.history.tail_p = elem_p;
        module.history.length++;
    }

    return (0);
}

/**
 * Find the previous element, if any.
 */
static char *history_get_previous_command(void)
{
    if (module.history.current_p == module.history.head_p) {
        return (NULL);
    } else if (module.history.current_p == NULL) {
        module.history.current_p = module.history.tail_p;

        /* Save the current command to be able to restore it when DOWN
           is pressed. */
        module.history.line = module.line;
        module.history.line_valid = true;
    } else if (module.history.current_p != module.history.head_p) {
        module.history.current_p = module.history.current_p->prev_p;
    }

    if (module.history.current_p != NULL) {
        return (module.history.current_p->buf);
    } else {
        return (NULL);
    }
}

/**
 * Find the next element, if any.
 */
static char *history_get_next_command(void)
{
    if (module.history.current_p != NULL) {
        module.history.current_p = module.history.current_p->next_p;
    }

    if (module.history.current_p != NULL) {
        return (module.history.current_p->buf);
    } else if (module.history.line_valid) {
        module.history.line_valid = false;

        return (line_get_buf(&module.history.line));
    } else {
        return (NULL);
    }
}

static void history_reset_current(void)
{
    module.history.current_p = NULL;
}

static char *history_reverse_search(const char *pattern_p)
{
    struct history_elem_t *elem_p;

    elem_p = module.history.tail_p;

    while (elem_p != NULL) {
        if (strstr(elem_p->buf, pattern_p) != NULL) {
            return (elem_p->buf);
        }

        elem_p = elem_p->prev_p;
    }

    return (NULL);
}

static void auto_complete_command(void)
{
    char next_char;
    bool mismatch;
    int size;
    int i;
    int first_match;
    bool completion_happend;
    char *line_p;

    line_p = line_get_buf(&module.line);
    size = line_get_length(&module.line);

    /* Find the first command matching given line. */
    first_match = -1;

    for (i = 0; i < module.number_of_commands; i++) {
        if (strncmp(module.commands_p[i].name_p, line_p, size) == 0) {
            first_match = i;
            break;
        }
    }

    /* No command matching the line. */
    if (first_match == -1) {
        return;
    }

    /* Auto-complete given line. Compare the next character each
       iteration. */
    completion_happend = false;

    while (true) {
        mismatch = false;
        next_char = module.commands_p[first_match].name_p[size];

        /* It's a match if all commands matching line has the same
           next character. */
        i = first_match;

        while ((i < module.number_of_commands)
               && (strncmp(&module.commands_p[i].name_p[0],
                           line_p,
                           size) == 0)) {
            if (module.commands_p[i].name_p[size] != next_char) {
                mismatch = true;
                break;
            }

            i++;
        }

        /* This character mismatch? */
        if (mismatch) {
            break;
        }

        completion_happend = true;

        /* Append a space on full match. */
        if (next_char == '\0') {
            line_insert(&module.line, ' ');
            break;
        }

        line_insert(&module.line, next_char);
        size++;
    }

    /* Print all alternatives if no completion happened. */
    if (!completion_happend) {
        putchar('\n');
        i = first_match;

        while ((i < module.number_of_commands)
               && (strncmp(module.commands_p[i].name_p,
                           line_p,
                           size) == 0)) {
            printf("%s\n", module.commands_p[i].name_p);
            i++;
        }

        print_prompt();
        printf("%s", line_get_buf(&module.line));
    }
}

static void handle_tab(void)
{
    auto_complete_command();
}

static void handle_carrige_return(void)
{
    module.carriage_return_received = true;
}

static void handle_newline(void)
{
    module.newline_received = true;
}

/**
 * BACKSPACE Delete the character before the cursor.
 */
static void handle_backspace(void)
{
    if (line_seek(&module.line, -1)) {
        line_delete(&module.line);
    }
}

/**
 * Ctrl+A Go to the beginning of the line.
 */
static void handle_ctrl_a(void)
{
    line_seek_begin(&module.line);
}

/**
 * Ctrl+E Go to the end of the line.
 */
static void handle_ctrl_e(void)
{
    line_seek_end(&module.line);
}

/**
 * Ctrl+D Delete the chracter at the cursor.
 */
static void handle_ctrl_d(void)
{
    line_delete(&module.line);
}

/**
 * Ctrl+K Cut the line from cursor to end.
 */
static void handle_ctrl_k(void)
{
    line_truncate(&module.line);
}

/**
 * Ctrl+T Swap the last two characters before the cursor (typo).
 */
static void handle_ctrl_t(void)
{
    int ch;
    int cursor;

    /* Is a swap possible? */
    cursor = line_get_cursor(&module.line);

    /* Cannot swap if the cursor is at the beginning of the line. */
    if (cursor == 0) {
        return;
    }

    /* Cannot swap if there are less than two characters. */
    if (line_get_length(&module.line) < 2) {
        return;
    }

    /* Move the cursor to the second character. */
    if (cursor == line_get_length(&module.line)) {
        line_seek(&module.line, -1);
    }

    /* Swap the two characters. */
    ch = line_peek(&module.line);
    line_delete(&module.line);
    line_seek(&module.line, -1);
    line_insert(&module.line, ch);
    line_seek(&module.line, 1);
}

static void restore_previous_line(struct line_t *pattern_p)
{
    int cursor;
    int length;

    printf("\x1b[%dD\x1b[K%s",
           17 + line_get_length(pattern_p),
           line_get_buf(&module.prev_line));

    cursor = line_get_cursor(&module.prev_line);
    length = line_get_length(&module.prev_line);

    if (cursor != length) {
        printf("\x1b[%dD", length - cursor);
    }
}

/**
 * Ctrl+R Recall the last command including the specified character(s)
 * searches the command history as you type.
 *
 * The original line buffer is printed and cursor reset, then the
 * selected command is copied into the line buffer. The output of the
 * new command occurs in the main command loop.
 */
static void handle_ctrl_r(void)
{
    int ch;
    char *buf_p;

    line_init(&module.history.pattern);
    line_init(&module.history.match);

    if (!line_is_empty(&module.line)) {
        printf("\x1b[%dD", line_get_length(&module.line));
    }

    printf("\x1b[K(history-search)`': \x1b[3D");

    while (true) {
        fflush(stdout);
        ch = xgetc();

        switch (ch) {

        case DELETE:
        case BACKSPACE:
            if (!line_is_empty(&module.history.pattern)) {
                printf("\x1b[1D\x1b[K': ");
                line_seek(&module.history.pattern, -1);
                line_delete(&module.history.pattern);
                buf_p = history_reverse_search(
                    line_get_buf(&module.history.pattern));
                line_init(&module.history.match);

                if (buf_p != NULL) {
                    line_insert_string(&module.history.match, buf_p);
                }

                printf("%s\x1b[%dD",
                       line_get_buf(&module.history.match),
                       line_get_length(&module.history.match) + 3);
            }

            break;

        case CARRIAGE_RETURN:
            module.carriage_return_received = true;
            break;

        case CTRL_G:
            restore_previous_line(&module.history.pattern);
            return;

        default:
            if (isprint(ch)) {
                if (line_insert(&module.history.pattern, ch)) {
                    printf("\x1b[K%c': ", ch);
                    buf_p = history_reverse_search(
                        line_get_buf(&module.history.pattern));
                    line_init(&module.history.match);

                    if (buf_p != NULL) {
                        line_insert_string(&module.history.match, buf_p);
                    }

                    printf("%s\x1b[%dD",
                           line_get_buf(&module.history.match),
                           line_get_length(&module.history.match) + 3);
                }
            } else {
                restore_previous_line(&module.history.pattern);

                /* Copy the match to current line. */
                module.line = module.history.match;

                if (ch == NEWLINE) {
                    module.newline_received = true;
                } else {
                    if (ch == ALT) {
                        ch = xgetc();

                        if (ch != 'd') {
                            (void)xgetc();
                        }
                    }
                }

                return;
            }
        }
    }
}

/**
 * ALT.
 */
static void handle_alt(void)
{
    int ch;
    char *buf_p;

    ch = xgetc();

    switch (ch) {

    case 'd':
        /* Alt+D Delete the word at the cursor. */
        while (isblank((int)line_peek(&module.line))) {
            line_delete(&module.line);
        }

        while (!isblank((int)line_peek(&module.line))
               && (line_peek(&module.line) != '\0')) {
            line_delete(&module.line);
        }

        break;

    case 'O':
        ch = xgetc();

        switch (ch) {

        case 'H':
            /* HOME. */
            line_seek_begin(&module.line);
            break;

        case 'F':
            /* END. */
            line_seek_end(&module.line);
            break;

        default:
            break;
        }

        break;

    case '[':
        ch = xgetc();

        switch (ch) {

        case 'A':
        case 'B':
            if (ch == 'A') {
                /* UP Previous command. */
                buf_p = history_get_previous_command();
            } else {
                /* DOWN Next command. */
                buf_p = history_get_next_command();
            }

            if (buf_p != NULL) {
                line_init(&module.line);
                line_insert_string(&module.line, buf_p);
            }

            break;

        case 'C':
            /* RIGHT Go right on character. */
            line_seek(&module.line, 1);
            break;

        case 'D':
            /* LEFT Go left one character. */
            line_seek(&module.line, -1);
            break;

        default:
            break;
        }

        break;

    default:
        break;
    }
}

static void handle_other(char ch)
{
    line_insert(&module.line, ch);
}

/**
 * Show updated line to the user and update the cursor to its new
 * position.
 */
static void show_line(void)
{
    int i;
    int cursor;
    int new_cursor;
    int length;
    int new_length;
    int min_length;

    cursor = line_get_cursor(&module.prev_line);
    length = line_get_length(&module.prev_line);
    new_length = line_get_length(&module.line);
    new_cursor = line_get_cursor(&module.line);
    min_length = MIN(line_get_length(&module.prev_line), new_length);

    /* Was the line edited? */
    if (strcmp(line_get_buf(&module.line),
               line_get_buf(&module.prev_line)) != 0) {
        /* Only output the change if the last part of the string
           shall be deleted. */
        if ((strncmp(line_get_buf(&module.line),
                     line_get_buf(&module.prev_line),
                     min_length) == 0)
            && (new_cursor == new_length)) {
            if (length < new_length) {
                /* New character. */
                printf("%s", &line_get_buf(&module.line)[cursor]);
            } else {
                /* Move the cursor to the end of the old line. */
                for (i = cursor; i < length; i++) {
                    printf(" ");
                }

                /* Backspace. */
                for (i = new_length; i < length; i++) {
                    printf("\x08 \x08");
                }
            }
        } else {
            if (cursor > 0) {
                printf("\x1b[%dD", cursor);
            }

            printf("\x1b[K%s", line_get_buf(&module.line));

            if (new_cursor < new_length) {
                printf("\x1b[%dD", new_length - new_cursor);
            }
        }
    } else if (cursor < new_cursor) {
        printf("\x1b[%dC", new_cursor - cursor);
    } else if (new_cursor < cursor) {
        printf("\x1b[%dD", cursor - new_cursor);
    }

    fflush(stdout);
}

/**
 * Execute the current line.
 */
static int execute_line(void)
{
    if (module.carriage_return_received) {
        printf("\r");
    }

    printf("\n");

    /* Append the command to the history. */
    if (!line_is_empty(&module.line)) {
        history_append(line_get_buf(&module.line));
    }

    history_reset_current();

    return (line_get_length(&module.line));
}

/**
 * Read the next command.
 */
static int read_command(void)
{
    int ch;

    /* Initialize the read command state. */
    line_init(&module.line);
    module.carriage_return_received = false;
    module.newline_received = false;

    while (true) {
        ch = xgetc();

        /* Save current line. */
        module.prev_line = module.line;

        switch (ch) {

        case TAB:
            handle_tab();
            break;

        case CARRIAGE_RETURN:
            handle_carrige_return();
            break;

        case NEWLINE:
            handle_newline();
            break;

        case DELETE:
        case BACKSPACE:
            handle_backspace();
            break;

        case CTRL_A:
            handle_ctrl_a();
            break;

        case CTRL_E:
            handle_ctrl_e();
            break;

        case CTRL_D:
            handle_ctrl_d();
            break;

        case CTRL_K:
            handle_ctrl_k();
            break;

        case CTRL_T:
            handle_ctrl_t();
            break;

        case CTRL_R:
            handle_ctrl_r();
            break;

        case ALT:
            handle_alt();
            break;

        default:
            handle_other(ch);
            break;
        }

        /* Show the new line to the user and execute it if enter was
           pressed. */
        show_line();

        if (module.newline_received) {
            return (execute_line());
        }
    }
}

void *shell_main(void *arg_p)
{
    (void)arg_p;

    int res;
    char *stripped_line_p;

    qsort(module.commands_p,
          module.number_of_commands,
          sizeof(*module.commands_p),
          compare_qsort);

    pthread_setname_np(pthread_self(), "ml_shell");

    while (true) {
        /* Read command.*/
        res = read_command();

        if (res > 0) {
            stripped_line_p = ml_strip(line_get_buf(&module.line), NULL);

            if (is_comment(stripped_line_p)) {
                /* Just print a prompt. */
            } else if (is_exit(stripped_line_p)) {
                break;
            } else {
                res = execute_command(stripped_line_p);

                if (res == 0) {
                    printf("OK\n");
                } else {
                    printf("ERROR(%d: %s)\n", res, ml_strerror(-res));
                }
            }
        }

        print_prompt();
    }

    return (NULL);
}

void ml_shell_init(void)
{
    make_stdin_unbuffered();

    module.number_of_commands = 0;
    module.commands_p = xmalloc(1);
    history_init();

    ml_shell_register_command("help",
                              "Print this help.",
                              command_help);
    ml_shell_register_command("history",
                              "List command history.",
                              command_history);
    ml_shell_register_command("exit",
                              "Shell exit.",
                              command_exit);
    ml_shell_register_command("suicide",
                              "Process suicide.",
                              command_suicide);
    ml_shell_register_command("ls",
                              "List directory contents.",
                              command_ls);
    ml_shell_register_command("cat",
                              "Print a file.",
                              command_cat);
    ml_shell_register_command("hexdump",
                              "Hexdump a file.",
                              command_hexdump);
    ml_shell_register_command("reboot",
                              "Reboot.",
                              command_reboot);
    ml_shell_register_command("insmod",
                              "Insert a kernel module.",
                              command_insmod);
    ml_shell_register_command("mknod",
                              "Create a node.",
                              command_mknod);
    ml_shell_register_command("mount",
                              "Mount a filesystem.",
                              command_mount);
    ml_shell_register_command("df",
                              "Disk space usage.",
                              command_df);
    ml_shell_register_command("find",
                              "Find files and folders.",
                              command_find);
    ml_shell_register_command("date",
                              "Print current date.",
                              command_date);
    ml_shell_register_command("print",
                              "Print to file.",
                              command_print);
    ml_shell_register_command("dmesg",
                              "Print the kernel ring buffer.",
                              command_dmesg);
    ml_shell_register_command("sync",
                              "Synchronize cached writes to persistent storage.",
                              command_sync);
    ml_shell_register_command("status",
                              "System status.",
                              command_status);
    ml_shell_register_command("ntp_sync",
                              "NTP time sync.",
                              command_ntp_sync);
    ml_shell_register_command("dd",
                              "File copy.",
                              command_dd);
}

void ml_shell_start(void)
{
    pthread_create(&module.pthread,
                   NULL,
                   (void *(*)(void *))shell_main,
                   NULL);
}

void ml_shell_join(void)
{
    pthread_join(module.pthread, NULL);
}

void ml_shell_register_command(const char *name_p,
                               const char *description_p,
                               ml_shell_command_callback_t callback)
{
    struct command_t *command_p;

    module.number_of_commands++;
    module.commands_p = xrealloc(
        module.commands_p,
        sizeof(*module.commands_p) * module.number_of_commands);
    command_p = &module.commands_p[module.number_of_commands - 1];
    command_p->name_p = name_p;
    command_p->description_p = description_p;
    command_p->callback = callback;
}
