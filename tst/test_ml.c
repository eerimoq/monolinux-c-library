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
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/statvfs.h>
#include "nala.h"
#include "ml/ml.h"

static void init(void)
{
    ml_init();
}

TEST(strip)
{
    char string1[] = "1  ";
    char string2[] = " 1 ";
    char string3[] = "  1";
    char string4[] = "   ";
    char string5[] = "";
    char *begin_p;

    init();

    begin_p = ml_strip(string1, NULL);
    ASSERT_EQ(begin_p, "1");
    ASSERT_EQ(begin_p, &string1[0]);

    begin_p = ml_strip(string1, "1");
    ASSERT_EQ(begin_p, "");
    ASSERT_EQ(begin_p, &string1[0]);

    begin_p = ml_strip(string2, NULL);
    ASSERT_EQ(begin_p, "1");
    ASSERT_EQ(begin_p, &string2[1]);

    begin_p = ml_strip(string3, NULL);
    ASSERT_EQ(begin_p, "1");
    ASSERT_EQ(begin_p, &string3[2]);

    begin_p = ml_strip(string4, NULL);
    ASSERT_EQ(begin_p, "");
    ASSERT_EQ(begin_p, &string4[3]);

    begin_p = ml_strip(string5, NULL);
    ASSERT_EQ(begin_p, "");
}

TEST(lstrip)
{
    char string1[] = "1 ";
    char string2[] = " 1";
    char string3[] = "  ";
    char *begin_p;

    init();

    begin_p = ml_lstrip(string1, NULL);
    ASSERT_EQ(begin_p, "1 ");
    ASSERT_EQ(begin_p, &string1[0]);

    begin_p = ml_lstrip(string1, "1");
    ASSERT_EQ(begin_p, " ");
    ASSERT_EQ(begin_p, &string1[1]);

    begin_p = ml_lstrip(string2, NULL);
    ASSERT_EQ(begin_p, "1");
    ASSERT_EQ(begin_p, &string2[1]);

    begin_p = ml_lstrip(string3, NULL);
    ASSERT_EQ(begin_p, "");
    ASSERT_EQ(begin_p, &string3[2]);
}

TEST(rstrip)
{
    char string1[] = "1 ";
    char string2[] = " 1";
    char string3[] = "  ";

    init();

    ml_rstrip(string1, NULL);
    ASSERT_EQ(&string1[0], "1");

    ml_rstrip(string2, NULL);
    ASSERT_EQ(&string2[0], " 1");

    ml_rstrip(string2, "1");
    ASSERT_EQ(&string2[0], " ");

    ml_rstrip(string3, NULL);
    ASSERT_EQ(&string3[0], "");
}

TEST(hexdump_empty)
{
    init();

    CAPTURE_OUTPUT(output, errput) {
        ml_hexdump("", 0, stdout);
    }

    ASSERT_EQ(output, "");
}

TEST(hexdump_short)
{
    init();

    CAPTURE_OUTPUT(output, errput) {
        ml_hexdump("1", 1, stdout);
    }

    ASSERT_EQ(
        output,
        "00000000: 31                                              '1'\n");
}

