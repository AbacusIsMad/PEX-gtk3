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
    
    sleep(1);
    char *msg = "SELL 0 Basil 4 100;", *msg2 = "BUY 1 Chives 1 200;",
    *msg3 = "BUY 2 Coriander 20 200;", *msg4 = "SELL 3 Chives 2 100;";
    write(write_fd, msg, strlen(msg));
    kill(getppid(), SIGUSR1);
    sleep(3);
    write(write_fd, msg2, strlen(msg2));
    kill(getppid(), SIGUSR1);
    sleep(2);
    write(write_fd, msg3, strlen(msg3));
    kill(getppid(), SIGUSR1);
    sleep(1);
    write(write_fd, msg4, strlen(msg4));
    kill(getppid(), SIGUSR1);
    sleep(1);

    return 0;
}
