#include "pe_exchange.h"

extern struct exchange_arg_helper exchange_arg;

volatile sig_atomic_t received, child_exit;
int globl_fdpair[2];

static void usr1_received(int sig, siginfo_t *info, void *ucontext){
    (void)!write(globl_fdpair[1], &(info->si_pid), sizeof(pid_t));
    received = 1;
}

static void chld_received(int sig, siginfo_t *info, void *ucontext){
    child_exit = 1;
}

//int main(int argc, char ** argv) {
int ex_main(int argc, char ** argv, struct exchange *ex, struct exchange_arg_helper *ex_arg){
    if (argc < 3){
        printf("Not enough arguments\n");
        return 1;
    }
    received = 0, child_exit = 0;

    
    
    //struct exchange exchange;
    //struct exchange *ex = &exchange;
    pthread_mutex_lock(ex_arg->mutex); //wait for exchange structure to be setup before reading on gui thread
    int status = exc_init(ex, argv[1], argc - 2, argv);
    ex_arg->ready_flag = true;
    pthread_cond_signal(ex_arg->init_ready);
    pthread_mutex_unlock(ex_arg->mutex);
    if (status != 0) return status; //error happened
    #if SANDBOX == 0
    //setup pipe and fd
    struct epoll_event e3;
    e3.events = EPOLLIN;
    (void)!pipe2(globl_fdpair, O_NONBLOCK | O_CLOEXEC);
    e3.data.u32 = (uint32_t)-1;
    epoll_ctl(ex->err2poll_fd, EPOLL_CTL_ADD, globl_fdpair[0], &e3);
    #endif

    struct sigaction act = {0};
    act.sa_sigaction = usr1_received;
    act.sa_flags = SA_RESTART | SA_SIGINFO;
    sigemptyset(&(act.sa_mask));
    sigaddset(&(act.sa_mask), SIGCHLD);
    sigaction(SIGUSR1, &act, NULL);
    act.sa_sigaction = chld_received;
    sigemptyset(&(act.sa_mask));
    sigaddset(&(act.sa_mask), SIGUSR1);
    sigaction(SIGCHLD, &act, NULL);
    signal(SIGPIPE, SIG_IGN);


    msg_trader(ex, -1, "MARKET OPEN;", 20, 1);
    
    //usleep(4000);
    //msg_trader(ex, -1, "MARKET SELL Basil 100 100;", 40, 1);
    
    /**
    * the strat:
    * 1: wait for signal
    * 2: mask (for both USR1 and CHLD)
    * 3: poll (do I want to just read one? or more), goto 5 if none
    * 4: process, return to step 3 (remember to check for pipes closing!)
    * 5: process dead/disconnected children
    * 6: unmask
    */
    /**
    * new strat:
    * 1: wait for changes (both in errors and the main fd)
    * 1.1: process errors/disconnects
    * 2: get avaliable if any
    * 3: process and read the main fd
    * 4: handle dead children
    */

    int quit = 0;
    received = 0, child_exit = 0;
    int num_triggered = 0;
    
    while(!quit){
        if (child_exit == 0) {
            //1
            if ((num_triggered = epoll_wait(ex->err2poll_fd, ex->events, ex->trader_num * 2 + 1, -1)) >= 0){
                for (int i = 0; i < num_triggered; i++){
                    //1.1
                    //newly added: repeat message to msg_pipe so it displays on the GUI
                    char repeat_buf[MAX_BUF_GUI];
                    int trigger_status = -1; //meaning message only
                    
                    if (ex->events[i].data.u32 != (uint32_t)-1){
                        if (ex->events[i].events & (EPOLLERR | EPOLLHUP)){
                        	int tid = ex->events[i].data.u32;
                        	if (ex->traders[tid].disconnected) continue;

                        	sprintf(repeat_buf, "%d -> PEX: disconnect\n", tid);
                        	trigger_status = -2;
                        	//writing is done below
                            #ifndef PEX_TEST
                            if (disconnect_trader(ex, tid) == 0){
                            #else 
                            if (disconnect_trader(ex, tid) == 1){
                            #endif
                                quit = 1;
                    			(void)!write(ex_arg->msg_pipe, repeat_buf, strnlen(repeat_buf, MAX_BUF_GUI));
                                (void)!write(ex_arg->trigger_pipe, &trigger_status, sizeof(int));
                                break;
                            }
                        }
                    } else if ((ex->events[i].events & POLLIN)) { //2
                        
                        int tid = -1;
                        #if SANDBOX == 1
                        (void)!read(ex->globl_read_fd, &tid, sizeof(int));
                        #else
                        pid_t tmp_pid;
                        (void)!read(globl_fdpair[0], &tmp_pid, sizeof(pid_t));
                        for (int j = 0; j < ex->trader_num; j++){
                            if (ex->traders[j].pid == tmp_pid){
                                tid = j;
                                break;
                            }
                        }
                        if (tid == -1) continue; //not found
                        #endif
                        
                        if (ex->traders[tid].disconnected) continue; //don't
                        char buf[50];
                        //3
                        int status, count = 0;

                        status = rec_trader(ex, tid, buf);
                        
                        if (status == -1){
                            msg_trader(ex, tid, "INVALID;", 40, 0);
                            sprintf(repeat_buf, "%d -> PEX: [malformed message]\n", tid); 
                            //continue;
                            goto finish_read_trader;
                        }
                        if (status == 0){
                            if (count == 0) msg_trader(ex, tid, "INVALID;", 40, 0);
                            sprintf(repeat_buf, "%d -> PEX: [no message]\n", tid); 
                            //continue;
                            goto finish_read_trader;
                        }
                        count++;
                        sprintf(repeat_buf, "%d -> PEX: [%s]\n", tid, buf); 
                        //invalid regex
                        if (regexec(&(ex->trader_pattern), buf, 0, NULL, 0)){
                            msg_trader(ex, tid, "INVALID;", 40, 0);
                            //continue;
                            goto finish_read_trader;
                        }
                        
                        pthread_mutex_lock(ex_arg->mutex); //it can receive signals while locked
                        trigger_status = process_cmd(ex, tid, buf);
						pthread_mutex_unlock(ex_arg->mutex);
                        
                    }
                    finish_read_trader:
                    //write the message
                    (void)!write(ex_arg->msg_pipe, repeat_buf, strnlen(repeat_buf, MAX_BUF_GUI));
                    (void)!write(ex_arg->trigger_pipe, &trigger_status, sizeof(int));
                    ex->events[i].events = 0;
                }
                if (quit) break;
            }
        }
        //4
        if (child_exit){
            child_exit = 0;
            int stat;
            pid_t pid;
            while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
                //find trader id
                int tid = -1;
                for (int i = 0; i < ex->trader_num; i++){
                    if (pid == ex->traders[i].pid) tid = i;
                }
                if (tid == -1) continue;
                //disconnect, gotta send the message here again
                char buf[30];
                int trigger_status = -2;
                if (ex->traders[tid].disconnected) continue;
                sprintf(buf, "%d -> PEX: disconnect\n", tid);
                (void)!write(ex_arg->msg_pipe, buf, strnlen(buf, MAX_BUF_GUI));
                #ifndef PEX_TEST
                if (disconnect_trader(ex, tid) == 0){
                #else 
                if (disconnect_trader(ex, tid) == 1){
                #endif
                    quit = 1;
                    (void)!write(ex_arg->trigger_pipe, &trigger_status, sizeof(int));
                    break;
                }
                (void)!write(ex_arg->trigger_pipe, &trigger_status, sizeof(int));
            }
        }
    }

    printf("[PEX] Trading completed\n[PEX] Exchange fees collected: $%ld\n", ex->fees);
    //exc_teardown(ex);
    return 0;
}
