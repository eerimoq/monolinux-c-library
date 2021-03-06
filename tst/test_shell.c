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
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <ftw.h>
#include <fcntl.h>
#include <sys/sysmacros.h>
#include <linux/i2c-dev.h>
#include <sys/mount.h>
#include <mntent.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include "nala.h"
#include "utils/utils.h"
#include "ml/ml.h"

#define ESC "\x1b"
#define BACKSPACE "\x08"

static int init_and_start(void)
{
    int fd;

    ml_shell_init();
    fd = stdin_pipe();
    ml_shell_start();

    return (fd);
}

static int command_hello(int argc, const char *argv[], FILE *fout_p)
{
    const char *name_p;

    if (argc == 2) {
        name_p = argv[1];
    } else {
        name_p = "stranger";
    }

    fprintf(fout_p, "Hello %s!\n", name_p);

    return (0);
}

TEST(various_commands)
{
    int fd;

    ml_shell_init();
    ml_shell_register_command("hello", "My command.", command_hello);
    fd = stdin_pipe();
    ml_shell_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "help\n");
        input(fd, "history\n");
        input(fd, "hello\n");
        input(fd, "hello Foo\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "help\n"
              "Cursor movement\n"
              "\n"
              "         LEFT   Go left one character.\n"
              "        RIGHT   Go right one character.\n"
              "  HOME/Ctrl+A   Go to the beginning of the line.\n"
              "   END/Ctrl+E   Go to the end of the line.\n"
              "\n"
              "Edit\n"
              "\n"
              "        Alt+D   Delete the word at the cursor.\n"
              "       Ctrl+D   Delete the chracter at the cursor.\n"
              "       Ctrl+K   Cut the line from cursor to end.\n"
              "       Ctrl+T   Swap the last two characters before the "
              "cursor (typo).\n"
              "          TAB   Tab completion for command names.\n"
              "    BACKSPACE   Delete the character before the cursor.\n"
              "\n"
              "History\n"
              "\n"
              "           UP   Previous command.\n"
              "         DOWN   Next command.\n"
              "       Ctrl+R   Recall the last command including the specified "
              "character(s)\n"
              "                searches the command history as you type.\n"
              "       Ctrl+G   Escape from history searching mode.\n"
              "\n"
              "Commands\n"
              "\n"
              "          cat   Print a file.\n"
              "         date   Print current date.\n"
              "           dd   File copy.\n"
              "           df   Disk space usage.\n"
              "        dmesg   Print the kernel ring buffer.\n"
              "         exit   Shell exit.\n"
              "         find   Find files and folders.\n"
              "         halt   Halt the system.\n"
              "        hello   My command.\n"
              "         help   Print this help.\n"
              "      hexdump   Hexdump a file.\n"
              "      history   List command history.\n"
              "          i2c   I2C bus commands.\n"
              "       insmod   Insert a kernel module.\n"
              "           ll   List detailed directory contents.\n"
              "          log   Log control.\n"
              "           ls   List directory contents.\n"
              "        mkdir   Create a directory.\n"
              "        mknod   Create a node.\n"
              "        mount   Mount a filesystem.\n"
              "     ntp_sync   NTP time sync.\n"
              "     poweroff   Power off the system.\n"
              "        print   Print to file.\n"
              "       reboot   Reboot the system.\n"
              "           rm   Remove files and directories.\n"
              "        rmmod   Remove a kernel module.\n"
              "      suicide   Process suicide.\n"
              "         sync   Synchronize cached writes to persistent storage.\n"
              "          top   System status.\n"
              "       umount   Unmount a filesystem.\n"
              "OK\n"
              "$ history\n"
              "1: help\n"
              "2: history\n"
              "OK\n"
              "$ hello\n"
              "Hello stranger!\n"
              "OK\n"
              "$ hello Foo\n"
              "Hello Foo!\n"
              "OK\n"
              "$ exit\n");
}

TEST(command_ls)
{
    int fd;

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "ls\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_NOT_SUBSTRING(output, "..");
    ASSERT_SUBSTRING(output, "OK\n$ ");
}

TEST(command_ll)
{
    int fd;
    DIR *dir_p;
    struct dirent dirent;
    struct stat statbuf;

    fd = init_and_start();

    dir_p = nala_alloc(1);
    opendir_mock_once(".", dir_p);
    memset(&dirent, 0, sizeof(dirent));
    strcpy(&dirent.d_name[0], "foo");
    readdir_mock_once(&dirent);
    memset(&statbuf, 0, sizeof(statbuf));
    statbuf.st_mtim.tv_sec = 1000000000;
    statbuf.st_size = 104;
    lstat_mock_once("./foo", 0);
    lstat_mock_set_buf_out(&statbuf, sizeof(statbuf));
    readdir_mock_once(NULL);
    closedir_mock_once(0);

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "ll\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "ll\n"
              "- 2001-09-09 01:46:40      104 foo\n"
              "OK\n"
              "$ exit\n");
}

