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
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>
#include "nala.h"
#include "nala_mocks.h"
#include "ml/ml.h"

#define NAME_LEN_MAX   127
#define UUID_LEN_MAX   128
#define TYPE_NAME_MAX  15

struct ioctl_t {
    uint32_t version[3];
    uint32_t data_size;
    uint32_t data_start;
    uint32_t target_count;
    int32_t open_count;
    uint32_t flags;
    uint32_t event_nr;
    uint32_t padding;
    uint64_t dev;
    char name[NAME_LEN_MAX + 1];
    char uuid[UUID_LEN_MAX + 1];
    char padding2[7];
};

struct target_t {
    uint64_t sector_start;
    uint64_t length;
    int32_t status;
    uint32_t next;
    char target_type[TYPE_NAME_MAX + 1];
};

struct load_table_t {
    struct ioctl_t ctl;
    struct target_t target;
    char string[512];
};

#define READONLY_FLAG     (1 << 0)
#define EXISTS_FLAG       (1 << 2)
#define SECURE_DATA_FLAG  (1 << 15)

static void mock_push_create_device(int fd,
                                    int ioctl_res,
                                    int mknod_res)
{
    struct ioctl_t ctl;

    memset(&ctl, 0, sizeof(ctl));
    ctl.version[0] = 4;
    ctl.version[1] = 0;
    ctl.version[2] = 0;
    strncpy(&ctl.name[0], "name", sizeof(ctl.name));
    strncpy(&ctl.uuid[0], "00000000-1111-2222-3333-444444444444", sizeof(ctl.uuid));
    ctl.data_size = sizeof(ctl);
    ctl.flags = EXISTS_FLAG;
    ioctl_mock_once(fd, 3241737475, ioctl_res, "%p");
    ioctl_mock_set_va_arg_in_at(0, &ctl, sizeof(ctl), NULL);

    if (ioctl_res != 0) {
        ioctl_mock_set_errno(EPERM);

        return;
    }

    ctl.dev = 5;
    ioctl_mock_set_va_arg_out_at(0, &ctl, sizeof(ctl));
    ml_mknod_mock_once("/dev/mapper/00000000-1111-2222-3333-444444444444",
                       S_IFBLK,
                       5,
                       mknod_res);

    if (mknod_res != 0) {
        ml_mknod_mock_set_errno(EPERM);
    }
}

static void mock_push_load_table(int control_fd, int res)
{
    struct load_table_t params;

    memset(&params, 0, sizeof(params));
    params.ctl.version[0] = 4;
    params.ctl.version[1] = 0;
    params.ctl.version[2] = 0;
    params.ctl.data_size = sizeof(params);
    params.ctl.data_start = sizeof(params.ctl);
    strncpy(&params.ctl.name[0], "name", sizeof(params.ctl.name));
    params.ctl.target_count = 1;
    params.ctl.flags = (READONLY_FLAG | EXISTS_FLAG | SECURE_DATA_FLAG);
    params.target.sector_start = 0;
    params.target.length = 0x6000;
    strcpy(&params.target.target_type[0], "verity");
    strcpy(&params.string[0],
           "1 /dev/loop0 /dev/loop1 4096 4096 3072 1 sha256 "
           "0000000000000000000000000000000000000000000000000000000000000001 "
           "1111111111111111111111111111111111111111111111111111111111111112");
    ioctl_mock_once(control_fd, 3241737481, res, "%p");
    ioctl_mock_set_va_arg_in_at(0, &params, sizeof(params), NULL);

    if (res != 0) {
        ioctl_mock_set_errno(EPERM);
    }
}

static void mock_push_suspend_device(int control_fd, int res)
{
    struct ioctl_t ctl;

    memset(&ctl, 0, sizeof(ctl));
    ctl.version[0] = 4;
    ctl.version[1] = 0;
    ctl.version[2] = 0;
    ctl.data_size = sizeof(ctl);
    strncpy(&ctl.name[0], "name", sizeof(ctl.name));
    ioctl_mock_once(control_fd, 3241737478, res, "%p");
    ioctl_mock_set_va_arg_in_at(0, &ctl, sizeof(ctl), NULL);

    if (res != 0) {
        ioctl_mock_set_errno(EPERM);
    }
}