TEST(hexdump_long)
{
    init();

    CAPTURE_OUTPUT(output, errput) {
        ml_hexdump(
            "110238\x00\x21h0112039jiajsFEWAFWE@#%!45eeeeeeeeeeeeeeeeeeeeeee"
            "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee\x01\x0ageeeeerG012345678901234"
            "567890123456789012345678901234567890123456789012345678901234567"
            "890123456789012345678901234567890123456789012345678901234567890"
            "12345678901234567",
            257,
            stdout);
    }

    ASSERT_EQ(
        output,
        "00000000: 31 31 30 32 33 38 00 21 68 30 31 31 32 30 33 39 '110238.!h0112039'\n"
        "00000010: 6a 69 61 6a 73 46 45 57 41 46 57 45 40 23 25 21 'jiajsFEWAFWE@#%!'\n"
        "00000020: 34 35 65 65 65 65 65 65 65 65 65 65 65 65 65 65 '45eeeeeeeeeeeeee'\n"
        "00000030: 65 65 65 65 65 65 65 65 65 65 65 65 65 65 65 65 'eeeeeeeeeeeeeeee'\n"
        "00000040: 65 65 65 65 65 65 65 65 65 65 65 65 65 65 65 65 'eeeeeeeeeeeeeeee'\n"
        "00000050: 65 65 65 65 65 65 65 65 65 01 0a 67 65 65 65 65 'eeeeeeeee..geeee'\n"
        "00000060: 65 72 47 30 31 32 33 34 35 36 37 38 39 30 31 32 'erG0123456789012'\n"
        "00000070: 33 34 35 36 37 38 39 30 31 32 33 34 35 36 37 38 '3456789012345678'\n"
        "00000080: 39 30 31 32 33 34 35 36 37 38 39 30 31 32 33 34 '9012345678901234'\n"
        "00000090: 35 36 37 38 39 30 31 32 33 34 35 36 37 38 39 30 '5678901234567890'\n"
        "000000a0: 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35 36 '1234567890123456'\n"
        "000000b0: 37 38 39 30 31 32 33 34 35 36 37 38 39 30 31 32 '7890123456789012'\n"
        "000000c0: 33 34 35 36 37 38 39 30 31 32 33 34 35 36 37 38 '3456789012345678'\n"
        "000000d0: 39 30 31 32 33 34 35 36 37 38 39 30 31 32 33 34 '9012345678901234'\n"
        "000000e0: 35 36 37 38 39 30 31 32 33 34 35 36 37 38 39 30 '5678901234567890'\n"
        "000000f0: 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35 36 '1234567890123456'\n"
        "00000100: 37                                              '7'\n");
}

TEST(hexdump_file_0_0)
{
    FILE *fin_p;

    init();

    fin_p = fopen("hexdump.in", "rb");
    ASSERT_NE(fin_p, NULL);

    CAPTURE_OUTPUT(output, errput) {
        ml_hexdump_file(fin_p, 0, 0, stdout);
    }

    fclose(fin_p);

    ASSERT_EQ(output, "");
}

TEST(hexdump_file_0_16)
{
    FILE *fin_p;

    init();

    fin_p = fopen("hexdump.in", "rb");
    ASSERT_NE(fin_p, NULL);

    CAPTURE_OUTPUT(output, errput) {
        ml_hexdump_file(fin_p, 0, 16, stdout);
    }

    fclose(fin_p);

    ASSERT_EQ(
        output,
        "00000000: 30 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35 '0123456789012345'\n");
}

TEST(hexdump_file_1_16)
{
    FILE *fin_p;

    init();

    fin_p = fopen("hexdump.in", "rb");
    ASSERT_NE(fin_p, NULL);

    CAPTURE_OUTPUT(output, errput) {
        ml_hexdump_file(fin_p, 1, 16, stdout);
    }

    fclose(fin_p);

    ASSERT_EQ(
        output,
        "00000001: 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35 36 '1234567890123456'\n");
}

TEST(hexdump_file_0_m1)
{
    FILE *fin_p;

    init();

    fin_p = fopen("hexdump.in", "rb");
    ASSERT_NE(fin_p, NULL);

    CAPTURE_OUTPUT(output, errput) {
        ml_hexdump_file(fin_p, 0, -1, stdout);
    }

    fclose(fin_p);

    ASSERT_EQ(
        output,
        "00000000: 30 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35 '0123456789012345'\n"
        "00000010: 36 37 38 39 30 31 32 33 34 35 36 37 38 39 30 31 '6789012345678901'\n"
        "00000020: 32 33 34 35 36 37 38 39                         '23456789'\n");
}

TEST(hexdump_file_1_m1)
{
    FILE *fin_p;

    init();

    fin_p = fopen("hexdump.in", "rb");
    ASSERT_NE(fin_p, NULL);

    CAPTURE_OUTPUT(output, errput) {
        ml_hexdump_file(fin_p, 1, -1, stdout);
    }

    fclose(fin_p);

    ASSERT_EQ(
        output,
        "00000001: 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35 36 '1234567890123456'\n"
        "00000011: 37 38 39 30 31 32 33 34 35 36 37 38 39 30 31 32 '7890123456789012'\n"
        "00000021: 33 34 35 36 37 38 39                            '3456789'\n");
}