TEST(command_cat)
{
    int fd;

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "cat\n");
        input(fd, "cat hexdump.in\n");
        input(fd, "cat foobar\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(
        output,
        "cat\n"
        "Usage: cat <file>\n"
        "ERROR(-22: Invalid argument)\n"
        "$ cat hexdump.in\n"
        "0123456789012345678901234567890123456789"
        "OK\n"
        "$ cat foobar\n"
        "ERROR(-2: No such file or directory)\n"
        "$ exit\n");
}

TEST(command_rm)
{
    int fd;

    fd = init_and_start();

    remove_mock_once("foobar", 0);
    remove_mock_once("foobar", -1);
    remove_mock_set_errno(ENOENT);
    remove_mock_once("utils", -1);
    remove_mock_set_errno(ENOTEMPTY);

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "rm\n");
        input(fd, "rm foobar\n");
        input(fd, "rm foobar\n");
        input(fd, "rm utils\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(
        output,
        "rm\n"
        "Usage: rm <file or directory> [<file or directory> ...]\n"
        "ERROR(-22: Invalid argument)\n"
        "$ rm foobar\n"
        "OK\n"
        "$ rm foobar\n"
        "ERROR(-2: No such file or directory)\n"
        "$ rm utils\n"
        "ERROR(-39: Directory not empty)\n"
        "$ exit\n");
}

TEST(command_hexdump)
{
    int fd;

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "hexdump\n");
        input(fd, "hexdump hexdump.in\n");
        input(fd, "hexdump 0 hexdump.in\n");
        input(fd, "hexdump 1 hexdump.in\n");
        input(fd, "hexdump 0 1 hexdump.in\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(
        output,
        "hexdump\n"
        "Usage: hexdump [[<offset>] <size>] <file>\n"
        "ERROR(-22: Invalid argument)\n"
        "$ hexdump hexdump.in\n"
        "00000000: 30 31 32 33 34 35 36 37 38 39 30 31 32 33 34 35 '0123456789012345'\n"
        "00000010: 36 37 38 39 30 31 32 33 34 35 36 37 38 39 30 31 '6789012345678901'\n"
        "00000020: 32 33 34 35 36 37 38 39                         '23456789'\n"
        "OK\n"
        "$ hexdump 0 hexdump.in\n"
        "OK\n"
        "$ hexdump 1 hexdump.in\n"
        "00000000: 30                                              '0'\n"
        "OK\n"
        "$ hexdump 0 1 hexdump.in\n"
        "00000000: 30                                              '0'\n"
        "OK\n"
        "$ exit\n");
}

TEST(command_editing)
{
    int fd;

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        /* 1. Ctrl-A + Ctrl-D + Enter. */
        input(fd, "12");
        input(fd, "\x01\x04\n");

        /* 2. Left arrow + Ctrl-T + Enter. */
        input(fd, "12");
        input(fd, ESC"[D\x14\n");

        /* 3. Left arrow + Space + Enter. */
        input(fd, "12");
        input(fd, ESC"[D \n");

        /* 4. Left arrow + Right arrow+ Enter. */
        input(fd, "12");
        input(fd, ESC"[D"ESC"[C""\n");

        /* 5. Left arrow + Ctrl-K + Enter. */
        input(fd, "12");
        input(fd, ESC"[D\x0b\n");

        /* 6. Ctrl-A + Ctrl-E + backspace + Enter. */
        input(fd, "12");
        input(fd, "\x01\x05\x08\n");

        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(
        output,
        /* 1. Ctrl-A + Ctrl-D + Enter. */
        "12"ESC"[2D"ESC"[K2"ESC"[1D\n"
        "2: command not found\n"
        "ERROR(-22: Invalid argument)\n"

        /* 2. Left arrow + Ctrl-T + Enter. */
        "$ 12"ESC"[1D"ESC"[1D"ESC"[K21\n"
        "21: command not found\n"
        "ERROR(-22: Invalid argument)\n"

        /* 3. Left arrow + Space + Enter. */
        "$ 12"ESC"[1D"ESC"[1D"ESC"[K1 2"ESC"[1D\n"
        "1: command not found\n"
        "ERROR(-22: Invalid argument)\n"

        /* 4. Left arrow + Right arrow + Enter. */
        "$ 12"ESC"[1D"ESC"[1C\n"
        "12: command not found\n"
        "ERROR(-22: Invalid argument)\n"

        /* 5. Left arrow + Ctrl-K + Enter. */
        "$ 12"ESC"[1D "BACKSPACE" "BACKSPACE"\n"
        "1: command not found\n"
        "ERROR(-22: Invalid argument)\n"

        /* 6. Ctrl-A + Ctrl-E + backspace + Enter. */
        "$ 12"ESC"[2D"ESC"[2C"BACKSPACE" "BACKSPACE"\n"
        "1: command not found\n"
        "ERROR(-22: Invalid argument)\n"

        "$ exit\n");
}

