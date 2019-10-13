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
 *
 * Based on the Simba (https://github.com/eerimoq/simba) timer
 * implementation (MIT).
 */

#include <stdint.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include "ml/ml.h"
#include "internal.h"

static void expired_list_insert(struct ml_timer_t *self_p,
                                struct ml_timer_timeout_message_t *message_p)
{
    message_p->next_p = self_p->expired_list.next_p;
    message_p->prev_p = &self_p->expired_list;
    self_p->expired_list.next_p = message_p;
}

static void expired_list_remove(struct ml_timer_t *self_p,
                                struct ml_timer_timeout_message_t *message_p)
{
    pthread_mutex_lock(&self_p->handler_p->mutex);
    message_p->prev_p->next_p = message_p->next_p;
    pthread_mutex_unlock(&self_p->handler_p->mutex);
}

static void on_free(struct ml_timer_timeout_message_t *message_p)
{
    expired_list_remove(message_p->timer_p, message_p);
}

static void timer_list_insert(struct ml_timer_list_t *self_p,
                              struct ml_timer_t *timer_p)
{
    struct ml_timer_t *elem_p;
    struct ml_timer_t *prev_p;

    /* Find element preceeding this timer. */
    elem_p = self_p->head_p;
    prev_p = NULL;

    /* Find the element to insert this timer before. Delta is
       initially the timeout. */
    while (elem_p->delta < timer_p->delta) {
        timer_p->delta -= elem_p->delta;
        prev_p = elem_p;
        elem_p = elem_p->next_p;
    }

    /* Adjust the next timer for this timers delta. Do not adjust the
       tail timer. */
    if (elem_p != &self_p->tail) {
        elem_p->delta -= timer_p->delta;
    }

    /* Insert the new timer into list. */
    timer_p->next_p = elem_p;

    if (prev_p == NULL) {
        self_p->head_p = timer_p;
    } else {
        prev_p->next_p = timer_p;
    }
}

/**
 * Remove given timer from given list of active timers.
 */
static void timer_list_remove(struct ml_timer_list_t *self_p,
                              struct ml_timer_t *timer_p)
{
    struct ml_timer_t *elem_p;
    struct ml_timer_t *prev_p;

    /* Find element preceeding this timer.*/
    elem_p = self_p->head_p;
    prev_p = NULL;

    while (elem_p != NULL) {
        if (elem_p == timer_p) {
            /* Remove the timer from the list. */
            if (prev_p != NULL) {
                prev_p->next_p = elem_p->next_p;
            } else {
                self_p->head_p = elem_p->next_p;
            }

            /* Add the delta timeout to the next timer. */
            if (elem_p->next_p != &self_p->tail) {
                elem_p->next_p->delta += elem_p->delta;
            }

            return;
        }

        prev_p = elem_p;
        elem_p = elem_p->next_p;
    }
}

static void fire_timer(struct ml_timer_t *self_p)
{
    struct ml_timer_timeout_message_t *message_p;

    message_p = ml_message_alloc(self_p->message_p, sizeof(*message_p));
    ml_message_set_on_free(message_p, (ml_message_on_free_t)on_free);
    message_p->stopped = false;
    message_p->timer_p = self_p;
    expired_list_insert(self_p, message_p);
    ml_queue_put(self_p->queue_p, message_p);
}

static void tick(struct ml_timer_handler_t *self_p)
{
    struct ml_timer_t *timer_p;
    struct ml_timer_list_t *list_p;

    pthread_mutex_lock(&self_p->mutex);
    list_p = &self_p->timers;

    /* Return if no timers are active.*/
    if (list_p->head_p != &list_p->tail) {
        /* Fire all expired timers.*/
        list_p->head_p->delta--;

        while (list_p->head_p->delta == 0) {
            timer_p = list_p->head_p;
            list_p->head_p = timer_p->next_p;
            fire_timer(timer_p);

            /* Re-set periodic timers. */
            if (timer_p->flags & ML_TIMER_PERIODIC) {
                timer_p->delta = timer_p->timeout;
                timer_list_insert(list_p, timer_p);
            }
        }
    }

    pthread_mutex_unlock(&self_p->mutex);
}

static void *handler_main(struct ml_timer_handler_t *handler_p)
{
    struct itimerspec timeout;
    uint64_t value;
    ssize_t res;

    handler_p->fd = timerfd_create(CLOCK_REALTIME, 0);

    if (handler_p->fd == -1) {
        return (NULL);
    }

    timeout.it_value.tv_sec = 0;
    timeout.it_value.tv_nsec = 10000000;
    timeout.it_interval.tv_sec= 0;
    timeout.it_interval.tv_nsec = 10000000;
    timerfd_settime(handler_p->fd, 0, &timeout, NULL);

    while (true) {
        res = read(handler_p->fd, &value, sizeof(value));

        if (res != sizeof(value)) {
            sleep(1);
            continue;
        }

        tick(handler_p);
    }

    return (NULL);
}

void ml_timer_handler_init(struct ml_timer_handler_t *self_p)
{
    self_p->timers.head_p = &self_p->timers.tail;
    self_p->timers.tail.next_p = NULL;
    self_p->timers.tail.delta = -1;

    pthread_create(&self_p->pthread,
                   NULL,
                   (void *(*)(void *))handler_main,
                   self_p);
    pthread_mutex_init(&self_p->mutex, NULL);
}

void ml_timer_handler_timer_init(struct ml_timer_handler_t *self_p,
                                 struct ml_timer_t *timer_p,
                                 unsigned int timeout_ms,
                                 struct ml_uid_t *message_p,
                                 struct ml_queue_t *queue_p,
                                 int flags)
{
    timer_p->handler_p = self_p;
    timer_p->timeout = (timeout_ms / 10);

    if (timer_p->timeout == 0) {
        timer_p->timeout = 1;
    }

    timer_p->message_p = message_p;
    timer_p->queue_p = queue_p;
    timer_p->flags = flags;
    timer_p->expired_list.next_p = NULL;
    timer_p->expired_list.prev_p = NULL;
}

void ml_timer_handler_timer_start(struct ml_timer_t *self_p)
{
    self_p->delta = self_p->timeout;

    /* Must wait at least two ticks to ensure the timer does not
       expire early since it may be started close to the next tick
       occurs. */
    self_p->delta++;

    pthread_mutex_lock(&self_p->handler_p->mutex);
    timer_list_insert(&self_p->handler_p->timers, self_p);
    pthread_mutex_unlock(&self_p->handler_p->mutex);
}

void ml_timer_handler_timer_stop(struct ml_timer_t *self_p)
{
    struct ml_timer_timeout_message_t *message_p;

    pthread_mutex_lock(&self_p->handler_p->mutex);
    message_p = self_p->expired_list.next_p;

    while (message_p != NULL) {
        message_p->stopped = true;
        message_p = message_p->next_p;
    }

    timer_list_remove(&self_p->handler_p->timers, self_p);
    pthread_mutex_unlock(&self_p->handler_p->mutex);
}