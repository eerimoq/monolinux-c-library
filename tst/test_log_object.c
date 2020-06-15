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

#include <fcntl.h>
#include <unistd.h>
#include "nala.h"
#include "ml/ml.h"

TEST(format)
{
    struct ml_log_object_t log_object;

    ml_log_object_module_init();

    ml_log_object_init(&log_object, "foo", ML_LOG_DEBUG);

    /* Emergency. */
    CAPTURE_OUTPUT(output1, errput1) {
        ml_log_object_print(&log_object, ML_LOG_EMERGENCY, "bar %d", 1);
    }

    ASSERT_SUBSTRING(output1, " EMERGENCY foo bar 1\n");

    /* Alert. */
    CAPTURE_OUTPUT(output2, errput2) {
        ml_log_object_print(&log_object, ML_LOG_ALERT, "bar %d", 2);
    }

    ASSERT_SUBSTRING(output2, " ALERT foo bar 2\n");

    /* Critical. */
    CAPTURE_OUTPUT(output3, errput3) {
        ml_log_object_print(&log_object, ML_LOG_CRITICAL, "bar %d", 3);
    }

    ASSERT_SUBSTRING(output3, " CRITICAL foo bar 3\n");

    /* Error. */
    CAPTURE_OUTPUT(output4, errput4) {
        ml_log_object_print(&log_object, ML_LOG_ERROR, "bar %d", 4);
    }

    ASSERT_SUBSTRING(output4, " ERROR foo bar 4\n");

    /* Warning. */
    CAPTURE_OUTPUT(output5, errput5) {
        ml_log_object_print(&log_object, ML_LOG_WARNING, "bar %d", 5);
    }

    ASSERT_SUBSTRING(output5, " WARNING foo bar 5\n");

    /* Notice. */
    CAPTURE_OUTPUT(output6, errput6) {
        ml_log_object_print(&log_object, ML_LOG_NOTICE, "bar %d", 6);
    }

    ASSERT_SUBSTRING(output6, " NOTICE foo bar 6\n");

    /* Info. */
    CAPTURE_OUTPUT(output7, errput7) {
        ml_log_object_print(&log_object, ML_LOG_INFO, "bar %d", 7);
    }

    ASSERT_SUBSTRING(output7, " INFO foo bar 7\n");

    /* Debug. */
    CAPTURE_OUTPUT(output8, errput8) {
        ml_log_object_print(&log_object, ML_LOG_DEBUG, "bar %d", 8);
    }

    ASSERT_SUBSTRING(output8, " DEBUG foo bar 8\n");
}

TEST(enable_disable)
{
    struct ml_log_object_t log_object;

    ml_log_object_module_init();

    /* No debug, info and up. */
    ml_log_object_init(&log_object, "foo", ML_LOG_INFO);
    ASSERT_FALSE(ml_log_object_is_enabled_for(&log_object, ML_LOG_DEBUG));
    ASSERT_TRUE(ml_log_object_is_enabled_for(&log_object, ML_LOG_INFO));

    CAPTURE_OUTPUT(output1, errput1) {
        ml_log_object_print(&log_object, ML_LOG_DEBUG, "bar");
        ml_log_object_print(&log_object, ML_LOG_INFO, "bar");
    }

    ASSERT_SUBSTRING(output1, " INFO foo bar\n");
    ASSERT_NOT_SUBSTRING(output1, " DEBUG foo bar\n");
}

TEST(load)
{
    FILE *file_p;
    struct ml_log_object_t log_object;

    ml_log_object_module_init();

    file_p = fopen("log_object.txt", "w");
    ASSERT_NE(file_p, NULL);
    fprintf(file_p, "foo info\n");
    fprintf(file_p,
            "123456789012345678901234567890123456789012345678901234567890123 "
            "notice\n");
    /* Unknown name skipped. */
    fprintf(file_p, "bar alert\n");
    /* Unknown level skipped. */
    fprintf(file_p, "fie kalle\n");
    /* Long name aborts. */
    fprintf(file_p,
            "1234567890123456789012345678901234567890123456789012345678901230 "
            "debug\n");
    /* Not reached. */
    fprintf(file_p, "gam info\n");
    fclose(file_p);

    ml_log_object_get_by_name_mock_once("foo", &log_object);
    ml_log_object_set_level_mock_once(ML_LOG_INFO);

    ml_log_object_get_by_name_mock_once(
        "123456789012345678901234567890123456789012345678901234567890123",
        &log_object);
    ml_log_object_set_level_mock_once(ML_LOG_NOTICE);

    ml_log_object_get_by_name_mock_once("bar", NULL);

    ml_log_object_get_by_name_mock_once("fie", &log_object);

    ml_log_object_load();
}

TEST(store)
{
    ml_log_object_module_init();

    remove("log_object.txt");

    ml_log_object_store();

    ASSERT_FILE_EQ("log_object.txt", "files/log_object.txt");
}

TEST(store_open_error)
{
    ml_log_object_module_init();

    fopen_mock_once("log_object.txt", "w", NULL);
    fclose_mock_none();

    ml_log_object_store();
}