static int command_quotes(int argc, const char *argv[], FILE *fout_p)
{
    (void)fout_p;

    ASSERT_EQ(argc, 3);
    ASSERT_EQ(strcmp(argv[0], "quotes"), 0);
    ASSERT_EQ(strcmp(argv[1], "ba\" \\r"), 0);
    ASSERT_EQ(strcmp(argv[2], ""), 0);

    return (0);
}

TEST(quotes)
{
    int fd;

    ml_shell_init();
    ml_shell_register_command("quotes", ".", command_quotes);

    CAPTURE_OUTPUT(output, errput) {
        fd = stdin_pipe();
        ml_shell_start();
        input(fd, "quotes \"ba\\\" \\r\" \"\"\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(
        output,
        "quotes \"ba\\\" \\r\" \"\"\n"
        "OK\n"
        "$ exit\n");
}

TEST(history)
{
    int fd;

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "foo\n");
        input(fd, "bar\n");
        input(fd, "fie\n");
        input(fd, "history\n");
        /* Ctrl R. */
        input(fd, "\x12""fo\n");
        /* Down arrow - nothing should happen. */
        input(fd, ESC"[B");
        /* Up arrow one beyond top. */
        input(fd, ESC"[A"ESC"[A"ESC"[A"ESC"[A"ESC"[A"ESC"[A"ESC"[A");
        /* Down arrow once and press enter. */
        input(fd, ESC"[B\n");
        /* Up once and then down. */
        input(fd, ESC"[A"ESC"[B");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(
        output,
        "foo\n"
        "foo: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ bar\n"
        "bar: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ fie\n"
        "fie: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ history\n"
        "1: foo\n"
        "2: bar\n"
        "3: fie\n"
        "4: history\n"
        "OK\n"
        "$ "ESC"[K(history-search)`': "ESC"[3D"ESC"[Kf': fie"ESC"[6D"
        ESC"[Ko': foo"ESC"[6D"ESC"[19D"ESC"[Kfoo\n"
        "foo: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ foo"
        ESC"[3D"ESC"[K"
        "history"
        ESC"[7D"ESC"[K"
        "fie"
        ESC"[3D"ESC"[K"
        "bar"
        ESC"[3D"ESC"[K"
        "foo"
        ESC"[3D"ESC"[K"
        "bar\n"
        "bar: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ bar"
        BACKSPACE" "BACKSPACE
        BACKSPACE" "BACKSPACE
        BACKSPACE" "BACKSPACE
        "exit\n");
}

