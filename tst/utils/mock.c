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

#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "nala.h"
#include "ml/ml.h"
#include "mock.h"

struct callback_t {
    const char *name_p;
    ml_shell_command_callback_t callback;
    struct callback_t *next_p;
};

static struct callback_t list = {
    .name_p = "",
    .next_p = NULL
};

void mock_set_callback(const char *name_p,
                       const char *description_p,
                       ml_shell_command_callback_t callback)
{
    (void)description_p;

    struct callback_t *item_p;

    item_p = xmalloc(sizeof(*item_p));
    item_p->name_p = name_p;
    item_p->callback = callback;
    item_p->next_p = list.next_p;
    list.next_p = item_p;
}

ml_shell_command_callback_t mock_get_callback(const char *name_p)
{
    struct callback_t *item_p;

    item_p = &list;

    while (item_p != NULL) {
        if (strcmp(item_p->name_p, name_p) == 0) {
            return (item_p->callback);
        }

        item_p = item_p->next_p;
    }

    nala_test_failure(
        nala_format("Shell command callback not found for %s.\n",
                    name_p));

    return (NULL);
}

int ioctl_mock_va_arg_real(int __fd,
                           unsigned long int __request,
                           va_list __nala_va_list)
{
    (void)__fd;
    (void)__request;
    (void)__nala_va_list;

    return (-1);
}