TEST(create_mapping_device_ok)
{
    int control_fd;

    control_fd = 7;
    ml_open_mock_once("/dev/mapper/control", O_RDWR, control_fd);
    mock_push_create_device(control_fd, 0, 0);
    mock_push_load_table(control_fd, 0);
    mock_push_suspend_device(control_fd, 0);
    close_mock_once(control_fd, 0);

    ASSERT_EQ(ml_device_mapper_create(
                  "name",
                  "00000000-1111-2222-3333-444444444444",
                  "/dev/loop0",
                  12 * 1024 * 1024,
                  "/dev/loop1",
                  4096,
                  "0000000000000000000000000000000000000000000000000000000000000001",
                  "1111111111111111111111111111111111111111111111111111111111111112"),
              0);
}

TEST(create_mapping_device_open_control_error)
{
    ml_open_mock_once("/dev/mapper/control", O_RDWR, -1);
    ml_open_mock_set_errno(EPERM);
    close_mock_none();

    ASSERT_EQ(ml_device_mapper_create(
                  "name",
                  "00000000-1111-2222-3333-444444444444",
                  "/dev/loop0",
                  12 * 1024 * 1024,
                  "/dev/loop1",
                  4096,
                  "0000000000000000000000000000000000000000000000000000000000000001",
                  "1111111111111111111111111111111111111111111111111111111111111112"),
              -1);
}

TEST(create_mapping_device_error_create_device_ioctl)
{
    int control_fd;

    control_fd = 7;
    ml_open_mock_once("/dev/mapper/control", O_RDWR, control_fd);
    mock_push_create_device(control_fd, -1, 0);
    close_mock_once(control_fd, 0);

    ASSERT_EQ(ml_device_mapper_create(
                  "name",
                  "00000000-1111-2222-3333-444444444444",
                  "/dev/loop0",
                  12 * 1024 * 1024,
                  "/dev/loop1",
                  4096,
                  "0000000000000000000000000000000000000000000000000000000000000001",
                  "1111111111111111111111111111111111111111111111111111111111111112"),
              -1);
}

TEST(create_mapping_device_error_create_device_mknod)
{
    int control_fd;

    control_fd = 7;
    ml_open_mock_once("/dev/mapper/control", O_RDWR, control_fd);
    mock_push_create_device(control_fd, 0, -1);
    close_mock_once(control_fd, 0);

    ASSERT_EQ(ml_device_mapper_create(
                  "name",
                  "00000000-1111-2222-3333-444444444444",
                  "/dev/loop0",
                  12 * 1024 * 1024,
                  "/dev/loop1",
                  4096,
                  "0000000000000000000000000000000000000000000000000000000000000001",
                  "1111111111111111111111111111111111111111111111111111111111111112"),
              -1);
}

TEST(create_mapping_device_error_load_table)
{
    int control_fd;

    control_fd = 7;
    ml_open_mock_once("/dev/mapper/control", O_RDWR, control_fd);
    mock_push_create_device(control_fd, 0, 0);
    mock_push_load_table(control_fd, -1);
    close_mock_once(control_fd, 0);

    ASSERT_EQ(ml_device_mapper_create(
                  "name",
                  "00000000-1111-2222-3333-444444444444",
                  "/dev/loop0",
                  12 * 1024 * 1024,
                  "/dev/loop1",
                  4096,
                  "0000000000000000000000000000000000000000000000000000000000000001",
                  "1111111111111111111111111111111111111111111111111111111111111112"),
              -1);
}

TEST(create_mapping_device_error_suspend_device)
{
    int control_fd;

    control_fd = 7;
    ml_open_mock_once("/dev/mapper/control", O_RDWR, control_fd);
    mock_push_create_device(control_fd, 0, 0);
    mock_push_load_table(control_fd, 0);
    mock_push_suspend_device(control_fd, -1);
    close_mock_once(control_fd, 0);

    ASSERT_EQ(ml_device_mapper_create(
                  "name",
                  "00000000-1111-2222-3333-444444444444",
                  "/dev/loop0",
                  12 * 1024 * 1024,
                  "/dev/loop1",
                  4096,
                  "0000000000000000000000000000000000000000000000000000000000000001",
                  "1111111111111111111111111111111111111111111111111111111111111112"),
              -1);
}