TEST(history_full)
{
    int fd;
    int i;
    char buf[64];

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        for (i = 0; i < 64; i++) {
            sprintf(&buf[0], "command-%d\n", i);
            input(fd, &buf[0]);
        }

        input(fd, "history\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(
        output,
        "command-0\n"
        "command-0: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-1\n"
        "command-1: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-2\n"
        "command-2: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-3\n"
        "command-3: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-4\n"
        "command-4: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-5\n"
        "command-5: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-6\n"
        "command-6: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-7\n"
        "command-7: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-8\n"
        "command-8: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-9\n"
        "command-9: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-10\n"
        "command-10: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-11\n"
        "command-11: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-12\n"
        "command-12: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-13\n"
        "command-13: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-14\n"
        "command-14: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-15\n"
        "command-15: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-16\n"
        "command-16: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-17\n"
        "command-17: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-18\n"
        "command-18: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-19\n"
        "command-19: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-20\n"
        "command-20: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-21\n"
        "command-21: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-22\n"
        "command-22: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-23\n"
        "command-23: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-24\n"
        "command-24: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-25\n"
        "command-25: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-26\n"
        "command-26: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-27\n"
        "command-27: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-28\n"
        "command-28: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-29\n"
        "command-29: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-30\n"
        "command-30: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-31\n"
        "command-31: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-32\n"
        "command-32: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-33\n"
        "command-33: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-34\n"
        "command-34: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-35\n"
        "command-35: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-36\n"
        "command-36: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-37\n"
        "command-37: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-38\n"
        "command-38: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-39\n"
        "command-39: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-40\n"
        "command-40: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-41\n"
        "command-41: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-42\n"
        "command-42: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-43\n"
        "command-43: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-44\n"
        "command-44: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-45\n"
        "command-45: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-46\n"
        "command-46: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-47\n"
        "command-47: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-48\n"
        "command-48: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-49\n"
        "command-49: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-50\n"
        "command-50: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-51\n"
        "command-51: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-52\n"
        "command-52: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-53\n"
        "command-53: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-54\n"
        "command-54: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-55\n"
        "command-55: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-56\n"
        "command-56: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-57\n"
        "command-57: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-58\n"
        "command-58: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-59\n"
        "command-59: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-60\n"
        "command-60: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-61\n"
        "command-61: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-62\n"
        "command-62: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ command-63\n"
        "command-63: command not found\n"
        "ERROR(-22: Invalid argument)\n"
        "$ history\n"
        "1: command-1\n"
        "2: command-2\n"
        "3: command-3\n"
        "4: command-4\n"
        "5: command-5\n"
        "6: command-6\n"
        "7: command-7\n"
        "8: command-8\n"
        "9: command-9\n"
        "10: command-10\n"
        "11: command-11\n"
        "12: command-12\n"
        "13: command-13\n"
        "14: command-14\n"
        "15: command-15\n"
        "16: command-16\n"
        "17: command-17\n"
        "18: command-18\n"
        "19: command-19\n"
        "20: command-20\n"
        "21: command-21\n"
        "22: command-22\n"
        "23: command-23\n"
        "24: command-24\n"
        "25: command-25\n"
        "26: command-26\n"
        "27: command-27\n"
        "28: command-28\n"
        "29: command-29\n"
        "30: command-30\n"
        "31: command-31\n"
        "32: command-32\n"
        "33: command-33\n"
        "34: command-34\n"
        "35: command-35\n"
        "36: command-36\n"
        "37: command-37\n"
        "38: command-38\n"
        "39: command-39\n"
        "40: command-40\n"
        "41: command-41\n"
        "42: command-42\n"
        "43: command-43\n"
        "44: command-44\n"
        "45: command-45\n"
        "46: command-46\n"
        "47: command-47\n"
        "48: command-48\n"
        "49: command-49\n"
        "50: command-50\n"
        "51: command-51\n"
        "52: command-52\n"
        "53: command-53\n"
        "54: command-54\n"
        "55: command-55\n"
        "56: command-56\n"
        "57: command-57\n"
        "58: command-58\n"
        "59: command-59\n"
        "60: command-60\n"
        "61: command-61\n"
        "62: command-62\n"
        "63: command-63\n"
        "64: history\n"
        "OK\n"
        "$ exit\n");
}

TEST(command_insmod)
{
    int fd;

    fd = 99;
    open_mock_once("foo.ko", O_RDONLY | O_CLOEXEC, fd, "");
    ml_finit_module_mock_once(fd, "", 0, 0);
    close_mock_once(fd, 0);

    fd = 98;
    open_mock_once("bar.ko", O_RDONLY | O_CLOEXEC, fd, "");
    ml_finit_module_mock_once(fd, "fie=fum", 0, 0);
    close_mock_once(fd, 0);

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "insmod\n");
        input(fd, "insmod foo.ko\n");
        input(fd, "insmod bar.ko fie=fum\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "insmod\n"
              "Usage: insmod <file> [<params>]\n"
              "ERROR(-22: Invalid argument)\n"
              "$ insmod foo.ko\n"
              "OK\n"
              "$ insmod bar.ko fie=fum\n"
              "OK\n"
              "$ exit\n");
}

TEST(command_rmmod)
{
    int fd;

    ml_remove_module_mock_once("foo", 0, 0);
    ml_remove_module_mock_once("bar", 1, 0);
    ml_remove_module_mock_once("fie", 0, -ENOENT);

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "rmmod\n");
        input(fd, "rmmod foo\n");
        input(fd, "rmmod bar 1\n");
        input(fd, "rmmod fie\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "rmmod\n"
              "Usage: rmmod <module> [<flags>]\n"
              "ERROR(-22: Invalid argument)\n"
              "$ rmmod foo\n"
              "OK\n"
              "$ rmmod bar 1\n"
              "OK\n"
              "$ rmmod fie\n"
              "Usage: rmmod <module> [<flags>]\n"
              "ERROR(-2: No such file or directory)\n"
              "$ exit\n");
}

TEST(command_mkdir)
{
    int fd;

    mkdir_mock_once("foo", 0777, 0);
    mkdir_mock_once("foo", 0777, -1);
    mkdir_mock_set_errno(EEXIST);

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "mkdir\n");
        input(fd, "mkdir foo\n");
        input(fd, "mkdir foo\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "mkdir\n"
              "Usage: mkdir <dir>\n"
              "ERROR(-22: Invalid argument)\n"
              "$ mkdir foo\n"
              "OK\n"
              "$ mkdir foo\n"
              "Usage: mkdir <dir>\n"
              "ERROR(-17: File exists)\n"
              "$ exit\n");
}

