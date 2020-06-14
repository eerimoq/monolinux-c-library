#include <unistd.h>
#include <string.h>
#include "nala.h"
#include "ml/ml.h"
#include "utils.h"

int stdin_pipe(void)
{
    int fds[2];

    ASSERT_EQ(pipe(fds), 0);
    dup2(fds[0], STDIN_FILENO);

    return (fds[1]);
}

void input(int fd, const char *string_p)
{
    ssize_t length;

    length = strlen(string_p);

    ASSERT_EQ(write(fd, string_p, length), length);
}


void ml_log_object_print_mock_va_arg_real(struct ml_log_object_t *self_p,
                                          int level,
                                          const char *fmt_p,
                                          va_list __nala_va_list)
{
    ml_log_object_vprint(self_p, level, fmt_p, __nala_va_list);
}
