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

void mock_push_setsockopt(int sockfd,
                          int level,
                          int optname,
                          const void *optval_p,
                          socklen_t optlen,
                          int res)
{
    mock_push("setsockopt(sockfd)", &sockfd, sizeof(sockfd));
    mock_push("setsockopt(level)", &level, sizeof(level));
    mock_push("setsockopt(optname)", &optname, sizeof(optname));
    mock_push("setsockopt(optval_p)", optval_p, optlen);
    mock_push("setsockopt(optlen)", &optlen, sizeof(optlen));
    mock_push("setsockopt(): return (res)", &res, sizeof(res));
}

int __wrap_setsockopt(int sockfd,
                      int level,
                      int optname,
                      const void *optval_p,
                      socklen_t optlen)
{
    int res;

    mock_pop_assert("setsockopt(sockfd)", &sockfd);
    mock_pop_assert("setsockopt(level)", &level);
    mock_pop_assert("setsockopt(optname)", &optname);
    mock_pop_assert("setsockopt(optval_p)", optval_p);
    mock_pop_assert("setsockopt(optlen)", &optlen);
    mock_pop("setsockopt(): return (res)", &res);

    return (res);
}

void mock_push_ioctl(int fd,
                     unsigned long request,
                     void *in_data_p,
                     void *out_data_p,
                     size_t data_size,
                     int res)
{
    mock_push("ioctl(fd)", &fd, sizeof(fd));
    mock_push("ioctl(request)", &request, sizeof(request));
    mock_push("ioctl(in_data_p)", in_data_p, data_size);
    mock_push("ioctl(out_data_p)", out_data_p, data_size);
    mock_push("ioctl(): return (res)", &res, sizeof(res));
}

void mock_push_ioctl_ifreq_ok(int fd,
                              unsigned long request,
                              struct ifreq *ifreq_p)
{
    mock_push_ioctl(fd,
                    request,
                    ifreq_p,
                    ifreq_p,
                    sizeof(*ifreq_p),
                    0);
}

int __wrap_ioctl(int fd,
                 unsigned long request,
                 void *data_p)
{
    int res;

    mock_pop_assert("ioctl(fd)", &fd);
    mock_pop_assert("ioctl(request)", &request);
    mock_pop_assert("ioctl(in_data_p)", data_p);
    mock_pop("ioctl(out_data_p)", data_p);
    mock_pop("ioctl(): return (res)", &res);

    return (res);
}

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

void mock_push_poll(struct pollfd *fds_p, nfds_t nfds, int timeout, int res)
{
    nfds_t i;

    mock_push("poll(nfds)", &nfds, sizeof(nfds));

    for (i = 0; i < nfds; i++) {
        mock_push("poll(fds_p->fd)", &fds_p[i].fd, sizeof(fds_p[i].fd));
        mock_push("poll(fds_p->events)", &fds_p[i].events, sizeof(fds_p[i].events));
        mock_push("poll(): return (fds_p->revents)",
                  &fds_p[i].revents,
                  sizeof(fds_p[i].revents));
    }

    mock_push("poll(timeout)", &timeout, sizeof(timeout));
    mock_push("poll(): return (res)", &res, sizeof(res));
}

int __wrap_poll(struct pollfd *fds_p, nfds_t nfds, int timeout)
{
    int res;
    nfds_t i;

    mock_pop_assert("poll(nfds)", &nfds);

    for (i = 0; i < nfds; i++) {
        mock_pop_assert("poll(fds_p->fd)", &fds_p[i].fd);
        mock_pop_assert("poll(fds_p->events)", &fds_p[i].events);
        mock_pop("poll(): return (fds_p->revents)", &fds_p[i].revents);
    }

    mock_pop_assert("poll(timeout)", &timeout);
    mock_pop("poll(): return (res)", &res);

    return (res);
}
