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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
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

#define DEV_CREATE_CMD     3
#define DEV_SUSPEND_CMD    6
#define TABLE_LOAD_CMD     9

#define IOCTL             0xfd

#define DEV_CREATE    _IOWR(IOCTL, DEV_CREATE_CMD, struct ioctl_t)
#define DEV_SUSPEND   _IOWR(IOCTL, DEV_SUSPEND_CMD, struct ioctl_t)
#define TABLE_LOAD    _IOWR(IOCTL, TABLE_LOAD_CMD, struct ioctl_t)

#define READONLY_FLAG     (1 << 0)
#define EXISTS_FLAG       (1 << 2)
#define SECURE_DATA_FLAG  (1 << 15)

static int create_device(int control_fd,
                         const char *mapping_name_p,
                         const char *mapping_uuid_p)
{
    struct ioctl_t ctl;
    int res;
    char buf[256];

    memset(&ctl, 0, sizeof(ctl));
    ctl.version[0] = 4;
    ctl.version[1] = 0;
    ctl.version[2] = 0;
    strncpy(&ctl.name[0], mapping_name_p, sizeof(ctl.name));
    strncpy(&ctl.uuid[0], mapping_uuid_p, sizeof(ctl.uuid));
    ctl.data_size = sizeof(ctl);
    ctl.flags = EXISTS_FLAG;

    res = ioctl(control_fd, DEV_CREATE, &ctl);

    if (res != 0) {
        ml_info("device-mapper: Failed to create mapping device '%s': %s",
                mapping_name_p,
                strerror(errno));

        return (res);
    }

    snprintf(&buf[0], sizeof(buf), "/dev/mapper/%s", mapping_uuid_p);

    res = ml_mknod(&buf[0], S_IFBLK, ctl.dev);

    if (res != 0) {
        ml_info("device-mapper: Failed to create node for mapping device '%s': %s",
                mapping_name_p,
                strerror(errno));
    }

    return (res);
}

static int load_table(int control_fd,
                      const char *mapping_name_p,
                      const char *data_dev_p,
                      size_t data_size,
                      const char *hash_tree_dev_p,
                      size_t hash_offset,
                      const char *root_hash_p,
                      const char *salt_p)
{
    int res;
    struct load_table_t params;

    memset(&params, 0, sizeof(params));
    params.ctl.version[0] = 4;
    params.ctl.version[1] = 0;
    params.ctl.version[2] = 0;
    params.ctl.data_size = sizeof(params);
    params.ctl.data_start = sizeof(params.ctl);
    strncpy(&params.ctl.name[0], mapping_name_p, sizeof(params.ctl.name));
    params.ctl.target_count = 1;
    params.ctl.flags = (READONLY_FLAG | EXISTS_FLAG | SECURE_DATA_FLAG);
    params.target.sector_start = 0;
    params.target.length = (data_size / 512);
    strcpy(&params.target.target_type[0], "verity");

    snprintf(&params.string[0],
             sizeof(params.string),
             "1 %s %s 4096 4096 %lu %lu sha256 %s %s",
             data_dev_p,
             hash_tree_dev_p,
             data_size / 4096,
             hash_offset / 4096,
             root_hash_p,
             salt_p);

    res = ioctl(control_fd, TABLE_LOAD, &params);

    if (res != 0) {
        ml_info(
            "device-mapper: Failed to load hash tree for mapping device '%s': %s",
            mapping_name_p,
            strerror(errno));
    }

    return (res);
}

static int suspend_device(int control_fd,
                          const char *mapping_name_p)
{
    struct ioctl_t ctl;
    int res;

    memset(&ctl, 0, sizeof(ctl));
    ctl.version[0] = 4;
    ctl.version[1] = 0;
    ctl.version[2] = 0;
    ctl.data_size = sizeof(ctl);
    strncpy(&ctl.name[0], mapping_name_p, sizeof(ctl.name));

    res = ioctl(control_fd, DEV_SUSPEND, &ctl);

    if (res != 0) {
        ml_info("device-mapper: Failed to suspend mapping device '%s': %s",
                mapping_name_p,
                strerror(errno));
    }

    return (res);
}

int ml_device_mapper_verity_create(const char *mapping_name_p,
                                   const char *mapping_uuid_p,
                                   const char *data_dev_p,
                                   size_t data_size,
                                   const char *hash_tree_dev_p,
                                   size_t hash_offset,
                                   const char *root_hash_p,
                                   const char *salt_p)
{
    int control_fd;
    int res;

    control_fd = ml_open("/dev/mapper/control", O_RDWR);

    if (control_fd == -1) {
        res = -errno;
        ml_info("device-mapper: Failed to open file '/dev/mapper/control': %s",
                strerror(errno));

        return (res);
    }

    res = create_device(control_fd, mapping_name_p, mapping_uuid_p);

    if (res != 0) {
        goto out;
    }

    res = load_table(control_fd,
                     mapping_name_p,
                     data_dev_p,
                     data_size,
                     hash_tree_dev_p,
                     hash_offset,
                     root_hash_p,
                     salt_p);

    if (res != 0) {
        goto out;
    }

    res = suspend_device(control_fd, mapping_name_p);

 out:
    close(control_fd);

    return (res);
}