TEST(command_umount)
{
    int fd;

    umount_mock_once("foo", 0);
    umount_mock_once("bar", -1);
    umount_mock_set_errno(EEXIST);

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "umount\n");
        input(fd, "umount foo\n");
        input(fd, "umount bar\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "umount\n"
              "Usage: umount <dir>\n"
              "ERROR(-22: Invalid argument)\n"
              "$ umount foo\n"
              "OK\n"
              "$ umount bar\n"
              "Usage: umount <dir>\n"
              "ERROR(-17: File exists)\n"
              "$ exit\n");
}

TEST(command_df_setmntent_failure)
{
    int fd;

    setmntent_mock_once("/proc/mounts", "r", NULL);

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "df\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "df\n"
              "ERROR(-1000: General)\n"
              "$ exit\n");
}

TEST(command_df)
{
    int fd;
    FILE *f_p;
    struct mntent *mntent_root_p;
    struct mntent *mntent_proc_p;
    struct statvfs stat;

    f_p = (FILE *)1;
    setmntent_mock_once("/proc/mounts", "r", f_p);

    /* /. */
    mntent_root_p = xmalloc(sizeof(*mntent_root_p));
    mntent_root_p->mnt_dir = "/";
    getmntent_mock_once(mntent_root_p);
    getmntent_mock_set_stream_in_pointer(f_p);
    stat.f_bsize = 512;
    stat.f_blocks = 20000;
    stat.f_bfree = 15000;
    statvfs_mock_once("/", 0);
    statvfs_mock_set_buf_out(&stat, sizeof(stat));

    /* /proc. */
    mntent_proc_p = xmalloc(sizeof(*mntent_proc_p));
    mntent_proc_p->mnt_dir = "/proc";
    getmntent_mock_once(mntent_proc_p);
    getmntent_mock_set_stream_in_pointer(f_p);
    stat.f_bsize = 512;
    stat.f_blocks = 40000;
    stat.f_bfree = 10000;
    statvfs_mock_once("/proc", 0);
    statvfs_mock_set_buf_out(&stat, sizeof(stat));

    /* No more mounted file systems. */
    getmntent_mock_once(NULL);
    getmntent_mock_set_stream_in_pointer(f_p);
    endmntent_mock_once(0);
    endmntent_mock_set_streamp_in_pointer(f_p);

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "df\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "df\n"
              "MOUNTED ON               TOTAL      USED      FREE\n"
              "/                         9 MB      2 MB      7 MB\n"
              "/proc                    19 MB     14 MB      5 MB\n"
              "OK\n"
              "$ exit\n");

    free(mntent_root_p);
    free(mntent_proc_p);
}

TEST(command_suicide_no_args)
{
    int fd;

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "suicide\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "suicide\n"
              "Usage: suicide {abort,segfault}\n"
              "ERROR(-22: Invalid argument)\n"
              "$ exit\n");
}

TEST(command_find_too_many_args)
{
    int fd;

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "find a b\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "find a b\n"
              "Usage: find [<path>]\n"
              "ERROR(-22: Invalid argument)\n"
              "$ exit\n");
}

static void find_callback(const char *__dir,
                          __nftw_func_t __func,
                          int __descriptors,
                          int __flag)
{
    (void)__dir;
    (void)__descriptors;
    (void)__flag;

    struct stat stat;

    stat.st_mode = S_IFDIR;
    ASSERT_EQ(__func("./foo", &stat, 0, NULL), 0);
    stat.st_mode = 0;
    ASSERT_EQ(__func("./foo/bar", &stat, 0, NULL), 0);
    stat.st_mode = 0;
    ASSERT_EQ(__func("./fie", &stat, 0, NULL), 0);
}

TEST(command_find_no_args)
{
    int fd;

    nftw_mock_once(".", 20, FTW_PHYS, 0);
    nftw_mock_set_callback(find_callback);

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "find\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "find\n"
              "./foo/\n"
              "./foo/bar\n"
              "./fie\n"
              "OK\n"
              "$ exit\n");
}

static void tmp_callback(const char *__dir,
                         __nftw_func_t __func,
                         int __descriptors,
                         int __flag)
{
    (void)__dir;
    (void)__descriptors;
    (void)__flag;

    struct stat stat;

    stat.st_mode = S_IFDIR;
    ASSERT_EQ(__func("tmp/foo", &stat, 0, NULL), 0);
    stat.st_mode = 0;
    ASSERT_EQ(__func("tmp/foo/bar", &stat, 0, NULL), 0);
    stat.st_mode = 0;
    ASSERT_EQ(__func("tmp/fie", &stat, 0, NULL), 0);
}

