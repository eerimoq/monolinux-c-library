#ifndef NARWHAL_H
#define NARWHAL_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST(name)                                              \
    void name(void);                                            \
    void name ## _before_fork() {}                              \
    static struct narwhal_test_t narwhal_test_ ## name = {      \
        .name_p = #name,                                        \
        .func = name,                                           \
        .before_fork_func = name ## _before_fork,               \
        .next_p = NULL                                          \
    };                                                          \
    __attribute__ ((constructor))                               \
    void narwhal_constructor_ ## name(void)                     \
    {                                                           \
        narwhal_register_test(&narwhal_test_ ## name);          \
    }                                                           \
    void name(void)

#define REGISTER_TEST(name)                             \
    narwhal_register_test(&narwhal_test_ ## name)

#define NARWHAL_TEST_FAILURE() narwhal_test_failure()

#define NARWHAL_PRINT_FORMAT(value)                     \
    _Generic((value),                                   \
             char: "%c",                                \
             const char: "%c",                          \
             signed char: "%hhd",                       \
             const signed char: "%hhd",                 \
             unsigned char: "%hhu",                     \
             const unsigned char: "%hhu",               \
             signed short: "%hd",                       \
             const signed short: "%hd",                 \
             unsigned short: "%hu",                     \
             const unsigned short: "%hu",               \
             signed int: "%d",                          \
             const signed int: "%d",                    \
             unsigned int: "%u",                        \
             const unsigned int: "%u",                  \
             long int: "%ld",                           \
             const long int: "%ld",                     \
             unsigned long int: "%lu",                  \
             const unsigned long int: "%lu",            \
             long long int: "%lld",                     \
             const long long int: "%lld",               \
             unsigned long long int: "%llu",            \
             const unsigned long long int: "%llu",      \
             float: "%f",                               \
             const float: "%f",                         \
             double: "%f",                              \
             const double: "%f",                        \
             long double: "%Lf",                        \
             const long double: "%Lf",                  \
             char *: "\"%s\"",                          \
             const char *: "\"%s\"",                    \
             bool: "%d",                                \
    default: "%p")

#define NARWHAL_BINARY_ASSERTION(left, right, check, assertion, message) \
    do {                                                                \
        __typeof__(left) _narwhal_assert_left = (left);                 \
        __typeof__(right) _narwhal_assert_right = (right);              \
                                                                        \
        if (!check(_narwhal_assert_left, _narwhal_assert_right)) {      \
            char _narwhal_assert_message[1024];                         \
            snprintf(_narwhal_assert_message,                           \
                     sizeof(_narwhal_assert_message),                   \
                     message "\n",                                      \
                     NARWHAL_PRINT_FORMAT(_narwhal_assert_left),        \
                     NARWHAL_PRINT_FORMAT(_narwhal_assert_right));      \
            printf(_narwhal_assert_message,                             \
                   _narwhal_assert_left,                                \
                   _narwhal_assert_right);                              \
            NARWHAL_TEST_FAILURE();                                     \
        }                                                               \
    } while (0)

#define NARWHAL_CHECK_EQ(left, right)                   \
    _Generic(                                           \
        (left),                                         \
        char *: _Generic(                               \
            (right),                                    \
            char *: narwhal_check_string_equal(         \
                (char *)(uintptr_t)(left),              \
                (char *)(uintptr_t)(right)),            \
            const char *: narwhal_check_string_equal(   \
                (char *)(uintptr_t)(left),              \
                (char *)(uintptr_t)(right)),            \
            default: false),                            \
        const char *: _Generic(                         \
            (right),                                    \
            char *: narwhal_check_string_equal(         \
                (char *)(uintptr_t)(left),              \
                (char *)(uintptr_t)(right)),            \
            const char *: narwhal_check_string_equal(   \
                (char *)(uintptr_t)(left),              \
                (char *)(uintptr_t)(right)),            \
            default: false),                            \
        default: (left) == (right))

#define NARWHAL_CHECK_NE(left, right)                           \
    _Generic(                                                   \
        (left),                                                 \
        char *: _Generic(                                       \
            (right),                                            \
            char *: (!narwhal_check_string_equal(               \
                         (char *)(uintptr_t)(left),             \
                         (char *)(uintptr_t)(right))),          \
            const char *: (!narwhal_check_string_equal(         \
                               (char *)(uintptr_t)(left),       \
                               (char *)(uintptr_t)(right))),    \
            default: true),                                     \
        const char *: _Generic(                                 \
            (right),                                            \
            char *: (!narwhal_check_string_equal(               \
                         (char *)(uintptr_t)(left),             \
                         (char *)(uintptr_t)(right))),          \
            const char *: (!narwhal_check_string_equal(         \
                               (char *)(uintptr_t)(left),       \
                               (char *)(uintptr_t)(right))),    \
            default: true),                                     \
        default: (left) != (right))

#define NARWHAL_CHECK_LT(left, right) ((left) < (right))

#define NARWHAL_CHECK_LE(left, right) ((left) <= (right))

#define NARWHAL_CHECK_GT(left, right) ((left) > (right))

#define NARWHAL_CHECK_GE(left, right) ((left) >= (right))

#define NARWHAL_CHECK_SUBSTRING(left, right) narwhal_check_substring(left, right)

#define NARWHAL_CHECK_NOT_SUBSTRING(left, right) (!narwhal_check_substring(left, right))

#define ASSERT_EQ(left, right)                                          \
    NARWHAL_BINARY_ASSERTION(left,                                      \
                             right,                                     \
                             NARWHAL_CHECK_EQ,                          \
                             #left " == " #right,                       \
                             "First argument %s is not equal to %s.")

#define ASSERT_NE(left, right)                                          \
    NARWHAL_BINARY_ASSERTION(left,                                      \
                             right,                                     \
                             NARWHAL_CHECK_NE,                          \
                             #left " != " #right,                       \
                             "First argument %s is not different from %s.")

#define ASSERT_LT(left, right)                                          \
    NARWHAL_BINARY_ASSERTION(left,                                      \
                             right,                                     \
                             NARWHAL_CHECK_LT,                          \
                             #left " < " #right,                        \
                             "First argument %s is not less than %s.")

#define ASSERT_LE(left, right)                                          \
    NARWHAL_BINARY_ASSERTION(left,                                      \
                             right,                                     \
                             NARWHAL_CHECK_LE,                          \
                             #left " <= " #right,                       \
                             "First argument %s is not less than or equal to %s.")

#define ASSERT_GT(left, right)                                          \
    NARWHAL_BINARY_ASSERTION(left,                                      \
                             right,                                     \
                             NARWHAL_CHECK_GT,                          \
                             #left " > " #right,                        \
                             "First argument %s is not greater than %s.")

#define ASSERT_GE(left, right)                                          \
    NARWHAL_BINARY_ASSERTION(left,                                      \
                             right,                                     \
                             NARWHAL_CHECK_GE,                          \
                             #left " >= " #right,                       \
                             "First argument %s is not greater or equal to %s.")

#define ASSERT_SUBSTRING(string, substring)                             \
    NARWHAL_BINARY_ASSERTION(string,                                    \
                             substring,                                 \
                             NARWHAL_CHECK_SUBSTRING,                   \
                             "strstr(" #string ", " #substring ") != NULL", \
                             "First argument %s doesn't contain %s.")

#define ASSERT_NOT_SUBSTRING(string, substring)                         \
    NARWHAL_BINARY_ASSERTION(string,                                    \
                             substring,                                 \
                             NARWHAL_CHECK_NOT_SUBSTRING,               \
                             "strstr(" #string ", " #substring ") == NULL", \
                             "First argument %s contains %s.")

#define ASSERT_MEMORY(left, right, size)                \
    do {                                                \
        if (memcmp((left), (right), (size)) != 0) {     \
            NARWHAL_TEST_FAILURE();                     \
        }                                               \
    } while (0)

#define ASSERT(cond)                            \
    if (!(cond)) {                              \
        NARWHAL_TEST_FAILURE();                 \
    }

#define FAIL() NARWHAL_TEST_FAILURE()

#define CAPTURE_OUTPUT(stdout_name, stderr_name)                        \
    int stdout_name ## i;                                               \
    char *stdout_name = NULL;                                           \
    char *stderr_name = NULL;                                           \
                                                                        \
    for (stdout_name ## i = 0, narwhal_capture_output_start(&stdout_name, \
                                                            &stderr_name); \
         stdout_name ## i < 1;                                          \
         stdout_name ## i++, narwhal_capture_output_stop())

struct narwhal_test_t {
    const char *name_p;
    void (*func)(void);
    void (*before_fork_func)(void);
    int exit_code;
    int signal_number;
    float elapsed_time_ms;
    struct narwhal_test_t *next_p;
};

bool narwhal_check_string_equal(const char *actual_p, const char *expected_p);

bool narwhal_check_substring(const char *actual_p, const char *expected_p);

void narwhal_capture_output_start(char **stdout_pp, char **stderr_pp);

void narwhal_capture_output_stop(void);

__attribute__ ((noreturn)) void narwhal_test_failure(void);

void narwhal_register_test(struct narwhal_test_t *test_p);

int narwhal_run_tests(void);

#endif