TEST(hexdump_file_big)
{
    FILE *fin_p;

    init();

    fin_p = fopen("hexdump-big.in", "rb");
    ASSERT_NE(fin_p, NULL);

    CAPTURE_OUTPUT(output, errput) {
        /* Bigger than hexdump function buffer. */
        ml_hexdump_file(fin_p, 1, 350, stdout);
    }

    fclose(fin_p);

    ASSERT_EQ(
        output,
        "00000001: 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35 36 '1234567890123456'\n"
        "00000011: 37 38 39 30 31 32 33 34 35 36 37 38 39 30 31 32 '7890123456789012'\n"
        "00000021: 33 34 35 36 37 38 39 30 31 32 33 34 35 36 37 38 '3456789012345678'\n"
        "00000031: 39 30 31 32 33 34 35 36 37 38 39 30 31 32 33 34 '9012345678901234'\n"
        "00000041: 35 36 37 38 39 30 31 32 33 34 35 36 37 38 39 30 '5678901234567890'\n"
        "00000051: 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35 36 '1234567890123456'\n"
        "00000061: 37 38 39 30 31 32 33 34 35 36 37 38 39 30 31 32 '7890123456789012'\n"
        "00000071: 33 34 35 36 37 38 39 30 31 32 33 34 35 36 37 38 '3456789012345678'\n"
        "00000081: 39 30 31 32 33 34 35 36 37 38 39 30 31 32 33 34 '9012345678901234'\n"
        "00000091: 35 36 37 38 39 30 31 32 33 34 35 36 37 38 39 30 '5678901234567890'\n"
        "000000a1: 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35 36 '1234567890123456'\n"
        "000000b1: 37 38 39 30 31 32 33 34 35 36 37 38 39 30 31 32 '7890123456789012'\n"
        "000000c1: 33 34 35 36 37 38 39 30 31 32 33 34 35 36 37 38 '3456789012345678'\n"
        "000000d1: 39 30 31 32 33 34 35 36 37 38 39 30 31 32 33 34 '9012345678901234'\n"
        "000000e1: 35 36 37 38 39 30 31 32 33 34 35 36 37 38 39 30 '5678901234567890'\n"
        "000000f1: 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35 36 '1234567890123456'\n"
        "00000101: 37 38 39 30 31 32 33 34 35 36 37 38 39 30 31 32 '7890123456789012'\n"
        "00000111: 33 34 35 36 37 38 39 30 31 32 33 34 35 36 37 38 '3456789012345678'\n"
        "00000121: 39 30 31 32 33 34 35 36 37 38 39 30 31 32 33 34 '9012345678901234'\n"
        "00000131: 35 36 37 38 39 30 31 32 33 34 35 36 37 38 39 30 '5678901234567890'\n"
        "00000141: 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35 36 '1234567890123456'\n"
        "00000151: 37 38 39 30 31 32 33 34 35 36 37 38 39 30       '78901234567890'\n");
}

TEST(print_file)
{
    init();

    CAPTURE_OUTPUT(output, errput) {
        ml_print_file("hexdump.in", stdout);
    }

    ASSERT_EQ(output, "0123456789012345678901234567890123456789");
}

TEST(print_uptime)
{
    init();

    CAPTURE_OUTPUT(output, errput) {
        ml_print_uptime();
    }

    ASSERT_SUBSTRING(output, "Uptime:");
    ASSERT_SUBSTRING(output, "\n");
}

static ML_UID(m1);

static struct ml_queue_t queue;

TEST(bus)
{
    struct ml_uid_t *uid_p;
    int *message_p;
    int *rmessage_p;

    init();

    ml_queue_init(&queue, 1);
    ml_subscribe(&queue, &m1);

    message_p = ml_message_alloc(&m1, sizeof(int));
    *message_p = 9;
    ml_broadcast(message_p);

    uid_p = ml_queue_get(&queue, (void **)&rmessage_p);
    ASSERT_EQ(ml_uid_str(uid_p), ml_uid_str(&m1));
    ASSERT_EQ(*rmessage_p, 9);
    ml_message_free(rmessage_p);
}

