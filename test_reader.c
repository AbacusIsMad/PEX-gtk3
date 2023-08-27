#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>
#include <limits.h>
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
        if (read(fd, buf, 1) <= 0) return -1;
        if (*(buf++) == ';') {
            *buf = '\0';
            return 0;
        }
    }
    while(1){
        if (read(fd, buf, 1) <= 0) break;
        if (*buf == ';') break;
    }
    return -1;
}

volatile sig_atomic_t received = 0;

void int_received(int sig, siginfo_t *info, void *ucontext){
    received = 1;
}

int pipefd[2], t_pipefd[2];

FILE *fp;

int count = 0;

void term_received(int sig, siginfo_t *info, void *ucontext){
    char buf[PIPE_BUF + 1] = {0};
    usleep(500000);
    puts("test_receiver received:");
    
    fclose(fp);
    fp = fopen(".read_tmp", "r");
    for (int i = 0; i < count; i++){
        fgets(buf, PIPE_BUF, fp);
        printf("%s", buf);
    }
    
    
    fclose(fp);
    exit(0);
}

//this listens to messages
int main(int argc, char **argv){
    struct sigaction act = {0};
    act.sa_sigaction = int_received;
    act.sa_flags = SA_RESTART | SA_SIGINFO;
    sigaction(SIGUSR1, &act, NULL);
    
    fp = fopen(".read_tmp", "w+");
    
    
    act.sa_sigaction = term_received;
    sigaction(SIGTERM, &act, NULL);
    
    char buf[50] = {0};
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

    /**
    * event loop:
    * 1: wait for signal
    * 2: if signal, turn on mask
    * 2.1: poll for fd input
    * 2.2: process input
    * 2.3: go back to 2.1
    * 3: turn off mask
    */
    //alarm(10);
    int quit = 0;
    
    //struct timeval tv_start;
    //gettimeofday(&tv_start, NULL);
    //unsigned long long start_ms = (unsigned long long)(tv_start.tv_sec) * 1000 + (unsigned long long)(tv_start.tv_usec) / 1000;
    
    while(!quit){
        //1
        if (received == 0) pause();
        if (received == 0) continue;
        received = 0;
        //2
        sigprocmask (SIG_BLOCK, &base_mask, NULL);
        //2.1
        if(poll(&read_poll, 1, 0) > 0){
            //bad read
            if (read_msg(read_fd, buf) == -1) break;
            //2.2
            //printf("trader_receiver received: <%s>\n", buf);
            //write(pipefd[1], buf, strlen(buf));
            //struct timeval tv_current;
            //gettimeofday(&tv_current, NULL);
            //unsigned long long cur_ms = (unsigned long long)(tv_current.tv_sec) * 1000 + (unsigned long long)(tv_current.tv_usec) / 1000;
            
            //write(t_pipefd[1], &offset, sizeof(time_t));
            //fprintf(fp, "[t=%.01f] %s\n", ((float)(cur_ms - start_ms))/1000, buf);
            fprintf(fp, "[t=xxx] %s\n", buf);
            count++;
        }
        //3
        sigprocmask (SIG_UNBLOCK, &base_mask, NULL);
    }
    
    close(read_fd);
    close(write_fd);

    return 0;
}
