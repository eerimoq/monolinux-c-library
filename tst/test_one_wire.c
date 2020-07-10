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

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <linux/netlink.h>
#include <sys/un.h>
#include "nala.h"
#include "ml/ml.h"

/* Convert packets. */
static uint8_t convert_request[] = {
    0x35, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x11, 0x00, 0x00, 0x00, 0x05, 0x00, 0x05, 0x00,
    0x28, 0x95, 0x32, 0x5d, 0x05, 0x00, 0x00, 0x33, 0x01, 0x00,
    0x01, 0x00, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t convert_response[] = {
    0x34, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x05, 0x00, 0x04, 0x00,
    0x28, 0x95, 0x32, 0x5d, 0x05, 0x00, 0x00, 0x33, 0x01, 0x00,
    0x00, 0x00
};

/* Read scratchpad packets. */
static uint8_t read_scratchpad_request[] = {
    0x42, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x1e, 0x00, 0x00, 0x00, 0x05, 0x00, 0x12, 0x00,
    0x28, 0x95, 0x32, 0x5d, 0x05, 0x00, 0x00, 0x33, 0x01, 0x00,
    0x01, 0x00, 0xbe, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
};

static uint8_t read_scratchpad_response[] = {
    0x3d, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x02, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
    0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x03, 0x00,
    0x00, 0x00, 0x19, 0x00, 0x00, 0x00, 0x05, 0x00, 0x0d, 0x00,
    0x28, 0x95, 0x32, 0x5d, 0x05, 0x00, 0x00, 0x33, 0x00, 0x00,
    0x09, 0x00, 0x59, 0x01, 0x4b, 0x46, 0x7f, 0xff, 0x07, 0x10,
    0xa2, 0x00, 0x00, 0x00
};

static int netlink_fds[2];

static void *netlink_main()
{
    uint8_t buf[1024];
    ssize_t res;

    /* Convert. */
    res = read(netlink_fds[1], &buf[0], sizeof(buf));
    ASSERT_EQ(res, sizeof(convert_request));
    ASSERT_MEMORY_EQ(&buf[0], &convert_request[0], sizeof(convert_request));

    res = write(netlink_fds[1], &convert_response[0], sizeof(convert_response));
    ASSERT_EQ(res, sizeof(convert_response));

    /* Read scratchpad. */
    res = read(netlink_fds[1], &buf[0], sizeof(buf));
    ASSERT_EQ(res, sizeof(read_scratchpad_request));
    ASSERT_MEMORY_EQ(&buf[0],
                     &read_scratchpad_request[0],
                     sizeof(read_scratchpad_request));

    res = write(netlink_fds[1],
                &read_scratchpad_response[0],
                sizeof(read_scratchpad_response));
    ASSERT_EQ(res, sizeof(read_scratchpad_response));

    return (NULL);
}

TEST(read_temperature)
{
    int res;
    float temperature;
    pthread_t pthread;

    ml_init();
    ml_one_wire_init();

    /* The mocked netlink connector socket. */
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_DGRAM, 0, &netlink_fds[0]), 0);
    socket_mock_once(AF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR, netlink_fds[0]);
    bind_mock_ignore_in_once(0);

    ASSERT_EQ(pthread_create(&pthread, NULL, netlink_main, NULL), 0);

    ml_one_wire_start();

    res = ml_one_wire_read_temperature(0x280000055d329533ull, &temperature);

    ASSERT_EQ(res, 0);
    ASSERT_EQ(temperature, 21.562500);
}

TEST(command_read_temperature)
{
    int res;
    pthread_t pthread;
    char command[64];

    ml_init();
    ml_shell_init();
    ml_one_wire_init();

    /* The mocked netlink connector socket. */
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_DGRAM, 0, &netlink_fds[0]), 0);
    socket_mock_once(AF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR, netlink_fds[0]);
    bind_mock_ignore_in_once(0);

    ASSERT_EQ(pthread_create(&pthread, NULL, netlink_main, NULL), 0);

    ml_shell_start();
    ml_one_wire_start();

    /* Read. */
    strcpy(&command[0], "ds18b20 read 280000055d329533");

    CAPTURE_OUTPUT(output1, errput1) {
        res = ml_shell_execute_command(&command[0], stdout);
    }

    ASSERT_EQ(res, 0);
    ASSERT_EQ(output1, "Temperature: 21.56 C\n");

    /* Usage. */
    strcpy(&command[0], "ds18b20");

    CAPTURE_OUTPUT(output2, errput2) {
        res = ml_shell_execute_command(&command[0], stdout);
    }

    ASSERT_EQ(res, -EINVAL);
    ASSERT_EQ(output2, "Usage: ds18b20 read <sensor-id>\n");
}
