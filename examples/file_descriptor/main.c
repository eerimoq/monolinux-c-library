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

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/eventfd.h>
#include "ml/ml.h"

/* Message ids. */
static ML_UID(timeout);

static void event_write(int *event_fd_p)
{
    uint64_t value;
    ssize_t size;

    value = 1;
    size = write(*event_fd_p, &value, sizeof(value));

    if (size != sizeof(value)) {
        exit(1);
    }
}

int main()
{
    struct ml_timer_t timer;
    struct ml_queue_t queue;
    int event_fd;
    uint64_t value;
    ssize_t size;
    struct ml_uid_t *uid_p;
    void *message_p;

    ml_init();
    ml_queue_init(&queue, 16);
    ml_timer_init(&timer, &timeout, &queue);

    event_fd = eventfd(0, EFD_SEMAPHORE);

    if (event_fd == -1) {
        return (1);
    }

    ml_queue_set_on_put(&queue,
                        (ml_queue_put_t)event_write,
                        &event_fd);
    ml_timer_start(&timer, 1000, 1000);

    while (true) {
        size = read(event_fd, &value, sizeof(value));

        if (size != sizeof(value)) {
            continue;
        }

        uid_p = ml_queue_get(&queue, (void **)&message_p);

        if (uid_p == &timeout) {
            printf("Timer expired.\n");
        } else {
            printf("Unknown message.\n");
        }

        ml_message_free(message_p);
    }

    return (0);
}