TEST(command_find_in_dir)
{
    int fd;

    nftw_mock_once("tmp", 20, FTW_PHYS, 0);
    nftw_mock_set_callback(tmp_callback);

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "find tmp\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "find tmp\n"
              "tmp/foo/\n"
              "tmp/foo/bar\n"
              "tmp/fie\n"
              "OK\n"
              "$ exit\n");
}

TEST(command_mknod_no_args)
{
    int fd;

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "mknod\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "mknod\n"
              "Usage: mknod <path> <type> [<major>] [<minor>]\n"
              "ERROR(-22: Invalid argument)\n"
              "$ exit\n");
}

TEST(command_mknod_bad_type)
{
    int fd;

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "mknod /dev/foo g\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "mknod /dev/foo g\n"
              "Usage: mknod <path> <type> [<major>] [<minor>]\n"
              "ERROR(-22: Invalid argument)\n"
              "$ exit\n");
}

TEST(command_mknod_fifo)
{
    int fd;

    mknod_mock_once("/dev/foo", S_IFIFO | 0666, 0, 0);

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "mknod /dev/foo p\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "mknod /dev/foo p\n"
              "OK\n"
              "$ exit\n");
}

TEST(command_mknod_char)
{
    int fd;

    mknod_mock_once("/dev/bar", S_IFCHR | 0666, makedev(5, 6), 0);

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "mknod /dev/bar c 5 6\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "mknod /dev/bar c 5 6\n"
              "OK\n"
              "$ exit\n");
}

TEST(command_mknod_char_no_minor)
{
    int fd;

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "mknod /dev/bar c 5\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "mknod /dev/bar c 5\n"
              "Usage: mknod <path> <type> [<major>] [<minor>]\n"
              "ERROR(-22: Invalid argument)\n"
              "$ exit\n");
}

TEST(command_mknod_block)
{
    int fd;

    mknod_mock_once("/dev/sda1", S_IFBLK | 0666, makedev(8, 1), 0);

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "mknod /dev/sda1 b 8 1\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "mknod /dev/sda1 b 8 1\n"
              "OK\n"
              "$ exit\n");
}

TEST(command_mount_usage)
{
    int fd;

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "mount -h\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "mount -h\n"
              "Usage: mount [<device> <dir> <type> [<options>]]\n"
              "ERROR(-22: Invalid argument)\n"
              "$ exit\n");
}

TEST(command_mount)
{
    int fd;

    mount_mock_once("/dev/sda1", "/mnt/disk", "ext4", 0, 0);
    mount_mock_set_data_in_pointer(NULL);

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "mount /dev/sda1 /mnt/disk ext4\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "mount /dev/sda1 /mnt/disk ext4\n"
              "OK\n"
              "$ exit\n");
}

TEST(command_mount_with_options)
{
    int fd;

    mount_mock_once("/dev/sda1", "/mnt/disk", "ext4", 0, 0);
    mount_mock_set_data_in("size=1024k", 11);

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "mount /dev/sda1 /mnt/disk ext4 size=1024k\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "mount /dev/sda1 /mnt/disk ext4 size=1024k\n"
              "OK\n"
              "$ exit\n");
}

TEST(command_date_get)
{
    int fd;

    time_mock_once(1574845540);

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "date\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "date\n"
              "Wed Nov 27 09:05:40 2019\n"
              "OK\n"
              "$ exit\n");
}

TEST(command_date_set)
{
    int fd;
    struct timespec ts;

    ts.tv_sec = 3600;
    ts.tv_nsec = 0;
    clock_settime_mock_once(CLOCK_REALTIME, 0);
    clock_settime_mock_set_tp_in(&ts, sizeof(ts));

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "date 3600\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "date 3600\n"
              "OK\n"
              "$ exit\n");
}

TEST(command_date_too_many_args)
{
    int fd;

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "date 1 2\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "date 1 2\n"
              "Usage: date [<unix-time>]\n"
              "ERROR(-22: Invalid argument)\n"
              "$ exit\n");
}

TEST(command_print)
{
    int fd;
    FILE file;

    fd = init_and_start();

    fopen_mock_once("my-file", "w", &file);
    fwrite_mock_once(1, 5, 5);
    fwrite_mock_set_ptr_in("hello", 5);
    fwrite_mock_once(1, 1, 1);
    fwrite_mock_set_ptr_in("\n", 1);
    fclose_mock_once(0);

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "print hello my-file\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "print hello my-file\n"
              "OK\n"
              "$ exit\n");
}

