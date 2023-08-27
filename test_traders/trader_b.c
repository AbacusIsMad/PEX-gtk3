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

int main(int argc, char **argv){
    signal(SIGUSR1, SIG_IGN);
    
    char buf[50] = {0}, buf2[50];
    int trader_id = (int)strtol(argv[1], NULL, 10);
    
    int read_fd, write_fd;
    sprintf(buf, FIFO_EXCHANGE, trader_id);
    read_fd = open(buf, O_RDONLY | O_NONBLOCK);
    sprintf(buf, FIFO_TRADER, trader_id);
    write_fd = open(buf, O_WRONLY);
    
    sleep(2);
    char *msg = "SELL 0 Coriander 15 200;", *msg2 = "SELL 1 Basil 3 100;", *msg3 = "SELL 2 GPU 1000 402;";
    write(write_fd, msg, strlen(msg));
    kill(getppid(), SIGUSR1);
    sleep(1);
    write(write_fd, msg2, strlen(msg2));
    kill(getppid(), SIGUSR1);
    sleep(2);
    write(write_fd, msg3, strlen(msg2));
    kill(getppid(), SIGUSR1);
    sleep(2);

    return 0;
}
