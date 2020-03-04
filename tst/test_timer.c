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
#include "nala.h"
#include "nala_mocks.h"
#include "ml/ml.h"

static ML_UID(timeout);

TEST(single_shot)
{
    struct ml_timer_t timer;
    struct ml_queue_t queue;
    struct ml_uid_t *uid_p;
    struct ml_timer_timeout_message_t *message_p;

    ml_init();
    ml_queue_init(&queue, 1);
    ml_timer_init(&timer, &timeout, &queue);
    ml_timer_start(&timer, 0, 0);
    uid_p = ml_queue_get(&queue, (void **)&message_p);
    ASSERT_EQ(uid_p, &timeout);
    ASSERT_EQ(ml_timer_is_message_valid(&timer), true);
    ml_message_free(message_p);
}

TEST(periodic)
{
    struct ml_timer_t timer;
    struct ml_queue_t queue;
    struct ml_uid_t *uid_p;
    struct ml_timer_timeout_message_t *message_p;
    int i;

    ml_init();
    ml_queue_init(&queue, 1);
    ml_timer_init(&timer, &timeout, &queue);
    ml_timer_start(&timer, 1, 1);

    for (i = 0; i < 10; i++) {
        uid_p = ml_queue_get(&queue, (void **)&message_p);
        ASSERT_EQ(uid_p, &timeout);
        ASSERT_EQ(ml_timer_is_message_valid(&timer), true);
        ml_message_free(message_p);
    }

    ml_timer_stop(&timer);
}

TEST(is_message_valid)
{
    struct ml_timer_t timer;
    struct ml_queue_t queue;
    struct ml_uid_t *uid_p;
    struct ml_timer_timeout_message_t *message_p;

    ml_init();
    ml_queue_init(&queue, 1);
    ml_timer_init(&timer, &timeout, &queue);
    ml_timer_start(&timer, 0, 0);

    /* Wait for the timer to expire and then stop it. The received
       message should be invalid. */
    uid_p = ml_queue_get(&queue, (void **)&message_p);
    ml_timer_stop(&timer);
    ASSERT_EQ(uid_p, &timeout);
    ASSERT_EQ(ml_timer_is_message_valid(&timer), false);
    ml_message_free(message_p);
}

TEST(restart_after_timeout)
{
    struct ml_timer_t timer;
    struct ml_queue_t queue;
    struct ml_uid_t *uid_p;
    struct ml_timer_timeout_message_t *message_p;

    ml_init();
    ml_queue_init(&queue, 1);
    ml_timer_init(&timer, &timeout, &queue);

    /* First start. */
    ml_timer_start(&timer, 0, 0);
    uid_p = ml_queue_get(&queue, (void **)&message_p);
    ASSERT_EQ(uid_p, &timeout);
    ASSERT_EQ(ml_timer_is_message_valid(&timer), true);
    ml_message_free(message_p);

    /* Second start after timeout. */
    ml_timer_start(&timer, 0, 0);
    uid_p = ml_queue_get(&queue, (void **)&message_p);
    ASSERT_EQ(uid_p, &timeout);
    ASSERT_EQ(ml_timer_is_message_valid(&timer), true);
    ml_message_free(message_p);
}

TEST(restart_after_stop)
{
    struct ml_timer_t timer;
    struct ml_queue_t queue;

    ml_init();
    ml_queue_init(&queue, 1);
    ml_timer_init(&timer, &timeout, &queue);

    /* First start and stop. */
    ml_timer_start(&timer, 10000, 0);
    ml_timer_stop(&timer);

    /* Start and stop again. */
    ml_timer_start(&timer, 10000, 0);
    ml_timer_stop(&timer);
}

TEST(restart_without_stop)
{
    struct ml_timer_t timer;
    struct ml_queue_t queue;
    struct ml_uid_t *uid_p;
    struct ml_timer_timeout_message_t *message_p;

    ml_init();
    ml_queue_init(&queue, 1);
    ml_timer_init(&timer, &timeout, &queue);

    ml_timer_start(&timer, 10000, 0);
    ml_timer_start(&timer, 0, 0);
    uid_p = ml_queue_get(&queue, (void **)&message_p);
    ASSERT_EQ(uid_p, &timeout);
    ASSERT_EQ(ml_timer_is_message_valid(&timer), true);
    ml_message_free(message_p);
}

TEST(multiple_timers)
{
    int timeouts[10] = {
        50, 0, 100, 75, 50, 50, 100, 90, 10, 0
    };
    struct ml_timer_t timers[10];
    struct ml_queue_t queues[10];
    struct ml_uid_t *uid_p;
    struct ml_timer_timeout_message_t *message_p;
    int i;

    ml_init();

    for (i = 0; i < 10; i++) {
        ml_queue_init(&queues[i], 1);
        ml_timer_init(&timers[i], &timeout, &queues[i]);
        ml_timer_start(&timers[i], timeouts[i], 0);
    }

    for (i = 0; i < 10; i++) {
        uid_p = ml_queue_get(&queues[i], (void **)&message_p);
        ASSERT_EQ(uid_p, &timeout);
        ASSERT_EQ(ml_timer_is_message_valid(&timers[i]), true);
        ml_message_free(message_p);
    }
}
