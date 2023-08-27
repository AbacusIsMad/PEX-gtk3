#define _DEFAULT_SOURCE
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <poll.h>
#include <sys/epoll.h>
#include <sys/wait.h>

#define FIFO_EXCHANGE "/tmp/pe_exchange_%d"
#define FIFO_TRADER "/tmp/pe_trader_%d"


int main(int argc, char **argv){
    if (argc < 3){
        puts("test trader: not enough args");
        exit(1);
    }

    pid_t ppid = getppid();
    signal(SIGUSR1, SIG_IGN);
    int trader_id = (int)strtol(argv[1], NULL, 10);
    
    int read_fd, write_fd;
    char buf[200];
    sprintf(buf, FIFO_EXCHANGE, trader_id);
    read_fd = open(buf, O_RDONLY | O_NONBLOCK);
    sprintf(buf, FIFO_TRADER, trader_id);
    write_fd = open(buf, O_WRONLY);
    
    FILE *fp = fopen(argv[2], "r");
    sleep(1);
    while(fgets(buf, 200, fp)){
        buf[strcspn(buf, "\n")] = 0;
        if (strlen(buf) < 5);
        else {
            write(write_fd, buf, strlen(buf));
            kill(ppid, SIGUSR1);
        }
        //sleep(1);
        usleep(40000); //40ms
    }
    
    close(read_fd);
    close(write_fd);
    fclose(fp);
    return 0;
}
