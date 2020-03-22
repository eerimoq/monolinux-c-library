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

#include <stdio.h>
#include <stdbool.h>
#include <stdarg.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include "ml/ml.h"
#include "internal.h"

struct module_t {
    int fd;
    struct ml_log_object_t *head_p;
};

static struct module_t module = {
    .fd = STDOUT_FILENO,
    .head_p = NULL
};

static const char *level_to_string(int level)
{
    const char *name_p;

    switch (level) {

    case ML_LOG_EMERGENCY:
        name_p = "EMERGENCY";
        break;

    case ML_LOG_ALERT:
        name_p = "ALERT";
        break;

    case ML_LOG_CRITICAL:
        name_p = "CRITICAL";
        break;

    case ML_LOG_ERROR:
        name_p = "ERROR";
        break;

    case ML_LOG_WARNING:
        name_p = "WARNING";
        break;

    case ML_LOG_NOTICE:
        name_p = "NOTICE";
        break;

    case ML_LOG_INFO:
        name_p = "INFO";
        break;

    case ML_LOG_DEBUG:
        name_p = "DEBUG";
        break;

    default:
        name_p = "INVALID";
        break;
    }

    return (name_p);
}

void ml_log_object_module_init(void)
{
#ifndef UNIT_TEST
    /* Use /dev/kmsg without rate limiting. */
    if (ml_file_write_string("/proc/sys/kernel/printk_devkmsg", "on\n") == 0) {
        module.fd = ml_open("/dev/kmsg", O_WRONLY);
    }
#endif
}

void ml_log_object_register(struct ml_log_object_t *self_p)
{
    self_p->next_p = module.head_p;
    module.head_p = self_p;
}

struct ml_log_object_t *ml_log_object_get_by_name(const char *name_p)
{
    struct ml_log_object_t *log_object_p;

    log_object_p = NULL;

    while (true) {
        log_object_p = ml_log_object_list_next(log_object_p);

        if (log_object_p == NULL) {
            break;
        }

        if (strcmp(log_object_p->name_p, name_p) == 0) {
            break;
        }
    }

    return (log_object_p);
}

struct ml_log_object_t *ml_log_object_list_next(struct ml_log_object_t *log_object_p)
{
    if (log_object_p == NULL) {
        return (module.head_p);
    } else {
        return (log_object_p->next_p);
    }
}

void ml_log_object_init(struct ml_log_object_t *self_p,
                     const char *name_p,
                     int level)
{
    self_p->name_p = name_p;
    self_p->level = level;
}

void ml_log_object_set_level(struct ml_log_object_t *self_p,
                         int level)
{
    self_p->level = level;
}

bool ml_log_object_is_enabled_for(struct ml_log_object_t *self_p,
                                  int level)
{
    return (level <= self_p->level);
}

void ml_log_object_vprint(struct ml_log_object_t *self_p,
                          int level,
                          const char *fmt_p,
                          va_list vlist)
{
    char buf[512];
    time_t now;
    struct tm tm;
    size_t length;
    ssize_t written;

    if (level > self_p->level) {
        return;
    }

    now = time(NULL);
    gmtime_r(&now, &tm);

    length = strftime(&buf[0], sizeof(buf), "%F %T", &tm);
    length += snprintf(&buf[length],
                       sizeof(buf) - length,
                       " %s %s ",
                       level_to_string(level),
                       self_p->name_p);
    length += vsnprintf(&buf[length], sizeof(buf) - length, fmt_p, vlist);

    if (length >= sizeof(buf)) {
        length = (sizeof(buf) - 1);
    }

    buf[length++] = '\n';
    written = write(module.fd, &buf[0], length);
    (void)written;
}

void ml_log_object_print(struct ml_log_object_t *self_p,
                      int level,
                      const char *fmt_p,
                      ...)
{
    va_list vlist;

    va_start(vlist, fmt_p);
    ml_log_object_vprint(self_p, level, fmt_p, vlist);
    va_end(vlist);
}
