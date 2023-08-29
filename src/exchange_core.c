#include "exchange_core.h"

#define USE_THREAD (1)
#if USE_THREAD == 1
	#define DO_EXIT(x) return x
#else
	#define DO_EXIT(x) exit(x)
#endif

int globl_write_fd, globl_tid, globl_pid;

void globl_usr1(int signum){
    (void)!write(globl_write_fd, &globl_tid, sizeof(int));
}

//sandbox uses usr2 to communicate, so we convert it to usr1
void globl_usr2(int signum){
    kill(globl_pid, SIGUSR1);
}

void globl_term(int signum){
	kill(globl_pid, SIGTERM);
	exit(0); //shouldn't kill all threads as it is a subprocess
}

int exc_init(struct exchange *exchange, char* product_file, int trader_num, char **argv){
    /**
    * the strat:
    * 0: setup static ones
    * 1: read file and get names
    * 2: initialise buy and sell trees
    * 3: setup traders
    * 4: setup files, polling
    * 5: create children
    * 
    */
    //0
    puts("[PEX] Starting");
    exchange->global_order_id = 0;
    exchange->fees = 0;
    regcomp(&(exchange->trader_pattern),
        "^((CANCEL [0-9]{1,6})|(AMEND [0-9]{1,6} [1-9][0-9]{0,5} [1-9][0-9]{0,5})|"
        "((BUY|SELL) [0-9]{1,6} [a-zA-Z0-9]{1,16} [1-9][0-9]{0,5} [1-9][0-9]{0,5}));",
        REG_EXTENDED | REG_NEWLINE);
    //1
    FILE *fp = fopen(product_file, "r");
    if (fp == NULL) {
    	perror("could not open product file");
    	DO_EXIT(1);
  	}
    char buf[PRODUCT_LINE];
    (void)!fgets(buf, PRODUCT_LINE, fp);
    exchange->item_no = atoi(buf);
    exchange->item_names = malloc(exchange->item_no * PRODUCT_LINE);
    for(int i = 0; i < exchange->item_no; i++){
        (void)!fgets(exchange->item_names[i], PRODUCT_LINE, fp);
        exchange->item_names[i][strcspn(exchange->item_names[i], "\n")] = 0;
    }
    fclose(fp);
    printf("[PEX] Trading %d products:", exchange->item_no);
    for (int i = 0; i < exchange->item_no; i++) printf(" %s", exchange->item_names[i]);
    putchar('\n');
    //2
    exchange->sell_tree = malloc(exchange->item_no * sizeof(struct btree*));
    exchange->buy_tree = malloc(exchange->item_no * sizeof(struct btree*));
    for(int i = 0; i < exchange->item_no; i++){
        exchange->sell_tree[i] = btree_new(sizeof(struct order_node), 0, order_price_min, NULL);
        exchange->buy_tree[i] = btree_new(sizeof(struct order_node), 0, order_price_max, NULL);
    }
    //3
    exchange->trader_num = trader_num;
    exchange->traders_active = trader_num;
    exchange->traders = malloc(trader_num * sizeof(struct trader));
    for(int i = 0; i < trader_num; i++){
        struct trader *t = exchange->traders + i;
        t->orders = btree_new(sizeof(struct order_node), 0, order_orderid, NULL);
        t->quantities = calloc(exchange->item_no, sizeof(int)); //need to set to 0
        t->prices = calloc(exchange->item_no, sizeof(long));
        t->disconnected = 0;
        t->current_order_id = 0;
    }
    //4
    exchange->rpoll_fd = epoll_create1(0);
    exchange->errpoll_fd = epoll_create1(0);
    exchange->err2poll_fd = epoll_create1(0);
    exchange->events = malloc((exchange->trader_num * 2 + 1) * sizeof(struct epoll_event));
    
    struct epoll_event e, e2;
    e.events = EPOLLERR | EPOLLHUP; //when read/write has closed
    e2.events = EPOLLIN | EPOLLHUP; //when read end closed
    
    for(int i = 0; i < trader_num; i++){
        char filename[50];
        //create fifo
        sprintf(filename, FIFO_EXCHANGE, i);
        unlink(filename);
        if (mkfifo(filename, S_IRWXU)) {
			perror("could not make fifo");
			DO_EXIT(1);
		}
        sprintf(filename, FIFO_TRADER, i);
        unlink(filename);
        if (mkfifo(filename, S_IRWXU)) {
        	perror("could not make fifo");
        	DO_EXIT(1);
        }
    }
    //5
    
    //create internal pipes for signal orders
    #if SANDBOX == 1
    struct epoll_event e3;
    e3.events = EPOLLIN;
    int pipefd[2];
    (void)!pipe2(pipefd, O_NONBLOCK | O_CLOEXEC);
    exchange->globl_read_fd = pipefd[0];
    e3.data.u32 = (uint32_t)-1;
    epoll_ctl(exchange->err2poll_fd, EPOLL_CTL_ADD, pipefd[0], &e3);
    //setup sigaction
    struct sigaction act = {0}, act2 = {0};
    sigset_t mask1, mask2;
    act.sa_handler = globl_usr1, act2.sa_handler = globl_usr2;
    act.sa_flags = SA_RESTART, act2.sa_flags = SA_RESTART;
    sigemptyset(&mask1), sigemptyset(&mask2);
    sigaddset(&mask1, SIGUSR2), sigaddset(&mask2, SIGUSR1);
    act.sa_mask = mask1, act2.sa_mask = mask2;
    #endif
    
    
    for (int i = 0; i < exchange->trader_num; i++){
        #ifdef PEX_TEST
        char filename[50];
        (void)!fgets(filename, 50, stdin);
        filename[strcspn(filename, "\n")] = 0;
        #endif
        pid_t pid = fork();
        if (pid == -1) {
        	perror("Could not create child");
        	DO_EXIT(1);
        }
        if (pid == 0) {
            #if SANDBOX == 1
            close(pipefd[0]); //only maintain write side
            //setup signal and auxilleries
            globl_write_fd = pipefd[1];
            globl_tid = i;
            sigaction(SIGUSR1, &act, NULL);
            sigaction(SIGUSR2, &act2, NULL);
            signal(SIGTERM, globl_term);
            //sandbox trader by forking again
            pid = fork();
            if (pid == -1) {
            	perror("Could not create child");
            	DO_EXIT(1);
            }
            if (pid == 0){
                char num[20];
                sprintf(num, "%d", i);
                //differentiate between actual and test traders!
                execl(argv[2 + i], argv[2 + i], num, (char*)NULL);
                perror("Could not swtich to trader");
                DO_EXIT(1);
            }
            globl_pid = pid;
            //idle except for passing signals, but otherwise we are good
            wait(NULL);

            exit(0); //I think exit here is okay due to child process??
            #else
            char num[20];
            sprintf(num, "%d", i);
            #ifndef PEX_TEST
            execl(argv[2 + i], argv[2 + i], num, (char*)NULL);
            #else
            execl(argv[2 + i], argv[2 + i], num, filename, (char*)NULL);
            #endif
            perror("Could not swtich to trader");
            DO_EXIT(1);
            #endif
        } else {
            e2.data.u32 = i;
            exchange->traders[i].pid = pid;
            char filename[50], filename2[50];
            //link to read side
            sprintf(filename, FIFO_TRADER, i);
            exchange->traders[i].comms[0] = open(filename, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (exchange->traders[i].comms[0] == -1) {
            	perror("could not open read side");
            	DO_EXIT(1);
            }
            
            //masquarade setup, file is guarenteed to be open on the read side so nonblocking is okay
            exchange->traders[i].fake_read = open(filename, O_WRONLY | O_CLOEXEC | O_NONBLOCK);
            
            //link to fifo, 1 is write side
            sprintf(filename2, FIFO_EXCHANGE, i);
            //start by opening in blocking mode, then change to non-blocking
            //non-blocking open fails if the other side isn't open
            int tmp = open(filename2, O_WRONLY | O_CLOEXEC);
            if (tmp == -1) {
            	perror("could not open write side");
            	DO_EXIT(1);
            }
            fcntl(tmp, F_SETFL, fcntl(tmp, F_GETFL) | O_NONBLOCK);
            exchange->traders[i].comms[1] = tmp;
            //link to epoll
            e.data.u32 = i; //normal is read end
            e2.data.u32 = i;
            epoll_ctl(exchange->errpoll_fd, EPOLL_CTL_ADD, exchange->traders[i].comms[0], &e);

            epoll_ctl(exchange->errpoll_fd, EPOLL_CTL_ADD, exchange->traders[i].comms[1], &e);
            epoll_ctl(exchange->rpoll_fd, EPOLL_CTL_ADD, exchange->traders[i].comms[0], &e2);
            
            epoll_ctl(exchange->err2poll_fd, EPOLL_CTL_ADD, exchange->traders[i].comms[0], &e);
            epoll_ctl(exchange->err2poll_fd, EPOLL_CTL_ADD, exchange->traders[i].comms[1], &e);
            
            printf("[PEX] Created FIFO %s\n[PEX] Created FIFO %s\n"
                   "[PEX] Starting trader %d (%s)\n"
                   "[PEX] Connected to %s\n[PEX] Connected to %s\n",
                   filename2, filename, i, argv[2 + i], filename2, filename);
        }
    }
    #if SANDBOX == 1
    close(pipefd[1]);
    #endif
    return 0;
}

static bool teardown_helper(const void *a, void *udata) {
    const struct order_node *o = a;
    free(o->order);
    return true;
}

void exc_teardown(struct exchange *exchange){
    //0
    regfree(&(exchange->trader_pattern));
    //1
    free(exchange->item_names);
    //2, but free the orders too
    for(int i = 0; i < exchange->item_no; i++){
        btree_ascend(exchange->sell_tree[i], NULL, teardown_helper, NULL);
        btree_ascend(exchange->buy_tree[i], NULL, teardown_helper, NULL);
        //btree_free(exchange->sell_tree_all[i]);
        btree_free(exchange->sell_tree[i]);
        //btree_free(exchange->buy_tree_all[i]);
        btree_free(exchange->buy_tree[i]);
    }
    //free(exchange->sell_tree_all);
    free(exchange->sell_tree);
    //free(exchange->buy_tree_all);
    free(exchange->buy_tree);
    //4, open fds in parent process
    close(exchange->rpoll_fd);
    close(exchange->errpoll_fd);
    close(exchange->err2poll_fd);
    free(exchange->events);
    for(int i = 0; i < exchange->trader_num; i++){
        char filename[50];
        sprintf(filename, FIFO_EXCHANGE, i);
        unlink(filename);
        sprintf(filename, FIFO_TRADER, i);
        unlink(filename);
        close(exchange->traders[i].comms[0]);
        close(exchange->traders[i].comms[1]);
        //masquarade
        close(exchange->traders[i].fake_read);
    }
    //3
    for(int i = 0; i < exchange->trader_num; i++){
        struct trader *t = exchange->traders + i;
        kill(t->pid, SIGTERM);
        btree_free(t->orders);
        free(t->quantities);
        free(t->prices);
    }
    free(exchange->traders);
    close(globl_write_fd);

    close(exchange->globl_read_fd);
}

int disconnect_trader(struct exchange *ex, int id){
    if (id >= ex->trader_num) return -1;
    struct trader *t = ex->traders + id;
    if (t->disconnected == 1) return -1;
    //kill(t->pid, SIGKILL);
    //remove from epoll watchlist
    epoll_ctl(ex->errpoll_fd, EPOLL_CTL_DEL, t->comms[0], (void*)&id);
    epoll_ctl(ex->errpoll_fd, EPOLL_CTL_DEL, t->comms[1], (void*)&id);

    epoll_ctl(ex->err2poll_fd, EPOLL_CTL_DEL, t->comms[0], (void*)&id);
    epoll_ctl(ex->err2poll_fd, EPOLL_CTL_DEL, t->comms[1], (void*)&id);

    epoll_ctl(ex->rpoll_fd, EPOLL_CTL_DEL, t->comms[0], (void*)&id);
    //close fds
    close(t->comms[0]);
    close(t->comms[1]);
    
    ex->traders_active--;
    t->disconnected = 1;
    char filename[40];
    sprintf(filename, FIFO_EXCHANGE, id);
    unlink(filename);
    sprintf(filename, FIFO_TRADER, id);
    unlink(filename);
    
    printf("[PEX] Trader %d disconnected\n", id);
    return ex->traders_active;
}

int msg_trader(struct exchange *ex, int id, char *msg, int maxlen, int mode){
    //use comms[1] due to write
    struct trader *t;
    #if SANDBOX == 1
    int signum = SIGUSR2;
    #else
    int signum = SIGUSR1;
    #endif
    
    if (mode == 0){
        t = ex->traders + id;
        if (t->disconnected) return -1;
        (void)!write(t->comms[1], msg, strnlen(msg, maxlen));
        kill(t->pid, signum);
    } else { //notify all except
        for (int i = 0; i < ex->trader_num; i++) if (i != id) {
            t = ex->traders + i;
            if (t->disconnected) continue;
            (void)!write(t->comms[1], msg, strnlen(msg, maxlen));
        }
        for (int i = 0; i < ex->trader_num; i++) if (i != id) {
            t = ex->traders + i;
            if (t->disconnected) continue;
            kill(t->pid, signum);
        }
    }
    return 0;
}

void msg_trader2(struct exchange *ex, int id, char *msg_id, char *msg_oth, int maxlen){
    #if SANDBOX == 1
    int signum = SIGUSR2;
    #else
    int signum = SIGUSR1;
    #endif
    for (int i = 0; i < ex->trader_num; i++){
        struct trader *t = ex->traders + id;
        if (t->disconnected) continue;
        char *msg = (i == id) ? msg_id : msg_oth;
        (void)!write(t->comms[1], msg, strnlen(msg, maxlen));
        kill(t->pid, signum);
    }
    return;
}

int read_fd(int fd, int id, char *buf){
    for (int i = 0; i < MAX_BUF - 1; i++){
        int status = read(fd, buf, 1);
        //if (status == -1) perror("read failed"), exit(1);
        if (status <= 0) {
            if (i == 0) return 0;
            printf(">\n");
            return -1;
        }
        if (i == 0) printf("[PEX] [T%d] Parsing command: <", id);
        if (*(buf++) == ';') {
            *buf = '\0';
            printf(">\n");
            return i;
        }
        putchar(*(buf-1));
    }
    while(1){
        if (read(fd, buf, 1) <= 0) break;
        if (*buf == ';') break;
        putchar(*buf);
    }
    printf(">\n");
    return -1;
}

int rec_trader(struct exchange *ex, int id, char *buf){
    int fd = ex->traders[id].comms[0];
    #if 1 == 0
    for (int i = 0; i < MAX_BUF - 1; i++){
        if (!read(fd, buf, 1)) return -1;
        if (*(buf++) == ';') {
            *buf = '\0';
            return 0;
        }
        putchar(*(buf-1));
    }
    while(1){
        if (!read(fd, buf, 1)) break;
        if (*buf == ';') break;
        putchar(*buf);
    }
    #else
    /*
    for (int i = 0; i < MAX_BUF - 1; i++){
        int status = read(fd, buf, 1);
        //if (status == -1) perror("read failed"), exit(1);
        if (status <= 0) {
            if (i == 0) return 0;
            printf(">\n");
            return -1;
        }
        if (i == 0) printf("[PEX] [T%d] Parsing command: <", id);
        if (*(buf++) == ';') {
            *buf = '\0';
            printf(">\n");
            return i;
        }
        putchar(*(buf-1));
    }
    while(1){
        if (read(fd, buf, 1) <= 0) break;
        if (*buf == ';') break;
        putchar(*buf);
    }
    printf(">\n");
    */
    return read_fd(fd, id, buf);
    #endif
    return -1;
}


//user tree
int order_orderid(const void *a, const void *b, void *udata) {
    const struct order_node *oa = a, *ob = b;
    int i1 = oa->order->local_id, i2 = ob->order->local_id;
    int cmp = (i1 > i2) - (i1 < i2);
    return cmp;
}

//order tree (sell)
int order_price_min(const void *a, const void *b, void *udata) {
    const struct order_node *oa = a, *ob = b;
    int i1 = oa->order->price, i2 = ob->order->price; //sort by price
    int cmp = (i1 > i2) - (i1 < i2);
    if (cmp == 0) {
        i1 = oa->order->global_id, i2 = ob->order->global_id; //sort by time, earlist first
        //this means if i1 is later than i2 (i1 > i2 == 1) this will place i1 after i2
        cmp = (i1 > i2) - (i1 < i2);
        //cmp = (i1 < i2) - (i1 > i2);
    }
    return cmp;
}
//order tree (buy)
int order_price_max(const void *a, const void *b, void *udata) {
    const struct order_node *oa = a, *ob = b;
    int i1 = oa->order->price, i2 = ob->order->price;
    int cmp = (i1 < i2) - (i1 > i2);
    if (cmp == 0) {
        i1 = oa->order->global_id, i2 = ob->order->global_id;
        cmp = (i1 > i2) - (i1 < i2);
        //cmp = (i1 < i2) - (i1 > i2);
    }
    return cmp;
}
