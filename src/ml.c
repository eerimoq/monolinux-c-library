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

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdarg.h>
#include <arpa/inet.h>
#include <mntent.h>
#include <stdint.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/statvfs.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include "ml/ml.h"
#include "internal.h"

struct cpu_stats_t {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long total;
};

struct module_t {
    struct ml_bus_t bus;
    struct ml_worker_pool_t worker_pool;
    struct ml_timer_handler_t timer_handler;
    struct ml_log_object_t log_object;
};

static struct module_t module;

static inline bool char_in_string(char c, const char *str_p)
{
    while (*str_p != '\0') {
        if (c == *str_p) {
            return (true);
        }

        str_p++;
    }

    return (false);
}

static void print_ascii(FILE *fout_p, const uint8_t *buf_p, size_t size)
{
    size_t i;

    for (i = 0; i < 16 - size; i++) {
        fprintf(fout_p, "   ");
    }

    fprintf(fout_p, "'");

    for (i = 0; i < size; i++) {
        fprintf(fout_p, "%c", isprint((int)buf_p[i]) ? buf_p[i] : '.');
    }

    fprintf(fout_p, "'");
}

static void hexdump(const uint8_t *buf_p, size_t size, int offset, FILE *fout_p)
{
    int pos;

    pos = 0;

    while (size > 0) {
        if ((pos % 16) == 0) {
            fprintf(fout_p, "%08x: ", offset + pos);
        }

        fprintf(fout_p, "%02x ", buf_p[pos] & 0xff);

        if ((pos % 16) == 15) {
            print_ascii(fout_p, &buf_p[pos - 15], 16);
            fprintf(fout_p, "\n");
        }

        pos++;
        size--;
    }

    if ((pos % 16) != 0) {
        print_ascii(fout_p, &buf_p[pos - (pos % 16)], pos % 16);
        fprintf(fout_p, "\n");
    }
}

static int read_cpu_stats(FILE *file_p, struct cpu_stats_t *stats_p)
{
    char line[128];
    int res;

    if (fgets(&line[0], sizeof(line), file_p) == NULL) {
        return (-EGENERAL);
    }

    if (strncmp("intr", &line[0], 4) == 0) {
        return (1);
    }

    res = sscanf(&line[0],
                 "cp%*s %llu %llu %llu %llu %llu %llu %llu",
                 &stats_p->user,
                 &stats_p->nice,
                 &stats_p->system,
                 &stats_p->idle,
                 &stats_p->iowait,
                 &stats_p->irq,
                 &stats_p->softirq);

    if (res != 7) {
        return (-EGENERAL);
    }

    stats_p->total = 0;
    stats_p->total += stats_p->user;
    stats_p->total += stats_p->nice;
    stats_p->total += stats_p->system;
    stats_p->total += stats_p->idle;
    stats_p->total += stats_p->iowait;
    stats_p->total += stats_p->irq;
    stats_p->total += stats_p->softirq;

    return (0);
}

static int read_cpus_stats(struct cpu_stats_t *stats_p, int length)
{
    FILE *file_p;
    int res;
    int i;

    file_p = fopen("/proc/stat", "r");

    if (file_p == NULL) {
        return (-errno);
    }

    res = -EGENERAL;

    for (i = 0; i < length; i++) {
        res = read_cpu_stats(file_p, &stats_p[i]);

        if (res != 0) {
            break;
        }
    }

    fclose(file_p);

    return (res < 0 ? res : i);
}

static unsigned calc_cpu_load(unsigned long long old,
                              unsigned long long new,
                              unsigned long long total_diff)
{
    return ((100 * (new - old)) / total_diff);
}

void ml_init(void)
{
    ml_log_object_module_init();
    ml_log_object_init(&module.log_object, "default", ML_LOG_INFO);
    ml_log_object_register(&module.log_object);
    ml_message_init();
    ml_bus_init(&module.bus);
    ml_worker_pool_init(&module.worker_pool, 4, 32);
    ml_timer_handler_init(&module.timer_handler);
}

const char *ml_uid_str(struct ml_uid_t *uid_p)
{
    return (uid_p->name_p);
}

void ml_broadcast(void *message_p)
{
    ml_bus_broadcast(&module.bus, message_p);
}

void ml_subscribe(struct ml_queue_t *queue_p, struct ml_uid_t *uid_p)
{
    ml_bus_subscribe(&module.bus, queue_p, uid_p);
}

