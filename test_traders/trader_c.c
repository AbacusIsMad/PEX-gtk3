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
#define MAX_BUF (50)

int read_msg(int fd, char* buf){
    for (int i = 0; i < MAX_BUF - 1; i++){
        if (!read(fd, buf, 1)) return -1;
        if (*(buf++) == ';') {
            *buf = '\0';
            return 0;
        }
    }
    while(1){
        if (!read(fd, buf, 1)) break;
        if (*buf == ';') break;
    }
    return -1;
}

volatile sig_atomic_t received = 0;

void int_received(int sig, siginfo_t *info, void *ucontext){
    received = 1;
}

//this listens to messages
int main(int argc, char **argv){
    struct sigaction act = {0};
    act.sa_sigaction = int_received;
    act.sa_flags = SA_RESTART | SA_SIGINFO;
    sigaction(SIGUSR1, &act, NULL);
    
    char buf[50] = {0}, buf2[50];
    int trader_id = (int)strtol(argv[1], NULL, 10);
    
    int read_fd, write_fd;
    sprintf(buf, FIFO_EXCHANGE, trader_id);
    read_fd = open(buf, O_RDONLY | O_NONBLOCK);
    sprintf(buf, FIFO_TRADER, trader_id);
    write_fd = open(buf, O_WRONLY);
    
    struct pollfd read_poll = {.fd = read_fd, .events = POLLIN, .revents = 0};
    sigset_t base_mask;
    sigemptyset(&base_mask);
    sigaddset(&base_mask, SIGUSR1);

    int count = 0;
    char *msg = "";

    /**
    * event loop:
    * 1: wait for signal
    * 2: if signal, turn on mask
    * 2.1: poll for fd input
    * 2.2: process input
    * 2.3: go back to 2.1
    * 3: turn off mask
    */
    alarm(10);
    int quit = 0;
    while(!quit){
        //1
        if (received == 0) pause();
        if (received == 0) continue;
        received = 0;
        //2
        sigprocmask (SIG_BLOCK, &base_mask, NULL);
        //2.1
        while(poll(&read_poll, 1, 0) > 0){
            //bad read
            if (read_msg(read_fd, buf)) break;
            //2.2
            printf("trader_c received: <%s>\n", buf);
        }
        //3
        sigprocmask (SIG_UNBLOCK, &base_mask, NULL);
    }

    return 0;
}
