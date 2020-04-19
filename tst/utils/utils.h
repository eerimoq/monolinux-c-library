#ifndef UTILS_H
#define UTILS_H

#define UTILS_NAME_LEN_MAX   127
#define UTILS_UUID_LEN_MAX   128
#define UTILS_TYPE_NAME_MAX  15

struct utils_ioctl_t {
    uint32_t version[3];
    uint32_t data_size;
    uint32_t data_start;
    uint32_t target_count;
    int32_t open_count;
    uint32_t flags;
    uint32_t event_nr;
    uint32_t padding;
    uint64_t dev;
    char name[UTILS_NAME_LEN_MAX + 1];
    char uuid[UTILS_UUID_LEN_MAX + 1];
    char padding2[7];
};

struct utils_target_t {
    uint64_t sector_start;
    uint64_t length;
    int32_t status;
    uint32_t next;
    char target_type[UTILS_TYPE_NAME_MAX + 1];
};

struct utils_load_table_t {
    struct utils_ioctl_t ctl;
    struct utils_target_t target;
    char string[512];
};

#define UTILS_READONLY_FLAG     (1 << 0)
#define UTILS_EXISTS_FLAG       (1 << 2)
#define UTILS_SECURE_DATA_FLAG  (1 << 15)

int stdin_pipe(void);

void input(int fd, const char *string_p);

#endif