void ml_spawn(ml_worker_pool_job_entry_t entry, void *arg_p)
{
    ml_worker_pool_spawn(&module.worker_pool, entry, arg_p);
}

char *ml_strip(char *str_p, const char *strip_p)
{
    ml_rstrip(str_p, strip_p);

    return (ml_lstrip(str_p, strip_p));
}

char *ml_lstrip(char *str_p, const char *strip_p)
{
    /* Strip whitespace characters by default. */
    if (strip_p == NULL) {
        strip_p = "\t\n\x0b\x0c\r ";
    }

    /* String leading characters. */
    while (*str_p != '\0') {
        if (!char_in_string(*str_p, strip_p)) {
            break;
        }

        str_p++;
    }

    return (str_p);
}

void ml_rstrip(char *str_p, const char *strip_p)
{
    char *begin_p;
    size_t length;

    /* Strip whitespace characters by default. */
    if (strip_p == NULL) {
        strip_p = "\t\n\x0b\x0c\r ";
    }

    begin_p = str_p;

    /* Strip training characters. */
    length = strlen(str_p);
    str_p += (length - 1);

    while (str_p >= begin_p) {
        if (!char_in_string(*str_p, strip_p)) {
            break;
        }

        *str_p = '\0';
        str_p--;
    }
}

void ml_hexdump(const void *buf_p, size_t size, FILE *fout_p)
{
    hexdump(buf_p, size, 0, fout_p);
}

int ml_hexdump_file(FILE *fin_p, size_t offset, ssize_t size, FILE *fout_p)
{
    uint8_t buf[256];
    size_t chunk_size;

    if (fseek(fin_p, offset, SEEK_SET) != 0) {
        return (-EGENERAL);
    }

    while (true) {
        if (size == -1) {
            chunk_size = sizeof(buf);
        } else {
            if ((size_t)size < sizeof(buf)) {
                chunk_size = size;
            } else {
                chunk_size = sizeof(buf);
            }
        }

        chunk_size = fread(&buf[0], 1, chunk_size, fin_p);
        hexdump(&buf[0], chunk_size, offset, fout_p);

        if (chunk_size < sizeof(buf)) {
            break;
        }

        offset += chunk_size;
        size -= chunk_size;
    }

    return (0);
}

void ml_print_file(const char *name_p, FILE *fout_p)
{
    FILE *fin_p;
    uint8_t buf[256];
    size_t size;

    fin_p = fopen(name_p, "rb");

    if (fin_p == NULL) {
        return;
    }

    while ((size = fread(&buf[0], 1, membersof(buf), fin_p)) > 0) {
        if (fwrite(&buf[0], 1, size, fout_p) != size) {
            break;
        }
    }

    fclose(fin_p);
}

void ml_print_uptime(void)
{
    printf("Uptime: ");
    ml_print_file("/proc/uptime", stdout);
    printf("\n");
}

int ml_insert_module(const char *path_p, const char *params_p)
{
    int res;
    int fd;

    fd = open(path_p, O_RDONLY);

    if (fd == -1) {
        return (-errno);
    }

    res = ml_finit_module(fd, params_p, 0);

    if (res != 0) {
        res = -errno;
    }

    close(fd);

    return (res);
}

int ml_file_system_space_usage(const char *path_p,
                               unsigned long *total_p,
                               unsigned long *used_p,
                               unsigned long *free_p)
{
    int res;
    struct statvfs stat;
    unsigned long long total;
    unsigned long long used;

    res = statvfs(path_p, &stat);

    if (res != 0) {
        return (-errno);
    }

    total = (stat.f_bsize * stat.f_blocks);
    *total_p = (total / (1024 * 1024));
    used = (stat.f_bsize * (stat.f_blocks - stat.f_bfree));
    *used_p = (used / (1024 * 1024));
    *free_p = (*total_p - *used_p);

    return (0);
}

int ml_print_file_systems_space_usage(FILE *fout_p)
{
    int res;
    FILE *fin_p;
    struct mntent *mntent_p;
    unsigned long total;
    unsigned long available;
    unsigned long used;

    res = -EGENERAL;

    /* Find all mounted file systems. */
    fin_p = setmntent("/proc/mounts", "r");

    if (fin_p != NULL) {
        fprintf(fout_p, "MOUNTED ON               TOTAL      USED      FREE\n");

        while ((mntent_p = getmntent(fin_p)) != NULL) {
            res = ml_file_system_space_usage(mntent_p->mnt_dir,
                                             &total,
                                             &used,
                                             &available);

            if (res != 0) {
                break;
            }

            fprintf(fout_p,
                    "%-20s %6lu MB %6lu MB %6lu MB\n",
                    mntent_p->mnt_dir,
                    total,
                    used,
                    available);
        }

        endmntent(fin_p);
    }

    return (res);
}