TEST(ml_mount_ok)
{
    init();

    mount_mock_once("a", "b", "c", 0, 0);
    mount_mock_set_data_in_pointer(NULL);

    ASSERT_EQ(ml_mount("a", "b", "c", 0, NULL), 0);
}

TEST(insmod)
{
    int fd;

    init();

    fd = 99;
    open_mock_once("foo.ko", O_RDONLY | O_CLOEXEC, fd, "");
    ml_finit_module_mock_once(fd, "", 0, 0);
    close_mock_once(fd, 0);

    ASSERT_EQ(ml_insert_module("foo.ko", ""), 0);
}

TEST(insmod_open_error)
{
    init();

    open_mock_once("foo.ko", O_RDONLY | O_CLOEXEC, -1, "");
    open_mock_set_errno(ENOENT);
    close_mock_none();

    ASSERT_EQ(ml_insert_module("foo.ko", ""), -ENOENT);
}

TEST(insmod_finit_module_error)
{
    int fd;

    init();

    fd = 99;
    open_mock_once("foo.ko", O_RDONLY | O_CLOEXEC, fd, "");
    ml_finit_module_mock_once(fd, "", 0, -1);
    ml_finit_module_mock_set_errno(EEXIST);
    close_mock_once(fd, 0);

    ASSERT_EQ(ml_insert_module("foo.ko", ""), -EEXIST);
}

static struct ml_queue_t test_spawn_queue;
static ML_UID(test_spawn_message_id);

static void test_spawn_entry(void *arg_p)
{
    ml_queue_put(&test_spawn_queue, arg_p);
}

TEST(spawn)
{
    void *message_p;

    init();
    ml_queue_init(&test_spawn_queue, 10);
    message_p = ml_message_alloc(&test_spawn_message_id, 0);
    ml_spawn(test_spawn_entry, message_p);
    ASSERT_EQ(ml_queue_get(&test_spawn_queue, &message_p),
              &test_spawn_message_id);
    ml_message_free(message_p);
}

static void mock_prepare_read_cpus_stats(char **lines_pp, int length)
{
    int i;
    FILE file;

    fopen_mock_once("/proc/stat", "r", &file);

    for (i = 0; i < length; i++) {
        fgets_mock_once(NULL, 128, lines_pp[i]);
        fgets_mock_set_stream_in_pointer(&file);
        fgets_mock_ignore_s_in();
        fgets_mock_set_s_out(lines_pp[i], strlen(lines_pp[i]) + 1);
    }

    fclose_mock_once(0);
    fclose_mock_set_stream_in_pointer(&file);
}

TEST(get_cpus_stats_4_cpus)
{
    struct ml_cpu_stats_t stats[6];
    char *lines[2][6] = {
        {
            "cpu  9315241 7265 1323811 111314353 59144 0 334241 0 0 0\n",
            "cpu0 2464444 770 322927 27697262 20463 0 99753 0 0 0\n",
            "cpu1 2280832 1324 334732 27883573 13928 0 104802 0 0 0\n",
            "cpu2 2302634 2912 331600 27858568 12988 0 55300 0 0 0\n",
            "cpu3 2267331 2258 334550 27874949 11762 0 74385 0 0 0\n",
            "intr ...\n"
        },
        {
            "cpu  9315258 7265 1323812 111314374 59144 0 334241 0 0 0\n",
            "cpu0 2464448 770 322928 27697268 20463 0 99753 0 0 0\n",
            "cpu1 2280838 1324 334732 27883576 13928 0 104802 0 0 0\n",
            "cpu2 2302636 2912 331600 27858574 12988 0 55300 0 0 0\n",
            "cpu3 2267335 2258 334550 27874954 11762 0 74385 0 0 0\n"
        }
    };

    mock_prepare_read_cpus_stats(&lines[0][0], 6);
    usleep_mock_once(500000, 0);
    mock_prepare_read_cpus_stats(&lines[1][0], 5);

    ASSERT_EQ(ml_get_cpus_stats(&stats[0], membersof(stats)), 5);

    ASSERT_EQ(stats[0].user, 43);
    ASSERT_EQ(stats[0].system, 2);
    ASSERT_EQ(stats[0].idle, 53);
    ASSERT_EQ(stats[1].user, 36);
    ASSERT_EQ(stats[1].system, 9);
    ASSERT_EQ(stats[1].idle, 54);
    ASSERT_EQ(stats[2].user, 66);
    ASSERT_EQ(stats[2].system, 0);
    ASSERT_EQ(stats[2].idle, 33);
    ASSERT_EQ(stats[3].user, 25);
    ASSERT_EQ(stats[3].system, 0);
    ASSERT_EQ(stats[3].idle, 75);
    ASSERT_EQ(stats[4].user, 44);
    ASSERT_EQ(stats[4].system, 0);
    ASSERT_EQ(stats[4].idle, 55);
}

