#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include <sys/time.h>
#include "subprocess.h"
#include "traceback.h"
#include "narwhal.h"

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"

#define ANSI_BOLD "\x1b[1m"
#define ANSI_RESET "\x1b[0m"

#define COLOR(color, ...) ANSI_RESET ANSI_COLOR_##color __VA_ARGS__ ANSI_RESET

#define BOLD(...) ANSI_RESET ANSI_BOLD __VA_ARGS__ ANSI_RESET

#define COLOR_BOLD(color, ...) \
    ANSI_RESET ANSI_COLOR_##color ANSI_BOLD __VA_ARGS__ ANSI_RESET

struct tests_t {
    struct narwhal_test_t *head_p;
    struct narwhal_test_t *tail_p;
};

struct capture_output_t {
    char **output_pp;
    size_t length;
    int original_fd;
    FILE *temporary_file_p;
    FILE *original_file_p;
};

static struct tests_t tests = {
    .head_p = NULL,
    .tail_p = NULL
};

static struct capture_output_t capture_stdout;
static struct capture_output_t capture_stderr;

int setup(void);

int teardown(void);

static void capture_output_init(struct capture_output_t *self_p,
                                FILE *file_p)
{
    self_p->output_pp = NULL;
    self_p->length = 0;
    self_p->original_file_p = file_p;
}

static void capture_output_destroy(struct capture_output_t *self_p)
{
    if (self_p->output_pp != NULL) {
        if (*self_p->output_pp != NULL) {
            free(*self_p->output_pp);
            self_p->output_pp = NULL;
        }
    }
}

static void capture_output_redirect(struct capture_output_t *self_p)
{
    fflush(self_p->original_file_p);

    self_p->temporary_file_p = tmpfile();
    self_p->original_fd = dup(fileno(self_p->original_file_p));

    while ((dup2(fileno(self_p->temporary_file_p),
                 fileno(self_p->original_file_p)) == -1)
           && (errno == EINTR));
}

static void capture_output_restore(struct capture_output_t *self_p)
{
    fflush(self_p->original_file_p);

    while ((dup2(self_p->original_fd, fileno(self_p->original_file_p)) == -1)
           && (errno == EINTR));

    close(self_p->original_fd);
}

static void capture_output_start(struct capture_output_t *self_p,
                                 char **output_pp)
{
    self_p->output_pp = output_pp;
    capture_output_redirect(self_p);
}

static void capture_output_stop(struct capture_output_t *self_p)
{
    ssize_t nmembers;

    if (self_p->output_pp == NULL) {
        return;
    }

    capture_output_restore(self_p);

    self_p->length = ftell(self_p->temporary_file_p);
    fseek(self_p->temporary_file_p, 0, SEEK_SET);
    *self_p->output_pp = malloc(self_p->length + 1);

    if (self_p->length > 0) {
        nmembers = fread(*self_p->output_pp,
                         self_p->length,
                         1,
                         self_p->temporary_file_p);

        if (nmembers != 1) {
            printf("Failed to read capture output.\n");
            FAIL();
        }
    }

    (*self_p->output_pp)[self_p->length] = '\0';
    fclose(self_p->temporary_file_p);

    printf("%s", *self_p->output_pp);
}

static float timeval_to_ms(struct timeval *timeval_p)
{
    float res;

    res = (float)timeval_p->tv_usec;
    res /= 1000;
    res += (float)(1000 * timeval_p->tv_sec);

    return (res);
}

static void print_test_results(struct narwhal_test_t *test_p,
                               float elapsed_time_ms)
{
    int total;
    int passed;
    int failed;
    const char *result_p;

    total = 0;
    passed = 0;
    failed = 0;

    printf("\nTest results:\n\n");
    fflush(stdout);

    while (test_p != NULL) {
        total++;

        if (test_p->exit_code == 0) {
            result_p = COLOR_BOLD(GREEN, "PASSED");
            passed++;
        } else {
            result_p = COLOR_BOLD(RED, "FAILED");
            failed++;
        }

        printf("  %s %s (" COLOR_BOLD(YELLOW, "%.02f ms") ")",
               result_p,
               test_p->name_p,
               test_p->elapsed_time_ms);

        if (test_p->signal_number != -1) {
            printf(" (signal: %d)", test_p->signal_number);
        }

        printf("\n");
        fflush(stdout);

        test_p = test_p->next_p;
    }

    printf("\nTests: ");

    if (failed > 0) {
        printf(COLOR_BOLD(RED, "%d failed") ", ", failed);
    }

    if (passed > 0) {
        printf(COLOR_BOLD(GREEN, "%d passed") ", ", passed);
    }

    printf("%d total\n", total);
    printf("Time: " COLOR_BOLD(YELLOW, "%.02f ms") "\n", elapsed_time_ms);
}

