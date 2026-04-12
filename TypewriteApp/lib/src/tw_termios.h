#pragma once

int tw_termios_raw_enter(void);
void tw_termios_raw_leave(void);
int tw_termios_set_nonblocking(int fd, int enable);