TEST(get_cpus_stats_4_cpus_get_one)
{
    struct ml_cpu_stats_t stats;
    char *lines[2][6] = {
        {
            "cpu  9315241 7265 1323811 111314353 59144 0 334241 0 0 0\n",
            "cpu0 2464444 770 322927 27697262 20463 0 99753 0 0 0\n",
            "cpu1 2280832 1324 334732 27883573 13928 0 104802 0 0 0\n",
            "cpu2 2302634 2912 331600 27858568 12988 0 55300 0 0 0\n",
            "cpu3 2267331 2258 334550 27874949 11762 0 74385 0 0 0\n",
            "intr ...\n"
        },
        {
            "cpu  9315258 7265 1323812 111314374 59144 0 334241 0 0 0\n",
            "cpu0 2464448 770 322928 27697268 20463 0 99753 0 0 0\n",
            "cpu1 2280838 1324 334732 27883576 13928 0 104802 0 0 0\n",
            "cpu2 2302636 2912 331600 27858574 12988 0 55300 0 0 0\n",
            "cpu3 2267335 2258 334550 27874954 11762 0 74385 0 0 0\n"
        }
    };

    mock_prepare_read_cpus_stats(&lines[0][0], 1);
    usleep_mock_once(500000, 0);
    mock_prepare_read_cpus_stats(&lines[1][0], 1);

    ASSERT_EQ(ml_get_cpus_stats(&stats, 1), 1);

    ASSERT_EQ(stats.user, 43);
    ASSERT_EQ(stats.system, 2);
    ASSERT_EQ(stats.idle, 53);
}

TEST(get_cpus_stats_4_cpus_fopen_error)
{
    struct ml_cpu_stats_t stats;

    fopen_mock_once("/proc/stat", "r", NULL);
    fopen_mock_set_errno(ENOENT);
    fclose_mock_none();

    ASSERT_EQ(ml_get_cpus_stats(&stats, 1), -ENOENT);
}

TEST(get_cpus_stats_bad_proc_stat_contents)
{
    struct ml_cpu_stats_t stats;
    char *lines[1] = { "Something unexpected\n" };

    mock_prepare_read_cpus_stats(&lines[0], 1);
    usleep_mock_none();

    ASSERT_EQ(ml_get_cpus_stats(&stats, 1), -EGENERAL);
}

TEST(file_write_string)
{
    FILE file;

    fopen_mock_once("foo.txt", "w", &file);
    fwrite_mock_once(3, 1, 1);
    fwrite_mock_set_ptr_in("bar", 3);
    fclose_mock_once(0);

    ASSERT_EQ(ml_file_write_string("foo.txt", "bar"), 0);
}

TEST(file_write_string_open_failure)
{
    fopen_mock_once("foo.txt", "w", NULL);
    fopen_mock_set_errno(5);
    fclose_mock_none();

    ASSERT_EQ(ml_file_write_string("foo.txt", "bar"), -5);
}

TEST(file_write_string_write_failure)
{
    FILE file;

    fopen_mock_once("foo.txt", "w", &file);
    fwrite_mock_once(3, 1, 0);
    fwrite_mock_set_ptr_in("bar", 3);
    fclose_mock_once(0);

    ASSERT_EQ(ml_file_write_string("foo.txt", "bar"), -EGENERAL);
}

