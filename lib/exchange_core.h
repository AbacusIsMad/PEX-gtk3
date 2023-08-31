#ifndef PE_EXC_CORE_H
#define PE_EXC_CORE_H

#include "pe_common.h"
#include "btree.h"
#include <regex.h>

//for GUI
#include <gtk/gtk.h>
#include <cairo.h>
#include <glib/gstdio.h>
#include <sys/mman.h>
#include <pthread.h>
#include <errno.h>

enum order_type {BUY, SELL, AMEND, CANCELLED, DONE};

struct order {
    enum order_type type;
    enum order_type status;
    int owner;
    int product;
    int quantity;
    int price;
    long global_id;
    int local_id;
};
//wrapper
struct order_node {
    struct order* order;
};

int order_orderid(const void *a, const void *b, void *udata);
int order_price_min(const void *a, const void *b, void *udata);
int order_price_max(const void *a, const void *b, void *udata);

struct trader{
    pid_t pid;
    struct btree *orders;
    int comms[2];
    int *quantities;
    long *prices;
    char disconnected;
    int current_order_id;
    //newly added - to masquarade as trader
    int fake_read;
};

#define PRODUCT_LINE (20)
struct exchange {
    int trader_num;
    int traders_active;
    struct trader *traders; //number of traders
    regex_t trader_pattern;
    int rpoll_fd;
    int errpoll_fd;
    int err2poll_fd;
    struct epoll_event *events;
    int item_no;
    char (*item_names)[PRODUCT_LINE]; //number of items. Same goes with the trees too.
    //struct btree* *sell_tree_all;
    struct btree* *sell_tree;
    //struct btree* *buy_tree_all;
    struct btree* *buy_tree;
    long global_order_id;
    long fees;
    int globl_read_fd;
};

int exc_init(struct exchange *exchange, char* product_file, int trader_num, char **argv);
void exc_teardown(struct exchange *exchange);

int disconnect_trader(struct exchange *ex, int id);
int msg_trader(struct exchange *ex, int id, char *msg, int maxlen, int mode);
void msg_trader2(struct exchange *ex, int id, char *msg_id, char *msg_oth, int maxlen);
int rec_trader(struct exchange *ex, int id, char *buf);

//global variables
struct exchange_arg_helper {
	int argc;
	char **argv;
	struct exchange *ex;
	int trigger_pipe;
	int msg_pipe;
	FILE *msg_fp;
	pthread_mutex_t *mutex;
	pthread_cond_t *init_ready;
	bool ready_flag;
};
struct exchange_arg_helper exchange_arg;

struct gui_helper {
	struct {
		GtkScrolledWindow *scroll;
		GtkTextBuffer *content;
	} text_log;
	struct {
		GtkWidget *graph;
		cairo_surface_t *surface;
		cairo_t *context;
		int width;
		int height;
	} graph;
	struct {
		GtkSpinButton *trader_sel;
		GtkComboBox *action_sel;
		GtkComboBox *order_sel;
		GtkComboBox *product_sel;
		GtkSpinButton *qty_sel;
		GtkSpinButton *price_sel;
		GtkLabel *preview;
		GtkLabel *status;
		GtkListStore *static_product;
		char send_msg[60];
		bool valid_msg;
	} sender;
};
struct gui_helper gui_arg;

#endif
