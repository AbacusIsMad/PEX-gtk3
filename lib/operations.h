#ifndef PE_OPS_H
#define PE_OPS_H

#include "exchange_core.h"

struct order_node *get_order_by_user(struct exchange *ex, int tid, int order_id);


int process_cmd(struct exchange *ex, int tid, char *buf);
struct order *make_new_order(struct exchange *ex, int is_buy, int tid, int product, int qty, int price, int order_id);
struct order *amend_order(struct exchange *ex, int tid, int order_id, int qty, int price);
struct order *cancel_order(struct exchange *ex, int tid, int order_id);
void match_orders(struct exchange *ex, int product, struct order *new_added);
void print_orderbook(struct exchange *ex);

#endif