TEST(command_dmesg)
{
    int fd;

    fd = init_and_start();

    open_mock_once("/dev/kmsg", O_RDONLY | O_NONBLOCK, 5, "");
    read_mock_once(5, 1023, 54);
    read_mock_set_buf_out(
        "6,838,4248863,-;intel_rapl: Found RAPL domain package\n",
        54);
    read_mock_once(5, 1023, 50);
    read_mock_set_buf_out(
        "6,839,4248865,-;intel_rapl: Found RAPL domain core",
        50);
    read_mock_once(5, 1023, -1);
    close_mock_once(5, 0);

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "dmesg\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "dmesg\n"
              "[    4.248863] intel_rapl: Found RAPL domain package\n"
              "[    4.248865] intel_rapl: Found RAPL domain core\n"
              "OK\n"
              "$ exit\n");
}

TEST(command_dmesg_open_error)
{
    int fd;

    fd = init_and_start();

    open_mock_once("/dev/kmsg", O_RDONLY | O_NONBLOCK, -1, "");
    open_mock_set_errno(55);

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "dmesg\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "dmesg\n"
              "ERROR(-55: No anode)\n"
              "$ exit\n");
}

TEST(command_sync)
{
    int fd;

    fd = init_and_start();

    sync_mock_once();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "sync\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "sync\n"
              "OK\n"
              "$ exit\n");
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

TEST(command_top)
{
    int fd;
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

    fd = init_and_start();

    mock_prepare_read_cpus_stats(&lines[0][0], 6);
    usleep_mock_once(500000, 0);
    mock_prepare_read_cpus_stats(&lines[1][0], 5);

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "top\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "top\n"
              "CPU  USER  SYSTEM  IDLE\n"
              "all   43%      2%   53%\n"
              "1     36%      9%   54%\n"
              "2     66%      0%   33%\n"
              "3     25%      0%   75%\n"
              "4     44%      0%   55%\n"
              "OK\n"
              "$ exit\n");
}

TEST(command_ntp_date_default_server)
{
    int fd;

    fd = init_and_start();

    ml_ntp_client_sync_mock_once("0.se.pool.ntp.org", 0);

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "ntp_sync\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "ntp_sync\n"
              "OK\n"
              "$ exit\n");
}

TEST(command_ntp_date_non_default_server)
{
    int fd;

    fd = init_and_start();

    ml_ntp_client_sync_mock_once("foo.bar", 0);

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "ntp_sync foo.bar\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "ntp_sync foo.bar\n"
              "OK\n"
              "$ exit\n");
}

TEST(command_ntp_date_sync_failure)
{
    int fd;

    fd = init_and_start();

    ml_ntp_client_sync_mock_once("0.se.pool.ntp.org", -EACCES);

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "ntp_sync\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "ntp_sync\n"
              "ERROR(-13: Permission denied)\n"
              "$ exit\n");
}

TEST(command_ntp_date_too_many_arguments)
{
    int fd;

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "ntp_sync 1 2\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "ntp_sync 1 2\n"
              "Usage: ntp_sync [<server>]\n"
              "ERROR(-22: Invalid argument)\n"
              "$ exit\n");
}

TEST(command_dd)
{
    int fd;
    struct timeval start_time;
    struct timeval end_time;

    start_time.tv_sec = 1;
    start_time.tv_usec = 0;
    end_time.tv_sec = 1;
    end_time.tv_usec = 500;

    gettimeofday_mock_once(0);
    gettimeofday_mock_set_tv_out(&start_time, sizeof(start_time));
    ml_dd_mock_once("a", "b", 1000, 1000, 0);
    gettimeofday_mock_once(0);
    gettimeofday_mock_set_tv_out(&end_time, sizeof(end_time));

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "dd a b 1000 1000\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "dd a b 1000 1000\n"
              "1000 bytes copied in 0.500 ms (2.000 MB/s).\n"
              "OK\n"
              "$ exit\n");
}

TEST(command_dd_no_args)
{
    int fd;

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "dd\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "dd\n"
              "Usage: dd <infile> <outfile> <total-size> <chunk-size>\n"
              "ERROR(-22: Invalid argument)\n"
              "$ exit\n");
}

TEST(command_dd_error)
{
    int fd;

    ml_dd_mock_once("a", "b", 1000, 1000, -ENOENT);

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "dd a b 1000 1000\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "dd a b 1000 1000\n"
              "ERROR(-2: No such file or directory)\n"
              "$ exit\n");
}

TEST(i2c_no_args)
{
    int fd;

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "i2c\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "i2c\n"
              "Usage: i2c scan <device>\n"
              "ERROR(-22: Invalid argument)\n"
              "$ exit\n");
}

static void mock_prepare_devices_not_present(int fd,
                                             long first_address,
                                             long last_address)
{
    long address;

    for (address = first_address; address <= last_address; address++)  {
        ioctl_mock_once(fd, I2C_SLAVE, 0, "%ld", address);
        read_mock_once(fd, 1, -1);
        read_mock_set_errno(EACCES);
    }
}

