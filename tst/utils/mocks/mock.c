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

struct item_t {
    const char *name_p;
    size_t size;
    struct item_t *next_p;
    uint8_t data[1];
};

static struct item_t list = {
    .name_p = "",
    .size = 0,
    .next_p = NULL
};

static void push_item(struct item_t *item_p)
{
    struct item_t *prev_p;

    item_p->next_p = NULL;
    prev_p = &list;

    while (prev_p->next_p != NULL) {
        prev_p = prev_p->next_p;
    }

    prev_p->next_p = item_p;
}

static struct item_t *pop_item(const char *name_p)
{
    struct item_t *item_p;

    item_p = list.next_p;

    if (item_p == NULL) {
        printf("Expected mock item %s, but none are available.", name_p);
        FAIL();
    }

    if (strcmp(item_p->name_p, name_p) != 0) {
        printf("Expected mock item %s, but got %s.\n", name_p, item_p->name_p);
        FAIL();
    }

    list.next_p = item_p->next_p;

    return (item_p);
}

void mock_push(const char *name_p, const void *buf_p, size_t size)
{
    struct item_t *item_p;

    item_p = xmalloc(sizeof(*item_p) + size);
    item_p->name_p = name_p;
    item_p->size = size;
    memcpy(&item_p->data[0], buf_p, size);
    push_item(item_p);
}

void mock_pop(const char *name_p, void *buf_p)
{
    struct item_t *item_p;

    item_p = pop_item(name_p);
    memcpy(buf_p, &item_p->data[0], item_p->size);
    free(item_p);
}

void mock_pop_assert(const char *name_p, const void *buf_p)
{
    struct item_t *item_p;

    item_p = pop_item(name_p);

    if (memcmp(&item_p->data[0], buf_p, item_p->size) != 0) {
        printf("Mock item %s failed!\n", name_p);
        printf("Expected: \n");
        ml_hexdump(&item_p->data[0], item_p->size);
        printf("Actual: \n");
        ml_hexdump(buf_p, item_p->size);
        FAIL();
    }

    free(item_p);
}

void mock_finalize(void)
{
    struct item_t *item_p;

    item_p = list.next_p;

    if (item_p != NULL) {
        while (item_p->next_p != NULL) {
            printf("Unused mock item: %s\n", item_p->name_p);
            item_p = item_p->next_p;
        }

        FAIL();
    }
}

struct callback_t {
    const char *name_p;
    ml_shell_command_callback_t callback;
    struct callback_t *next_p;
};

static struct callback_t cmd_list = {
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
    item_p->next_p = cmd_list.next_p;
    cmd_list.next_p = item_p;
}

ml_shell_command_callback_t mock_get_callback(const char *name_p)
{
    struct callback_t *item_p;

    item_p = &cmd_list;

    while (item_p != NULL) {
        if (strcmp(item_p->name_p, name_p) == 0) {
            return (item_p->callback);
        }

        item_p = item_p->next_p;
    }

    NALA_TEST_FAILURE(
        nala_format("Shell command callback not found for %s.\n",
                    name_p));

    return (NULL);
}
