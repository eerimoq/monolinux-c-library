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
#include "ml/ml.h"

#define NTP_VERSION                               4
#define NTP_MODE_CLIENT                           3
#define NTP_MODE_SERVER                           4
#define NTP_PACKET_MIN                            48
#define NTP_PACKET_MAX                            68
#define JAN_1900_TO_1970                          2208988800

static int send_request(int sock)
{
    ssize_t res;
    uint8_t buf[NTP_PACKET_MIN];

    memset(&buf[0], 0, sizeof(buf));
    buf[0] = ((NTP_VERSION << 3) | NTP_MODE_CLIENT);

    res = write(sock, &buf[0], sizeof(buf));

    if (res != sizeof(buf)) {
        return (-1);
    }

    return (0);
}

static void ntp_time_to_timespec(uint8_t *time_p, struct timespec *ts_p)
{
    uint64_t secs;
    uint64_t nsecs;

    /* Seconds. */
    secs = (uint32_t)(((uint32_t)time_p[0] << 24)
                      | ((uint32_t)time_p[1] << 16)
                      | ((uint32_t)time_p[2] << 8)
                      | ((uint32_t)time_p[3] << 0));

    if ((time_p[0] & 0x80) == 0) {
        secs += (1ULL << 32);
    }

    secs -= JAN_1900_TO_1970;
    ts_p->tv_sec = secs;

    /* Nanoseconds. */
    nsecs = (uint32_t)(((uint32_t)time_p[4] << 24)
                       | ((uint32_t)time_p[5] << 16)
                       | ((uint32_t)time_p[6] << 8)
                       | ((uint32_t)time_p[7] << 0));
    nsecs *= 1000000000ULL;
    nsecs >>= 32;
    ts_p->tv_nsec = (long)nsecs;
}

static int receive_response(int sock, struct timespec *ts_p)
{
    uint8_t buf[NTP_PACKET_MAX];
    struct pollfd pfd[1];
    ssize_t size;
    int res;
    int mode;
    int version;

    pfd[0].fd = sock;
    pfd[0].events = POLLIN;

    res = poll(&pfd[0], 1, 5000);

    if ((res == -1) || (res != 1)) {
        return (-errno);
    }

    size = read(sock, &buf[0], sizeof(buf));

    if (size == -1) {
        return (-errno);
    } else if (size < NTP_PACKET_MIN) {
        return (-EPROTO);
    }

    mode = (buf[0] & 0x07);

    if (mode != NTP_MODE_SERVER) {
        return (-EPROTO);
    }

    version = ((buf[0] >> 3) & 0x07);

    if (version != NTP_VERSION) {
        return (-EPROTO);
    }

    ntp_time_to_timespec(&buf[32], ts_p);

    return (0);
}

static int try_sync(int sock)
{
    int res;
    struct timespec ts;

    res = send_request(sock);

    if (res != 0) {
        return (res);
    }

    res = receive_response(sock, &ts);

    if (res != 0) {
        return (res);
    }

    return (clock_settime(CLOCK_REALTIME, &ts));
}

static int try_sync_with_server(struct addrinfo *info_p)
{
    int res;
    int sock;

    sock = socket(info_p->ai_family,
                  info_p->ai_socktype,
                  info_p->ai_protocol);

    if (sock == -1) {
        return (-errno);
    }

    res = connect(sock, info_p->ai_addr, info_p->ai_addrlen);

    if (res == 0) {
        res = try_sync(sock);
    }

    close(sock);

    return (res);
}

int ml_ntp_client_sync(const char *address_p)
{
    int res;
    struct addrinfo hints;
    struct addrinfo *infolist_p;
    struct addrinfo *info_p;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    res = getaddrinfo(address_p, "123", &hints, &infolist_p);

    if (res != 0) {
        return (-1);
    }

    res = -1;

    for (info_p = infolist_p; info_p != NULL; info_p = info_p->ai_next) {
        res = try_sync_with_server(info_p);

        if (res == 0) {
            break;
        }
    }

    freeaddrinfo(infolist_p);

    return (res);
}