TEST(file_read)
{
    FILE file;
    uint8_t buf[3];

    buf[0] = 99;
    buf[1] = 98;
    buf[2] = 97;
    fopen_mock_once("foo.txt", "rb", &file);
    fread_mock_once(3, 1, 1);
    fread_mock_set_ptr_out(&buf[0], sizeof(buf));
    fclose_mock_once(0);

    memset(&buf[0], 0, sizeof(buf));
    ASSERT_EQ(ml_file_read("foo.txt", &buf[0], sizeof(buf)), 0);
    ASSERT_EQ(buf[0], 99);
    ASSERT_EQ(buf[1], 98);
    ASSERT_EQ(buf[2], 97);
}

TEST(file_read_open_failure)
{
    uint8_t buf[3];

    fopen_mock_once("foo.txt", "rb", NULL);
    fopen_mock_set_errno(5);
    fclose_mock_none();

    ASSERT_EQ(ml_file_read("foo.txt", &buf[0], sizeof(buf)), -5);
}

TEST(file_read_read_failure)
{
    FILE file;
    uint8_t buf[3];

    fopen_mock_once("foo.txt", "rb", &file);
    fread_mock_once(3, 1, 0);
    fclose_mock_once(0);

    ASSERT_EQ(ml_file_read("foo.txt", &buf[0], sizeof(buf)), -EGENERAL);
}

TEST(dd)
{
    int fdin;
    int fdout;
    char buf[1001];
    int i;

    fdin = 30;
    fdout = 40;

    for (i = 0; i < 1001; i++) {
        buf[i] = i;
    }

    open_mock_once("a", O_RDONLY, fdin, "");
    open_mock_once("b", O_WRONLY | O_CREAT | O_TRUNC, fdout, "");

    /* First 500 bytes. */
    read_mock_once(fdin, 500, 500);
    read_mock_set_buf_out(&buf[0], 500);
    write_mock_once(fdout, 500, 500);
    write_mock_set_buf_in(&buf[0], 500);

    /* Next 500 bytes. */
    read_mock_once(fdin, 500, 500);
    read_mock_set_buf_out(&buf[500], 500);
    write_mock_once(fdout, 500, 500);
    write_mock_set_buf_in(&buf[500], 500);

    /* Last byte. */
    read_mock_once(fdin, 1, 1);
    read_mock_set_buf_out(&buf[1000], 1);
    write_mock_once(fdout, 1, 1);
    write_mock_set_buf_in(&buf[1000], 1);

    close_mock_once(fdin, 0);
    close_mock_once(fdout, 0);

    ASSERT_EQ(ml_dd("a", "b", 1001, 500), 0);
}

TEST(dd_infile_open_error)
{
    open_mock_once("a", O_RDONLY, -1, "");
    open_mock_set_errno(ENOENT);
    close_mock_none();

    ASSERT_EQ(ml_dd("a", "b", 1000, 1000), -ENOENT);
}

TEST(dd_outfile_open_error)
{
    int fdin;

    fdin = 4;
    open_mock_once("a", O_RDONLY, fdin, "");
    open_mock_once("b", O_WRONLY | O_CREAT | O_TRUNC, -1, "");
    open_mock_set_errno(ENOENT);
    close_mock_once(fdin, 0);

    ASSERT_EQ(ml_dd("a", "b", 1000, 1000), -ENOENT);
}

TEST(dd_read_error)
{
    int fdin;
    int fdout;

    fdin = 30;
    fdout = 40;

    open_mock_once("a", O_RDONLY, fdin, "");
    open_mock_once("b", O_WRONLY | O_CREAT | O_TRUNC, fdout, "");
    read_mock_once(fdin, 1, -1);
    read_mock_set_errno(EACCES);
    close_mock_once(fdout, 0);
    close_mock_once(fdin, 0);

    ASSERT_EQ(ml_dd("a", "b", 1, 1), -EACCES);
}

TEST(dd_short_read_error)
{
    int fdin;
    int fdout;

    fdin = 30;
    fdout = 40;

    open_mock_once("a", O_RDONLY, fdin, "");
    open_mock_once("b", O_WRONLY | O_CREAT | O_TRUNC, fdout, "");
    read_mock_once(fdin, 1, 0);
    read_mock_set_errno(EACCES);
    close_mock_once(fdout, 0);
    close_mock_once(fdin, 0);

    ASSERT_EQ(ml_dd("a", "b", 1, 1), -EGENERAL);
}