int ml_mount(const char *source_p,
             const char *target_p,
             const char *type_p,
             unsigned long flags,
             const char *options_p)
{
    return (mount(source_p, target_p, type_p, flags, options_p));
}

int ml_get_cpus_stats(struct ml_cpu_stats_t *stats_p, int length)
{
    int res;
    struct cpu_stats_t stats[2][length];
    int i;
    unsigned long long total_diff;

    length = read_cpus_stats(&stats[0][0], length);

    if (length < 0) {
        return (length);
    }

    /* Measure over 500 ms. */
    usleep(500000);

    res = read_cpus_stats(&stats[1][0], length);

    if (res < 0) {
        return (res);
    }

    if (res != length) {
        return (-EGENERAL);
    }

    for (i = 0; i < length; i++) {
        total_diff = stats[1][i].total - stats[0][i].total;
        stats_p[i].user = calc_cpu_load(stats[0][i].user,
                                        stats[1][i].user,
                                        total_diff);
        stats_p[i].system = calc_cpu_load(stats[0][i].system,
                                          stats[1][i].system,
                                          total_diff);
        stats_p[i].idle = calc_cpu_load(stats[0][i].idle,
                                        stats[1][i].idle,
                                        total_diff);
    }

    return (length);
}

int ml_socket(int domain, int type, int protocol)
{
    return (socket(domain, type, protocol));
}

int ml_ioctl(int fd, unsigned long request, void *data_p)
{
    return (ioctl(fd, request, data_p));
}

const char *ml_bool_str(bool value)
{
    if (value) {
        return "true";
    } else {
        return "false";
    }
}

void *xmalloc(size_t size)
{
    void *buf_p;

    buf_p = malloc(size);

    if (buf_p == NULL) {
        exit(1);
    }

    return (buf_p);
}

void *xrealloc(void *buf_p, size_t size)
{
    buf_p = realloc(buf_p, size);

    if (buf_p == NULL) {
        exit(1);
    }

    return (buf_p);
}

void ml_timer_init(struct ml_timer_t *self_p,
                   struct ml_uid_t *message_p,
                   struct ml_queue_t *queue_p)
{
    ml_timer_handler_timer_init(&module.timer_handler,
                                self_p,
                                message_p,
                                queue_p);
}

void ml_timer_start(struct ml_timer_t *self_p,
                    unsigned int initial,
unsigned int repeat)
{
    ml_timer_handler_timer_start(self_p, initial, repeat);
}

void ml_timer_stop(struct ml_timer_t *self_p)
{
    ml_timer_handler_timer_stop(self_p);
}

void ml_log_print(int level, const char *fmt_p, ...)
{
    va_list vlist;

    va_start(vlist, fmt_p);
    ml_log_object_vprint(&module.log_object, level, fmt_p, vlist);
    va_end(vlist);
}

void ml_log_set_level(int level)
{
    ml_log_object_set_level(&module.log_object, level);
}

bool ml_log_is_enabled_for(int level)
{
    return (ml_log_object_is_enabled_for(&module.log_object, level));
}

int ml_file_write_string(const char *path_p, const char *data_p)
{
    FILE *file_p;
    int res;

    file_p = fopen(path_p, "w");

    if (file_p == NULL) {
        return (-errno);
    }

    if (fwrite(data_p, strlen(data_p), 1, file_p) == 1) {
        res = 0;
    } else {
        res = -EGENERAL;
    }

    fclose(file_p);

    return (res);
}

int ml_file_read(const char *path_p, void *buf_p, size_t size)
{
    size_t items_read;
    FILE *file_p;

    file_p = fopen(path_p, "rb");

    if (file_p == NULL) {
        return (-errno);
    }

    items_read = fread(buf_p, size, 1, file_p);

    if (items_read != 1) {
        goto out;
    }

    fclose(file_p);

    return (0);

 out:
    fclose(file_p);

    return (-EGENERAL);
}

