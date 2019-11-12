#include <unistd.h>
#include "nala.h"
#include "ml/ml.h"
#include "utils.h"
#include "mocks/mock.h"

int setup()
{
    ml_init();

    return (0);
}

int teardown()
{
    mock_finalize();

    return (0);
}

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
