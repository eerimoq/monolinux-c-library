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

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ftw.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "nala.h"
#include "mock.h"
#include "mock_libc.h"

void mock_push_nftw(const char *dirpath_p,
                    int nopenfd,
                    int flags,
                    const char *paths[],
                    mode_t modes[],
                    int length,
                    int res)
{
    int i;

    mock_push("nftw(dirpath_p)", dirpath_p, strlen(dirpath_p) + 1);
    mock_push("nftw(nopenfd)", &nopenfd, sizeof(nopenfd));
    mock_push("nftw(flags)", &flags, sizeof(flags));
    mock_push("nftw(): return (length)", &length, sizeof(length));

    for (i = 0; i < length; i++) {
        mock_push("nftw(): return (path)", &paths[i], sizeof(paths[0]));
        mock_push("nftw(): return (mode)", &modes[i], sizeof(modes[0]));
    }

    mock_push("nftw(): return (res)", &res, sizeof(res));
}

int __wrap_nftw(const char *dirpath_p,
                int (*fn) (const char *fpath_p,
                           const struct stat *sb_p,
                           int typeflag,
                           struct FTW *ftwbuf_p),
                int nopenfd,
                int flags)
{
    int res;
    int i;
    int length;
    const char *path_p;
    struct stat stat;

    mock_pop_assert("nftw(dirpath_p)", dirpath_p);
    mock_pop_assert("nftw(nopenfd)", &nopenfd);
    mock_pop_assert("nftw(flags)", &flags);
    mock_pop("nftw(): return (length)", &length);

    for (i = 0; i < length; i++) {
        mock_pop("nftw(): return (path)", &path_p);
        mock_pop("nftw(): return (mode)", &stat.st_mode);

        ASSERT_EQ(fn(path_p, &stat, 0, NULL), 0);
    }

    mock_pop("nftw(): return (res)", &res);

    return (res);
}
