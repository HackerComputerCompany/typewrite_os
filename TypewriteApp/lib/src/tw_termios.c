#include "tw_termios.h"

#include <errno.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

static int g_raw_active;
static struct termios g_prev;

int tw_termios_set_nonblocking(int fd, int enable) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    if (enable)
        flags |= O_NONBLOCK;
    else
        flags &= ~O_NONBLOCK;
    return fcntl(fd, F_SETFL, flags);
}

int tw_termios_raw_enter(void) {
    struct termios t;
    if (g_raw_active)
        return 0;
    if (tcgetattr(STDIN_FILENO, &g_prev) != 0)
        return -1;
    t = g_prev;
    cfmakeraw(&t);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &t) != 0)
        return -1;
    (void)tw_termios_set_nonblocking(STDIN_FILENO, 1);
    g_raw_active = 1;
    return 0;
}

void tw_termios_raw_leave(void) {
    if (!g_raw_active)
        return;
    (void)tw_termios_set_nonblocking(STDIN_FILENO, 0);
    (void)tcsetattr(STDIN_FILENO, TCSANOW, &g_prev);
    g_raw_active = 0;
}

