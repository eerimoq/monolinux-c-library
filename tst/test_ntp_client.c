/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2020, Erik Moqvist
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
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include "nala.h"
#include "nala_mocks.h"
#include "ml/ml.h"

static uint8_t request[48] = {
    0x23, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t response[48] = {
    0x24, 0x03, 0x00, 0xe6, 0x00, 0x00, 0x00, 0x4b, 0x00, 0x00,
    0x00, 0x12, 0x0a, 0x41, 0x08, 0x04, 0xe2, 0x00, 0xbc, 0xa8,
    0xf9, 0xe3, 0xeb, 0xb3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xe2, 0x00, 0xbd, 0x16, 0x6b, 0x4a, 0x01, 0xac,
    0xe2, 0x00, 0xbd, 0x16, 0x6b, 0x4f, 0x7a, 0xf4
};

static void mock_prepare_getaddrinfo(struct addrinfo **info_pp,
                                     struct addrinfo *info_p,
                                     struct sockaddr_in *addr_p)
{
    struct addrinfo hints;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if (info_p != NULL) {
        memset(addr_p, 0, sizeof(*addr_p));

        info_p->ai_family = AF_INET;
        info_p->ai_socktype = SOCK_DGRAM;
        info_p->ai_protocol = IPPROTO_UDP;
        info_p->ai_addrlen = sizeof(*addr_p);
        info_p->ai_addr = (struct sockaddr *)addr_p;
        info_p->ai_next = NULL;
    }

    *info_pp = info_p;

    getaddrinfo_mock_once("foo", "123", 0);
    getaddrinfo_mock_set___req_in(&hints, sizeof(hints));
    getaddrinfo_mock_set___pai_out(info_pp, sizeof(info_pp));
}

static void mock_prepare_freeaddrinfo(struct addrinfo *info_p)
{
    freeaddrinfo_mock_once();
    freeaddrinfo_mock_set___ai_in_pointer(info_p);
}

TEST(getaddrinfo_error)
{
    getaddrinfo_mock_once("foo", "123", -1);

    ASSERT_EQ(ml_ntp_client_sync("foo"), -1);
}

TEST(getaddrinfo_empty)
{
    struct addrinfo *info_p;

    mock_prepare_getaddrinfo(&info_p, NULL, NULL);
    mock_prepare_freeaddrinfo(info_p);

    ASSERT_EQ(ml_ntp_client_sync("foo"), -1);
}

TEST(socket_error)
{
    struct addrinfo info;
    struct addrinfo *info_p;
    struct sockaddr_in addr;

    mock_prepare_getaddrinfo(&info_p, &info, &addr);
    socket_mock_once(AF_INET, SOCK_DGRAM, IPPROTO_UDP, -1);
    mock_prepare_freeaddrinfo(info_p);

    ASSERT_EQ(ml_ntp_client_sync("foo"), -1);
}

TEST(connect_error)
{
    struct addrinfo info;
    struct addrinfo *info_p;
    struct sockaddr_in addr;
    int fd;

    fd = 6;
    mock_prepare_getaddrinfo(&info_p, &info, &addr);
    socket_mock_once(AF_INET, SOCK_DGRAM, IPPROTO_UDP, fd);
    connect_mock_once(fd, sizeof(addr), -1);
    mock_prepare_freeaddrinfo(info_p);

    ASSERT_EQ(ml_ntp_client_sync("foo"), -1);
}

TEST(ok)
{
    struct addrinfo info;
    struct addrinfo *info_p;
    struct sockaddr_in addr;
    int fd;
    struct timespec ts;

    fd = 8;
    mock_prepare_getaddrinfo(&info_p, &info, &addr);
    socket_mock_once(AF_INET, SOCK_DGRAM, IPPROTO_UDP, fd);
    connect_mock_once(fd, sizeof(addr), 0);
    write_mock_once(fd, sizeof(request), sizeof(request));
    write_mock_set_buf_in(&request[0], sizeof(request));
    poll_mock_once(1, 5000, 1);
    read_mock_once(fd, 68, sizeof(response));
    read_mock_set_buf_out(&response[0], sizeof(response));
    close_mock_once(fd, 0);
    clock_settime_mock_once(CLOCK_REALTIME, 0);
    ts.tv_sec =  0x5e563e96;
    ts.tv_nsec =  0x18faed90;
    clock_settime_mock_set___tp_in(&ts, sizeof(ts));
    mock_prepare_freeaddrinfo(info_p);

    ASSERT_EQ(ml_ntp_client_sync("foo"), 0);
}

TEST(poll_timeout)
{
    struct addrinfo info;
    struct addrinfo *info_p;
    struct sockaddr_in addr;
    int fd;

    fd = 8;
    mock_prepare_getaddrinfo(&info_p, &info, &addr);
    socket_mock_once(AF_INET, SOCK_DGRAM, IPPROTO_UDP, fd);
    connect_mock_once(fd, sizeof(addr), 0);
    write_mock_once(fd, sizeof(request), sizeof(request));
    write_mock_set_buf_in(&request[0], sizeof(request));
    poll_mock_once(1, 5000, 0);
    close_mock_once(fd, 0);
    mock_prepare_freeaddrinfo(info_p);

    ASSERT_EQ(ml_ntp_client_sync("foo"), -1);
}

TEST(clock_settime_error)
{
    struct addrinfo info;
    struct addrinfo *info_p;
    struct sockaddr_in addr;
    int fd;
    struct timespec ts;

    fd = 8;
    mock_prepare_getaddrinfo(&info_p, &info, &addr);
    socket_mock_once(AF_INET, SOCK_DGRAM, IPPROTO_UDP, fd);
    connect_mock_once(fd, sizeof(addr), 0);
    write_mock_once(fd, sizeof(request), sizeof(request));
    write_mock_set_buf_in(&request[0], sizeof(request));
    poll_mock_once(1, 5000, 1);
    read_mock_once(fd, 68, sizeof(response));
    read_mock_set_buf_out(&response[0], sizeof(response));
    close_mock_once(fd, 0);
    clock_settime_mock_once(CLOCK_REALTIME, -1);
    ts.tv_sec =  0x5e563e96;
    ts.tv_nsec =  0x18faed90;
    clock_settime_mock_set___tp_in(&ts, sizeof(ts));
    mock_prepare_freeaddrinfo(info_p);

    ASSERT_EQ(ml_ntp_client_sync("foo"), -1);
}
