#ifndef PE_COMMON_H
#define PE_COMMON_H

//#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/wait.h>

#define FIFO_EXCHANGE "/tmp/pe_exchange_%d"
#define FIFO_TRADER "/tmp/pe_trader_%d"
#define FEE_PERCENTAGE 1

#define DEBUG_PRINT(x) printf x

#define MAX_BUF (50)
#define MAX_BUF_GUI (80)

#define SANDBOX 1

//read a message delimited by semicolon. If it somehow dies in the middle it returns an error.
int read_msg(int fd, char* buf);

#endif
