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
#include <time.h>
#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "ml/ml.h"

int ml_rtc_get_time(const char *device_p, struct tm *tm_p)
{
    int res;
    int fd;
    struct rtc_time rtm;

    res = -1;
    fd = ml_open(device_p, O_RDONLY);

    if (fd != -1) {
        res = ioctl(fd, RTC_RD_TIME, &rtm);

        if (res == 0) {
            tm_p->tm_sec = rtm.tm_sec;
            tm_p->tm_min = rtm.tm_min;
            tm_p->tm_hour = rtm.tm_hour;
            tm_p->tm_mday = rtm.tm_mday;
            tm_p->tm_wday = rtm.tm_wday;
            tm_p->tm_mon = rtm.tm_mon;
            tm_p->tm_year = rtm.tm_year;
            tm_p->tm_yday = rtm.tm_yday;
            tm_p->tm_isdst = rtm.tm_isdst;
        }

        close(fd);
    }

    return (res);
}

int ml_rtc_set_time(const char *device_p, struct tm *tm_p)
{
    int res;
    int fd;
    struct rtc_time rtm;

    res = -1;
    fd = ml_open(device_p, O_WRONLY);

    if (fd != -1) {
        rtm.tm_sec = tm_p->tm_sec;
        rtm.tm_min = tm_p->tm_min;
        rtm.tm_hour = tm_p->tm_hour;
        rtm.tm_mday = tm_p->tm_mday;
        rtm.tm_wday = tm_p->tm_wday;
        rtm.tm_mon = tm_p->tm_mon;
        rtm.tm_year = tm_p->tm_year;
        rtm.tm_yday = tm_p->tm_yday;
        rtm.tm_isdst = tm_p->tm_isdst;

        res = ioctl(fd, RTC_SET_TIME, &rtm);

        close(fd);
    }

    return (res);
}
