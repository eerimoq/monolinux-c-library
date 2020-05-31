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
 * This file is part of the Monolinux project.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/eventfd.h>
#include <pthread.h>
#include <sys/epoll.h>
#include "bunga_server.h"
#include "ml/ml.h"

struct execute_command_t {
    char *command_p;
    int res;
    struct {
        char *buf_p;
        size_t size;
    } output;
    struct {
        struct bunga_server_t *server_p;
        struct bunga_server_client_t *client_p;
    } bunga;
    struct ml_queue_t *queue_p;
};

static struct ml_queue_t queue;

static ML_UID(uid_execute_command_complete);

static void on_client_connected(struct bunga_server_t *self_p,
                                struct bunga_server_client_t *client_p)
{
    (void)self_p;
    (void)client_p;

    printf("Bunga client connected.\n");
}

static void on_client_disconnected(struct bunga_server_t *self_p,
                                   struct bunga_server_client_t *client_p)
{
    (void)self_p;
    (void)client_p;

    printf("Bunga client disconnected.\n");
}

static void execute_command_job(struct execute_command_t *command_p)
{
    FILE *fout_p;

    fout_p = open_memstream(&command_p->output.buf_p, &command_p->output.size);

    if (fout_p != NULL) {
        command_p->res = ml_shell_execute_command(command_p->command_p, fout_p);
    } else {
        command_p->res = -ENOMEM;
    }

    fclose(fout_p);

    ml_queue_put(command_p->queue_p, command_p);
}

static void on_execute_command_req(struct bunga_server_t *self_p,
                                   struct bunga_server_client_t *client_p,
                                   struct bunga_execute_command_req_t *request_p)
{
    struct execute_command_t *command_p;

    command_p = ml_message_alloc(&uid_execute_command_complete, sizeof(*command_p));
    command_p->command_p = strdup(request_p->command_p);
    command_p->bunga.server_p = self_p;
    command_p->bunga.client_p = client_p;
    command_p->queue_p = &queue;
    ml_spawn((ml_worker_pool_job_entry_t)execute_command_job, command_p);
}

static void handle_execute_command_complete(struct execute_command_t *command_p)
{
    struct bunga_execute_command_rsp_t *response_p;
    char *output_p;
    size_t offset;
    size_t size;
    char saved;
    size_t chunk_size;
    struct bunga_server_t *server_p;
    struct bunga_server_client_t *client_p;

    server_p = command_p->bunga.server_p;
    client_p = command_p->bunga.client_p;

    /* Output. */
    output_p = command_p->output.buf_p;
    size = command_p->output.size;
    chunk_size = 96;

    for (offset = 0; offset < size; offset += chunk_size) {
        if ((size - offset) < 96) {
            chunk_size = (size - offset);
        }

        saved = output_p[offset + chunk_size];
        output_p[offset + chunk_size] = '\0';

        response_p = bunga_server_init_execute_command_rsp(server_p);
        response_p->kind = bunga_execute_command_rsp_kind_output_e;
        response_p->output_p = &output_p[offset];
        bunga_server_send(server_p, client_p);

        output_p[offset + chunk_size] = saved;
    }

    /* Command result. */
    response_p = bunga_server_init_execute_command_rsp(server_p);

    if (command_p->res == 0) {
        response_p->kind = bunga_execute_command_rsp_kind_ok_e;
    } else {
        response_p->kind = bunga_execute_command_rsp_kind_error_e;
        response_p->error_p = strerror(-command_p->res);
    }

    bunga_server_send(server_p, client_p);

    free(command_p->command_p);
    free(command_p->output.buf_p);
    ml_message_free(command_p);
}

static void on_put_signal_event(int *fd_p)
{
    uint64_t value;
    ssize_t size;

    value = 1;
    size = write(*fd_p, &value, sizeof(value));
    (void)size;
}

static void *server_main()
{
    struct bunga_server_t server;
    struct bunga_server_client_t clients[2];
    uint8_t clients_input_buffers[2][128];
    uint8_t message[128];
    uint8_t workspace_in[128];
    uint8_t workspace_out[128];
    int epoll_fd;
    int put_fd;
    struct epoll_event event;
    int res;
    struct ml_uid_t *uid_p;
    void *message_p;
    uint64_t value;

    printf("Starting a Bunga server on ':28000'.\n");

    epoll_fd = epoll_create1(0);

    if (epoll_fd == -1) {
        printf("epoll_create1() failed.\n");
        return (NULL);
    }

    put_fd = eventfd(0, EFD_SEMAPHORE);

    if (put_fd == -1) {
        printf("eventfd() failed.\n");

        return (NULL);
    }

    event.events = EPOLLIN;
    event.data.fd = put_fd;

    res = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, put_fd, &event);

    if (res == -1) {
        printf("epoll_ctl_add() failed.\n");

        return (NULL);
    }

    ml_queue_init(&queue, 32);
    ml_queue_set_on_put(&queue, (ml_queue_put_t)on_put_signal_event, &put_fd);

    res = bunga_server_init(&server,
                            "tcp://:28000",
                            &clients[0],
                            2,
                            &clients_input_buffers[0][0],
                            sizeof(clients_input_buffers[0]),
                            &message[0],
                            sizeof(message),
                            &workspace_in[0],
                            sizeof(workspace_in),
                            &workspace_out[0],
                            sizeof(workspace_out),
                            on_client_connected,
                            on_client_disconnected,
                            on_execute_command_req,
                            epoll_fd,
                            NULL);

    if (res != 0) {
        printf("Init failed.\n");

        return (NULL);
    }

    res = bunga_server_start(&server);

    if (res != 0) {
        printf("Start failed.\n");

        return (NULL);
    }

    printf("Server started.\n");

    while (true) {
        res = epoll_wait(epoll_fd, &event, 1, -1);

        if (res != 1) {
            break;
        }

        if (event.data.fd == put_fd) {
            res = read(put_fd, &value, sizeof(value));
            (void)res;
            uid_p = ml_queue_get(&queue, &message_p);

            if (uid_p == &uid_execute_command_complete) {
                handle_execute_command_complete(message_p);
            }
        } else {
            bunga_server_process(&server, event.data.fd, event.events);
        }
    }

    return (NULL);
}

void ml_bunga_server_create(void)
{
    pthread_t pthread;

    pthread_create(&pthread, NULL, server_main, NULL);
}
