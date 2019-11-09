/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Erik Moqvist
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
 * This file is part of the traceback project.
 */

#include <stdio.h>
#include <execinfo.h>
#include "traceback.h"
#include <stdlib.h>
#include <unistd.h>

#define DEPTH_MAX 100

void traceback_print(void)
{
    int depth;
    void *addresses[DEPTH_MAX];
    char exe[256];
    char command[384];
    ssize_t size;
    int res;
    int i;

    depth = backtrace(&addresses[0], DEPTH_MAX);

    printf("Traceback (most recent call last):\n");

    size = readlink("/proc/self/exe", &exe[0], sizeof(exe) - 1);

    if (size == -1) {
        printf("No executable found!\n");

        return;
    }

    exe[size] = '\0';

    for (i = (depth - 1); i >= 0; i--) {
        printf("  ");
        fflush(stdout);

        snprintf(&command[0],
                 sizeof(command),
                 "addr2line -f -p -e %s %p",
                 &exe[0],
                 addresses[i]);
        command[sizeof(command) - 1] = '\0';

        res = system(&command[0]);

        if (res == -1) {
            return;
        } else if (WIFEXITED(res)) {
            if (WEXITSTATUS(res) != 0) {
                return;
            }
        } else {
            return;
        }
    }
}
