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
#include "nala.h"
#include "ml/ml.h"
#include "utils/utils.h"

ML_UID(timeout);

int setup()
{
    ml_init();

    return (0);
}

TEST(single_shot)
{
    struct ml_timer_t timer;
    struct ml_queue_t queue;
    struct ml_uid_t *uid_p;
    struct ml_timer_timeout_message_t *message_p;

    ml_queue_init(&queue, 1);
    ml_timer_init(&timer,
                  0,
                  &timeout,
                  &queue,
                  0);
    ml_timer_start(&timer);
    uid_p = ml_queue_get(&queue, (void **)&message_p);
    ASSERT_EQ(uid_p, &timeout);
    ASSERT_EQ(ml_timer_is_stopped(&timer), false);
    ml_message_free(message_p);
}

TEST(periodic)
{
    struct ml_timer_t timer;
    struct ml_queue_t queue;
    struct ml_uid_t *uid_p;
    struct ml_timer_timeout_message_t *message_p;
    int i;

    ml_queue_init(&queue, 1);
    ml_timer_init(&timer,
                  1,
                  &timeout,
                  &queue,
                  ML_TIMER_PERIODIC);
    ml_timer_start(&timer);

    for (i = 0; i < 10; i++) {
        uid_p = ml_queue_get(&queue, (void **)&message_p);
        ASSERT_EQ(uid_p, &timeout);
        ASSERT_EQ(ml_timer_is_stopped(&timer), false);
        ml_message_free(message_p);
    }

    ml_timer_stop(&timer);
}

TEST(stopped)
{
    struct ml_timer_t timer;
    struct ml_queue_t queue;
    struct ml_uid_t *uid_p;
    struct ml_timer_timeout_message_t *message_p;

    ml_queue_init(&queue, 1);
    ml_timer_init(&timer,
                  0,
                  &timeout,
                  &queue,
                  0);
    ml_timer_start(&timer);

    /* Wait for the timer to expire and then stop it. It should be
       marked as stopped. */
    uid_p = ml_queue_get(&queue, (void **)&message_p);
    ml_timer_stop(&timer);
    ASSERT_EQ(uid_p, &timeout);
    ASSERT_EQ(ml_timer_is_stopped(&timer), true);
    ml_message_free(message_p);
}

TEST(restart_after_timeout)
{
    struct ml_timer_t timer;
    struct ml_queue_t queue;
    struct ml_uid_t *uid_p;
    struct ml_timer_timeout_message_t *message_p;

    ml_queue_init(&queue, 1);
    ml_timer_init(&timer,
                  0,
                  &timeout,
                  &queue,
                  0);

    /* First start. */
    ml_timer_start(&timer);
    uid_p = ml_queue_get(&queue, (void **)&message_p);
    ASSERT_EQ(uid_p, &timeout);
    ASSERT_EQ(ml_timer_is_stopped(&timer), false);
    ml_message_free(message_p);

    /* Second start after timeout. */
    ml_timer_start(&timer);
    uid_p = ml_queue_get(&queue, (void **)&message_p);
    ASSERT_EQ(uid_p, &timeout);
    ASSERT_EQ(ml_timer_is_stopped(&timer), false);
    ml_message_free(message_p);
}

TEST(restart_after_stop)
{
    struct ml_timer_t timer;
    struct ml_queue_t queue;

    ml_queue_init(&queue, 1);
    ml_timer_init(&timer,
                  10000,
                  &timeout,
                  &queue,
                  0);

    /* First start and stop. */
    ml_timer_start(&timer);
    ml_timer_stop(&timer);

    /* Start and stop again. */
    ml_timer_start(&timer);
    ml_timer_stop(&timer);
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

    for (i = 0; i < 10; i++) {
        ml_queue_init(&queues[i], 1);
        ml_timer_init(&timers[i],
                      timeouts[i],
                      &timeout,
                      &queues[i],
                      0);
        ml_timer_start(&timers[i]);
    }

    for (i = 0; i < 10; i++) {
        uid_p = ml_queue_get(&queues[i], (void **)&message_p);
        ASSERT_EQ(uid_p, &timeout);
        ASSERT_EQ(ml_timer_is_stopped(&timers[i]), false);
        ml_message_free(message_p);
    }
}