TEST(dd_write_error)
{
    int fdin;
    int fdout;

    fdin = 30;
    fdout = 40;

    open_mock_once("a", O_RDONLY, fdin, "");
    open_mock_once("b", O_WRONLY | O_CREAT | O_TRUNC, fdout, "");
    read_mock_once(fdin, 1, 1);
    write_mock_once(fdout, 1, -1);
    write_mock_set_errno(EACCES);
    close_mock_once(fdout, 0);
    close_mock_once(fdin, 0);

    ASSERT_EQ(ml_dd("a", "b", 1, 1), -EACCES);
}

TEST(file_system_space_usage)
{
    unsigned long total;
    unsigned long used;
    unsigned long free;
    struct statvfs stat;

    stat.f_bsize = 512;
    stat.f_blocks = 40000;
    stat.f_bfree = 10000;
    statvfs_mock_once("/mnt/foo", 0);
    statvfs_mock_set_buf_out(&stat, sizeof(stat));

    ASSERT_EQ(ml_file_system_space_usage("/mnt/foo", &total, &used, &free), 0);
    ASSERT_EQ(total, 19);
    ASSERT_EQ(used, 14);
    ASSERT_EQ(free, 5);
}

TEST(file_system_space_usage_statvfs_error)
{
    unsigned long total;
    unsigned long used;
    unsigned long free;

    statvfs_mock_once("/mnt/foo", -1);
    statvfs_mock_set_errno(EACCES);

    ASSERT_EQ(ml_file_system_space_usage("/mnt/foo", &total, &used, &free),
              -EACCES);
}

static void mock_prepare_write_info_file(const char *path_p)
{
    fprintf_mock_once("%s:\n\n", 0, "%s", path_p);
    ml_print_file_mock_once(path_p);
    fprintf_mock_once("%s\n", 0, "%s", "");
}

TEST(finalize_coredump)
{
    FILE fcore;
    FILE flog;
    FILE finfo;
    int log_fd;
    uint8_t coredump[] = { 9, 8, 7, 6, 5, 4, 3, 2, 1, 0 };

    log_fd = 20;

    /* Find a slot. */
    mkdir_mock_once("/disk/coredumps", 0777, 0);
    chdir_mock_once("/disk/coredumps", 0);
    lstat_mock_once("0", -1);

    /* Create output folder. */
    mkdir_mock_once("0", 0777, 0);
    chdir_mock_once("0", 0);

    /* Write core dump. */
    fopen_mock_once("core", "wb", &fcore);
    read_mock_once(STDIN_FILENO, 1024, sizeof(coredump));
    read_mock_set_buf_out(&coredump[0], sizeof(coredump));
    fwrite_mock_once(1, sizeof(coredump), sizeof(coredump));
    fwrite_mock_set_ptr_in(&coredump[0], sizeof(coredump));
    read_mock_once(STDIN_FILENO, 1024, -1);
    fclose_mock_once(0);
    fclose_mock_set_stream_in_pointer(&fcore);

    /* Write log. */
    fopen_mock_once("log.txt", "w", &flog);
    open_mock_once("/dev/kmsg", O_RDONLY | O_NONBLOCK, log_fd, "");
    read_mock_once(log_fd, 1023, 5);
    read_mock_set_buf_out("1,2 foo", 8);
    ml_print_kernel_message_mock_once("1,2 foo");
    read_mock_once(log_fd, 1023, -1);
    read_mock_set_errno(EAGAIN);
    close_mock_once(log_fd, 0);
    fclose_mock_once(0);
    fclose_mock_set_stream_in_pointer(&flog);

    /* Write info. */
    fopen_mock_once("info.txt", "w", &finfo);
    mock_prepare_write_info_file("/proc/version");
    mock_prepare_write_info_file("/proc/cmdline");
    mock_prepare_write_info_file("/proc/uptime");
    mock_prepare_write_info_file("/proc/loadavg");
    mock_prepare_write_info_file("/proc/meminfo");
    mock_prepare_write_info_file("/proc/interrupts");
    mock_prepare_write_info_file("/proc/diskstats");
    mock_prepare_write_info_file("/proc/partitions");
    mock_prepare_write_info_file("/proc/mounts");
    mock_prepare_write_info_file("/proc/cpuinfo");
    mock_prepare_write_info_file("/proc/modules");
    mock_prepare_write_info_file("/proc/ioports");
    mock_prepare_write_info_file("/proc/iomem");
    mock_prepare_write_info_file("/proc/stat");
    mock_prepare_write_info_file("/proc/net/route");
    mock_prepare_write_info_file("/proc/net/tcp");
    mock_prepare_write_info_file("/proc/net/udp");
    mock_prepare_write_info_file("/proc/net/netstat");
    mock_prepare_write_info_file("/proc/net/sockstat");
    mock_prepare_write_info_file("/proc/net/protocols");
    fclose_mock_once(0);
    fclose_mock_set_stream_in_pointer(&finfo);

    /* Sync and exit. */
    sync_mock_once();
    exit_mock_once(0);
    exit_mock_set_callback(nala_exit);

    ml_finalize_coredump("/disk/coredumps", 3);
}