static void test_entry(void *arg_p)
{
    struct narwhal_test_t *test_p;
    int res;

    test_p = (struct narwhal_test_t *)arg_p;

    capture_output_init(&capture_stdout, stdout);
    capture_output_init(&capture_stderr, stderr);

    res = setup();

    if (res == 0) {
        test_p->func();
        res = teardown();
    }

    capture_output_destroy(&capture_stdout);
    capture_output_destroy(&capture_stderr);

    exit(res == 0 ? 0 : 1);
}

static int run_tests(struct narwhal_test_t *tests_p)
{
    int res;
    struct timeval start_time;
    struct timeval end_time;
    struct timeval test_start_time;
    struct timeval test_end_time;
    struct timeval elapsed_time;
    struct narwhal_test_t *test_p;
    struct subprocess_result_t *result_p;

    test_p = tests_p;
    gettimeofday(&start_time, NULL);
    res = 0;

    while (test_p != NULL) {
        printf("enter: %s\n", test_p->name_p);
        gettimeofday(&test_start_time, NULL);
        test_p->before_fork_func();

        result_p = subprocess_call(test_entry, test_p);

        test_p->exit_code = result_p->exit_code;
        test_p->signal_number = result_p->signal_number;
        subprocess_result_free(result_p);

        if (test_p->exit_code != 0) {
            res = 1;
        }

        gettimeofday(&test_end_time, NULL);
        timersub(&test_end_time, &test_start_time, &elapsed_time);
        test_p->elapsed_time_ms = timeval_to_ms(&elapsed_time);
        printf("exit: %s\n", test_p->name_p);
        test_p = test_p->next_p;
    }

    gettimeofday(&end_time, NULL);
    timersub(&end_time, &start_time, &elapsed_time);
    print_test_results(tests_p, timeval_to_ms(&elapsed_time));

    return (res);
}

bool narwhal_check_string_equal(const char *actual_p, const char *expected_p)
{
    return (strcmp(actual_p, expected_p) == 0);
}

bool narwhal_check_substring(const char *actual_p, const char *expected_p)
{
    return ((actual_p != NULL)
            && (expected_p != NULL)
            && (strstr(actual_p, expected_p) != NULL));
}

void narwhal_test_failure(void)
{
    narwhal_capture_output_stop();
    capture_output_destroy(&capture_stdout);
    capture_output_destroy(&capture_stderr);
    traceback_print();
    exit(1);
}

void narwhal_capture_output_start(char **output_pp, char **errput_pp)
{
    capture_output_start(&capture_stdout, output_pp);
    capture_output_start(&capture_stderr, errput_pp);
}

void narwhal_capture_output_stop()
{
    capture_output_stop(&capture_stdout);
    capture_output_stop(&capture_stderr);
}

void narwhal_capture_stderr_start(char **output_pp)
{
    capture_output_start(&capture_stderr, output_pp);
}

void narwhal_capture_stderr_stop()
{
    capture_output_stop(&capture_stderr);
}

void narwhal_register_test(struct narwhal_test_t *test_p)
{
    if (tests.head_p == NULL) {
        tests.head_p = test_p;
    } else {
        tests.tail_p->next_p = test_p;
    }

    tests.tail_p = test_p;
}

int narwhal_run_tests()
{
    return (run_tests(tests.head_p));
}

__attribute__((weak)) int setup(void)
{
    return (0);
}

__attribute__((weak)) int teardown(void)
{
    return (0);
}

__attribute__((weak)) int main(void)
{
    return (narwhal_run_tests());
}