float ml_timeval_to_ms(struct timeval *timeval_p)
{
    float res;

    res = (float)timeval_p->tv_usec;
    res /= 1000;
    res += (float)(1000 * timeval_p->tv_sec);

    return (res);
}

static int dd_copy_chunk(size_t chunk_size,
                         int fdin,
                         int fdout,
                         void *buf_p)
{
    ssize_t size;

    size = read(fdin, buf_p, chunk_size);

    if (size == -1) {
        return (-errno);
    } else if ((size_t)size != chunk_size) {
        return (-EGENERAL);
    }

    size = write(fdout, buf_p, chunk_size);

    if (size == -1) {
        return (-errno);
    } else if ((size_t)size != chunk_size) {
        return (-EGENERAL);
    }

    return (0);
}

int ml_dd(const char *infile_p,
          const char *outfile_p,
          size_t total_size,
          size_t chunk_size)
{
    int fdin;
    int fdout;
    void *buf_p;
    int res;

    fdin = open(infile_p, O_RDONLY);

    if (fdin == -1) {
        return (-errno);
    }

    fdout = open(outfile_p, O_WRONLY | O_CREAT | O_TRUNC, 0);

    if (fdout == -1) {
        res = -errno;
        goto out1;
    }

    buf_p = malloc(chunk_size);

    if (buf_p == NULL) {
        res = -errno;
        goto out2;
    }

    while (total_size > 0) {
        if (chunk_size > total_size) {
            chunk_size = total_size;
        }

        res = dd_copy_chunk(chunk_size, fdin, fdout, buf_p);

        if (res != 0) {
            goto out3;
        }

        total_size -= chunk_size;
    }

    close(fdin);
    close(fdout);
    free(buf_p);

    return (0);

 out3:
    free(buf_p);

 out2:
    close(fdout);

 out1:
    close(fdin);

    return (res);
}

const char *ml_strerror(int errnum)
{
    switch (errnum) {

    case EGENERAL:
        return "General";

    default:
        return strerror(errnum);
    }
}

void ml_print_kernel_message(char *message_p, FILE *fout_p)
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

    fprintf(fout_p, "[%5lld.%06lld] %s\n", secs, usecs, text_p);
}

static void write_core_dump_to_disk(char *path_p)
{
    char buf[1024];
    FILE *fout_p;
    ssize_t size;
    char path[32];

    sprintf(&path[0], "%s/core", path_p);

    fout_p = fopen(&path[0], "wb");

    if (fout_p == NULL) {
        return;
    }

    while ((size = read(STDIN_FILENO, &buf[0], sizeof(buf))) > 0) {
        if (fwrite(&buf[0], 1, size, fout_p) != (size_t)size) {
            break;
        }
    }

    fclose(fout_p);
}

static void write_log_to_disk(char *path_p)
{
    FILE *fout_p;
    int fd;
    char message[1024];
    ssize_t size;
    char path[32];

    sprintf(&path[0], "%s/log", path_p);

    fout_p = fopen(&path[0], "wb");

    if (fout_p == NULL) {
        return;
    }

    fd = open("/dev/kmsg", O_RDONLY | O_NONBLOCK);

    if (fd == -1) {
        fprintf(fout_p,
                "*** Failed to open /dev/kmsg with error '%s'. ***\n",
                strerror(errno));
        goto out;
    }

    while (true) {
        size = read(fd, &message[0], sizeof(message) - 1);

        if (size <= 0) {
            if ((size < 0) && (errno != EAGAIN)) {
                fprintf(fout_p,
                        "*** Failed to read message with error %s. ***\n",
                        strerror(errno));
            }

            break;
        }

        ml_print_kernel_message(&message[0], fout_p);
    }

    close(fd);

out:
    fclose(fout_p);
}

static bool find_slot(char *path_p)
{
    struct stat statbuf;
    int slot;

    for (slot = 0; slot < 3; slot++) {
        sprintf(path_p, "/disk/coredumps/%d", slot);

        if (lstat(path_p, &statbuf) != 0) {
            return (true);
        }
    }

    return (false);
}

void ml_finalize_coredump(void)
{
    char path[32];

    mkdir("/disk/coredumps", 0777);

    if (find_slot(&path[0])) {
        mkdir(&path[0], 0777);
        write_core_dump_to_disk(&path[0]);
        write_log_to_disk(&path[0]);
        sync();
    }

    exit(0);
}