TEST(finalize_coredump_no_slot_found)
{
    mkdir_mock_once("/disk/coredumps", 0777, 0);
    chdir_mock_once("/disk/coredumps", 0);
    lstat_mock_once("0", 0);
    lstat_mock_once("1", 0);
    lstat_mock_once("2", 0);
    exit_mock_once(0);
    exit_mock_set_callback(nala_exit);

    ml_finalize_coredump("/disk/coredumps", 3);
}

TEST(finalize_coredump_error_open_output_files)
{
    /* Find a slot. */
    mkdir_mock_once("/disk/coredumps", 0777, 0);
    chdir_mock_once("/disk/coredumps", 0);
    lstat_mock_once("0", -1);

    /* Create output folder. */
    mkdir_mock_once("0", 0777, 0);
    chdir_mock_once("0", 0);

    /* Core and log open error. */
    fopen_mock_once("core", "wb", NULL);
    fopen_mock_once("log.txt", "w", NULL);
    fopen_mock_once("info.txt", "w", NULL);
    fwrite_mock_none();
    fclose_mock_none();

    /* Sync and exit. */
    sync_mock_once();
    exit_mock_once(0);
    exit_mock_set_callback(nala_exit);

    ml_finalize_coredump("/disk/coredumps", 3);
}

TEST(finalize_coredump_error_open_dev_kmsg)
{
    FILE flog;

    /* Find a slot. */
    mkdir_mock_once("/disk/coredumps", 0777, 0);
    chdir_mock_once("/disk/coredumps", 0);
    lstat_mock_once("0", -1);

    /* Create output folder. */
    mkdir_mock_once("0", 0777, 0);
    chdir_mock_once("0", 0);

    /* Skip coredump. */
    fopen_mock_once("core", "wb", NULL);

    /* /dev/kmsg open error. */
    fopen_mock_once("log.txt", "w", &flog);
    open_mock_once("/dev/kmsg", O_RDONLY | O_NONBLOCK, -1, "");
    open_mock_set_errno(ENODEV);
    read_mock_none();
    fprintf_mock_once("*** Failed to open /dev/kmsg with error '%s'. ***\n",
                      0,
                      "%s",
                      "No such device");
    close_mock_none();
    fclose_mock_once(0);
    fclose_mock_set_stream_in_pointer(&flog);

    /* info.txt open error. */
    fopen_mock_once("info.txt", "w", NULL);

    /* Sync and exit. */
    sync_mock_once();
    exit_mock_once(0);
    exit_mock_set_callback(nala_exit);

    ml_finalize_coredump("/disk/coredumps", 3);
}

TEST(finalize_coredump_error_chdir)
{
    mkdir_mock_once("/disk/coredumps", 0777, 0);
    chdir_mock_once("/disk/coredumps", -1);
    lstat_mock_none();
    exit_mock_once(0);
    exit_mock_set_callback(nala_exit);

    ml_finalize_coredump("/disk/coredumps", 3);
}
