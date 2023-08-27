#include "pe_trader.h"

#define SECS (2)

pid_t ppid;
volatile sig_atomic_t received;
int fdpair[2];

struct order {
    int id;
    time_t time;
};

int order_orderid(const void *a, const void *b, void *udata) {
    const struct order *oa = a, *ob = b;
    int i1 = oa->id, i2 = ob->id;
    int cmp = (i1 > i2) - (i1 < i2);
    return cmp;
}

void int_received(int sig, siginfo_t *info, void *ucontext){
    (void)!write(fdpair[1], &(info->si_pid), sizeof(pid_t));
    //time(&t);
    //(void)!write(fdpair[1], &t, sizeof(time_t));
    received = 1;
}

time_t t;
bool resend_signalsREAL(void *item, void *udata){
    struct order *o = item;
    //printf("%d\n", o->id);
    if (o->time + SECS <= t) {
        kill(ppid, SIGUSR1);
        o->time = t;
    }
    return true;
}

bool (*resend_signals)(const void*, void*) = (void*)resend_signalsREAL;

int main(int argc, char ** argv) {
    if (argc != 2) {
        printf("Not enough arguments\n");
        return 1;
    }
    
    ppid = getppid();
    int trader_id = (int)strtol(argv[1], NULL, 10);
    int order_id = 0;
    
    regex_t market_pattern;
    regcomp(&market_pattern, "^INVALID|((ACCEPTED|AMENDED|CANCELLED) [0-9]{1,6})|(MARKET (OPEN|((BUY|SELL|AMEND|CANCEL) [a-zA-Z0-9]{1,16} [1-9][0-9]{0,5} [1-9][0-9]{0,5})));", REG_EXTENDED);
    
    // register signal handler
    struct sigaction act = {0};
    act.sa_sigaction = int_received;
    act.sa_flags = SA_RESTART | SA_SIGINFO;
    signal(SIGUSR1, SIG_IGN);
    
    // connect to named pipes
    char buf[MAX_BUF], buf2[MAX_BUF];
    int read_fd, write_fd;
    sprintf(buf, FIFO_EXCHANGE, trader_id);
    read_fd = open(buf, O_RDONLY | O_NONBLOCK);
    sprintf(buf, FIFO_TRADER, trader_id);
    write_fd = open(buf, O_WRONLY);

    sigaction(SIGUSR1, &act, NULL);

    pipe2(fdpair, O_NONBLOCK);
    int epoll_fd = epoll_create1(0);
    struct epoll_event e, e2, e3;
    e.events = EPOLLERR | EPOLLHUP;
    e.data.u32 = 0;
    e2.events = EPOLLERR | EPOLLHUP;
    e2.data.u32 = 0;
    e3.events = EPOLLIN;
    e3.data.u32 = 1;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, read_fd, &e);
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, write_fd, &e2);
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fdpair[0], &e3);
    struct btree* order_tree = btree_new(sizeof(struct order), 0, order_orderid, NULL);


    /**
    * event loop:
    * 1: wait for signal
    * 2: if signal, turn on mask
    * 2.1: poll for fd input
    * 2.2: process input
    * 2.3: go back to 2.1
    * 3: turn off mask
    */ 
    int quit = 0;
    int start = 0;
    struct epoll_event event;
    while(!quit){
        //int status;
        //if ((status = epoll_wait(epoll_fd, &event, 1, -1)) > 0){
        if (epoll_wait(epoll_fd, &event, 1, 0) > 0){
            if (event.data.u32 == 0) {
                quit = 1;
                break;
            }
            //time to read
            pid_t pid;
            (void)!read(fdpair[0], &pid, sizeof(pid_t));
            if (pid != ppid) continue;

            int status, count = 0;

            status = read_msg(read_fd, buf);
            if (status <= 0) continue;
            
            count++;

            if (regexec(&market_pattern, buf, 0, NULL, 0)) continue; //bad regex
            if (start == 0) {
                if (strncmp(buf, "MARKET OPEN", 11) == 0) start = 1;
            } else if (start == 1){
                if (strncmp(buf, "MARKET SELL", 11) == 0) { //actually do stuff now
                    //check if over 1000
                    int quantity;
                    sscanf(buf, "MARKET SELL %*s %d %*d;", &quantity);
                    if (quantity >= 1000){
                        quit = 1;
                        break;
                    }
                    //place order
                    sprintf(buf2, "BUY %d %s", order_id, buf + 12);
                    btree_set(order_tree, &(struct order){.id = order_id, .time = time(NULL)});
                    order_id++;
                    
                    write(write_fd, buf2, strnlen(buf2, MAX_BUF));
                    kill(ppid, SIGUSR1);
                    //start = 2;
                } else if (strncmp(buf, "ACCEPTED", 7) == 0){
                    int id;
                    sscanf(buf, "ACCEPTED %d", &id);
                    btree_delete(order_tree, &(struct order){.id = id, .time = 0});
                }
            }
        }
        time(&t);
        btree_ascend(order_tree, NULL, resend_signals, NULL);
        sleep(1);
    }

    btree_free(order_tree);
    close(fdpair[0]), close(fdpair[1]);
    close(epoll_fd);
    close(read_fd);
    close(write_fd);
    regfree(&market_pattern);
    return 0;
}
