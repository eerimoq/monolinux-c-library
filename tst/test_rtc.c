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

#include <time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/rtc.h>
#include "nala.h"
#include "nala_mocks.h"
#include "ml/ml.h"

TEST(get_time)
{
    struct rtc_time rtm;
    struct tm tm;

    rtm.tm_sec = 1;
    rtm.tm_min = 2;
    rtm.tm_hour = 3;
    rtm.tm_mday = 4;
    rtm.tm_wday = 5;
    rtm.tm_mon = 6;
    rtm.tm_year = 7;
    rtm.tm_yday = 8;
    rtm.tm_isdst = 9;

    ml_open_mock_once("/dev/rtc99", O_RDONLY, 5);
    ioctl_mock_once(5, RTC_RD_TIME, 0, "%p", &rtm);
    ioctl_mock_set_va_arg_out_at(0, &rtm, sizeof(rtm));
    ml_close_mock_once(5, 0);

    ASSERT_EQ(ml_rtc_get_time("/dev/rtc99", &tm), 0);
    ASSERT_EQ(tm.tm_sec, 1);
    ASSERT_EQ(tm.tm_min, 2);
    ASSERT_EQ(tm.tm_hour, 3);
    ASSERT_EQ(tm.tm_mday, 4);
    ASSERT_EQ(tm.tm_wday, 5);
    ASSERT_EQ(tm.tm_mon, 6);
    ASSERT_EQ(tm.tm_year, 7);
    ASSERT_EQ(tm.tm_yday, 8);
    ASSERT_EQ(tm.tm_isdst, 9);
}

TEST(get_time_open_error)
{
    struct tm tm;

    ml_open_mock_once("/dev/rtc99", O_RDONLY, -1);
    ioctl_mock_none();
    ml_close_mock_none();

    ASSERT_EQ(ml_rtc_get_time("/dev/rtc99", &tm), -1);
}

TEST(get_time_ioctl_error)
{
    struct tm tm;

    ml_open_mock_once("/dev/rtc99", O_RDONLY, 1);
    ioctl_mock_once(1, RTC_RD_TIME, -1, "");
    ml_close_mock_once(1, 0);

    ASSERT_EQ(ml_rtc_get_time("/dev/rtc99", &tm), -1);
}

TEST(set_time)
{
    struct rtc_time rtm;
    struct tm tm;

    rtm.tm_sec = 1;
    rtm.tm_min = 2;
    rtm.tm_hour = 3;
    rtm.tm_mday = 4;
    rtm.tm_wday = 5;
    rtm.tm_mon = 6;
    rtm.tm_year = 7;
    rtm.tm_yday = 8;
    rtm.tm_isdst = 9;

    ml_open_mock_once("/dev/rtc99", O_WRONLY, 6);
    ioctl_mock_once(6, RTC_SET_TIME, 0, "%p", &rtm);
    ioctl_mock_set_va_arg_in_at(0, &rtm, sizeof(rtm));
    ml_close_mock_once(6, 0);

    tm.tm_sec = 1;
    tm.tm_min = 2;
    tm.tm_hour = 3;
    tm.tm_mday = 4;
    tm.tm_wday = 5;
    tm.tm_mon = 6;
    tm.tm_year = 7;
    tm.tm_yday = 8;
    tm.tm_isdst = 9;

    ASSERT_EQ(ml_rtc_set_time("/dev/rtc99", &tm), 0);
}

TEST(set_time_open_error)
{
    struct tm tm;

    ml_open_mock_once("/dev/rtc99", O_WRONLY, -1);
    ioctl_mock_none();
    ml_close_mock_none();

    tm.tm_sec = 1;
    tm.tm_min = 2;
    tm.tm_hour = 3;
    tm.tm_mday = 4;
    tm.tm_wday = 5;
    tm.tm_mon = 6;
    tm.tm_year = 7;
    tm.tm_yday = 8;
    tm.tm_isdst = 9;

    ASSERT_EQ(ml_rtc_set_time("/dev/rtc99", &tm), -1);
}

TEST(set_time_ioctl_error)
{
    struct tm tm;

    ml_open_mock_once("/dev/rtc99", O_WRONLY, 1);
    ioctl_mock_once(1, RTC_SET_TIME, -1, "");
    ml_close_mock_once(1, 0);

    tm.tm_sec = 1;
    tm.tm_min = 2;
    tm.tm_hour = 3;
    tm.tm_mday = 4;
    tm.tm_wday = 5;
    tm.tm_mon = 6;
    tm.tm_year = 7;
    tm.tm_yday = 8;
    tm.tm_isdst = 9;

    ASSERT_EQ(ml_rtc_set_time("/dev/rtc99", &tm), -1);
}
