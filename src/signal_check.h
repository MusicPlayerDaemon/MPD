#ifndef SIGNAL_CHECK_H
#define SIGNAL_CHECK_H

#include <signal.h>

void signal_handle(int sig);
void signal_unhandle(int sig);
int signal_is_pending(int sig);
void signal_clear(int sig);

#endif /* SIGNAL_CHECK_H */