static void mock_prepare_devices_present(int fd,
                                         long first_address,
                                         long last_address)
{
    long address;
    uint8_t value;

    value = 0;

    for (address = first_address; address <= last_address; address++)  {
        ioctl_mock_once(fd, I2C_SLAVE, 0, "%ld", address);
        read_mock_once(fd, 1, 1);
        read_mock_set_buf_out(&value, sizeof(value));
    }
}

static void mock_prepare_set_slave_address_error(int fd, long address)
{
    ioctl_mock_once(fd, I2C_SLAVE, -1, "%ld", address);
}

TEST(i2c_scan)
{
    int fd;
    int i2cfd;

    fd = init_and_start();

    i2cfd = 6;
    open_mock_once("/dev/i2c1", O_RDWR, i2cfd, "");

    mock_prepare_devices_not_present(i2cfd, 0x00, 0x0a);
    mock_prepare_devices_present(i2cfd, 0x0b, 0x0b);
    mock_prepare_devices_not_present(i2cfd, 0x0c, 0x59);
    mock_prepare_set_slave_address_error(i2cfd, 0x5a);
    mock_prepare_devices_present(i2cfd, 0x5b, 0x5d);
    mock_prepare_devices_not_present(i2cfd, 0x5e, 0x7f);
    close_mock_once(i2cfd, 0);

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "i2c scan /dev/i2c1\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "i2c scan /dev/i2c1\n"
              "Found device with address 0x0b.\n"
              "Failed to set address 0x5a.\n"
              "Found device with address 0x5b.\n"
              "Found device with address 0x5c.\n"
              "Found device with address 0x5d.\n"
              "OK\n"
              "$ exit\n");
}

TEST(i2c_scan_no_devices_found)
{
    int fd;
    int i2cfd;

    fd = init_and_start();

    i2cfd = 6;
    open_mock_once("/dev/i2c2", O_RDWR, i2cfd, "");

    mock_prepare_devices_not_present(i2cfd, 0x00, 0x7f);
    close_mock_once(i2cfd, 0);

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "i2c scan /dev/i2c2\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "i2c scan /dev/i2c2\n"
              "No devices found.\n"
              "OK\n"
              "$ exit\n");
}

TEST(i2c_scan_open_error)
{
    int fd;

    fd = init_and_start();

    open_mock_once("/dev/i2c1", O_RDWR, -1, "");
    open_mock_set_errno(ENOENT);
    close_mock_none();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "i2c scan /dev/i2c1\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "i2c scan /dev/i2c1\n"
              "Usage: i2c scan <device>\n"
              "ERROR(-2: No such file or directory)\n"
              "$ exit\n");
}

TEST(i2c_scan_no_device_given)
{
    int fd;

    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "i2c scan\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "i2c scan\n"
              "Usage: i2c scan <device>\n"
              "ERROR(-22: Invalid argument)\n"
              "$ exit\n");
}

TEST(log_list_and_set_mask)
{
    int fd;
    struct ml_log_object_t log_object;

    ml_log_object_module_init(NULL);
    ml_log_object_init(&log_object, "test-object", ML_LOG_INFO);
    ml_log_object_register(&log_object);
    fd = init_and_start();

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "log list\n");
        input(fd, "log set_level test-object warning\n");
        input(fd, "log list\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "log list\n"
              "OBJECT-NAME       LEVEL\n"
              "test-object       info\n"
              "log-object        info\n"
              "OK\n"
              "$ log set_level test-object warning\n"
              "OK\n"
              "$ log list\n"
              "OBJECT-NAME       LEVEL\n"
              "test-object       warning\n"
              "log-object        info\n"
              "OK\n"
              "$ exit\n");
}

TEST(log_store)
{
    int fd;

    fd = init_and_start();

    ml_log_object_store_mock_once(0);
    ml_log_object_store_mock_once(-ENOENT);

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "log store\n");
        input(fd, "log store\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "log store\n"
              "OK\n"
              "$ log store\n"
              "Usage: log show\n"
              "       log list\n"
              "       log set_level <log-object> <mask>\n"
              "       log store\n"
              "       log print <message>\n"
              "       log print <level> <message>\n"
              "ERROR(-2: No such file or directory)\n"
              "$ exit\n");
}

TEST(log_print)
{
    int fd;

    fd = init_and_start();

    ml_log_object_vprint_mock_once(ML_LOG_INFO, "hello");
    ml_log_object_vprint_mock_once(ML_LOG_ERROR, "hi");

    CAPTURE_OUTPUT(output, errput) {
        input(fd, "log print hello\n");
        input(fd, "log print error hi\n");
        input(fd, "exit\n");
        ml_shell_join();
    }

    ASSERT_EQ(output,
              "log print hello\n"
              "OK\n"
              "$ log print error hi\n"
              "OK\n"
              "$ exit\n");
}
