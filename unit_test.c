#include "exchange_core.c"

#include <stdarg.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include "cmocka.h"

struct btree *__wrap_btree_new(size_t elsize, size_t max_items,
    int (*compare)(const void *a, const void *b, void *udata), void *udata){return NULL;}

int t_pipefd[2];

static int setup(void **state) {
     int *answer = malloc(sizeof(int));
     if (answer == NULL) {
         return -1;
     }
     *answer = 42;
 
     *state = answer;
 
     return 0;
}

struct read_helper {
    int pipefd[2];
};

static int read_setup(void **state) {
    pipe2(t_pipefd, O_NONBLOCK);
    return 0;
}

static int read_teardown(void **state) {
    close(t_pipefd[0]), close(t_pipefd[1]);
    return 0;
}

static void n_read0(void **state){
    char *msg = "BUY 0 skill 1 2;";
    write(t_pipefd[1], msg, strlen(msg));
    char mgs[MAX_BUF];
    read_fd(t_pipefd[0], 0, mgs);
    struct read_helper *r = *state;
    assert_string_equal(msg, mgs);
}

static void n_read1(void **state){
    char *msg = "SELL 100000 aaaaaaaaaaaaaaaa 999999 999999;";
    write(t_pipefd[1], msg, strlen(msg));
    char mgs[MAX_BUF];
    read_fd(t_pipefd[0], 0, mgs);
    struct read_helper *r = *state;
    assert_string_equal(msg, mgs);
}

static void n_read2(void **state){
    char *msg = "BUY alskdjahfslkjchdslakjthldkfjhdsalkjhdslkfjhsadlkjhdslkjfhlad";
    write(t_pipefd[1], msg, strlen(msg));
    char mgs[MAX_BUF];
    int status = read_fd(t_pipefd[0], 0, mgs);
    struct read_helper *r = *state;
    assert_int_equal(status, -1);
}

static void n_read3(void **state){
    char *msg = "B;";
    write(t_pipefd[1], msg, strlen(msg));
    char mgs[MAX_BUF];
    read_fd(t_pipefd[0], 0, mgs);
    struct read_helper *r = *state;
    assert_string_equal(msg, mgs);
}

static void n_read4(void **state){
    char *msg = "SELL -1 2 -3 -5";
    write(t_pipefd[1], msg, strlen(msg));
    char mgs[MAX_BUF];
    read_fd(t_pipefd[0], 0, mgs);
    struct read_helper *r = *state;
    assert_string_equal(msg, mgs);
}

static void n_read5(void **state){
    char mgs[MAX_BUF];
    int status = read_fd(t_pipefd[0], 0, mgs);
    struct read_helper *r = *state;
    assert_int_equal(status, 0);
}

 
int main(void) {
    
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(n_read0, read_setup, read_teardown),
        cmocka_unit_test_setup_teardown(n_read1, read_setup, read_teardown),
        cmocka_unit_test_setup_teardown(n_read2, read_setup, read_teardown),
        cmocka_unit_test_setup_teardown(n_read3, read_setup, read_teardown),
        cmocka_unit_test_setup_teardown(n_read4, read_setup, read_teardown),
        cmocka_unit_test_setup_teardown(n_read5, read_setup, read_teardown),
    };
 
    return cmocka_run_group_tests(tests, NULL, NULL);
}
