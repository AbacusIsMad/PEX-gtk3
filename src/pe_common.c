#include "pe_common.h"

//nonblocking read, for errors lol
int read_msg(int fd, char* buf){
    for (int i = 0; i < MAX_BUF - 1; i++){
        if (read(fd, buf, 1) <= 0) {
            if (i == 0) return 0;
            return -1;
        }
        if (*(buf++) == ';') {
            *buf = '\0';
            return i;
        }
    }
    while(1){
        if (read(fd, buf, 1) <= 0) break;
        if (*buf == ';') break;
    }
    return -1;
}
