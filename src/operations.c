#include "operations.h"

int process_cmd(struct exchange *ex, int tid, char *buf){
    struct trader *t = ex->traders + tid;
    //command guarenteed to be valid, so we're good.
    char* type = strtok(buf, " ");
    int order_id = (int)strtol(strtok(NULL, " "), NULL, 0);
    int product = -1;
    int qty, price;
    char cmd_buf[50];
    //char cmd_buf2[50];
    if ((strncmp(type, "BUY", 3) == 0) || (strncmp(type, "SELL", 4) == 0)){
        int is_buy = (strncmp(type, "BUY", 3) == 0);
        //check id. Must be new one due to new order.
        if (order_id != t->current_order_id){
            msg_trader(ex, tid, "INVALID;", 40, 0);
            return -1;
        }

        //find product
        char *pd = strtok(NULL, " ");
        for (int i = 0; i < ex->item_no; i++){
            if (strcmp(pd, ex->item_names[i]) == 0){
                product = i;
                break;
            }
        }
        if (product == -1){ //didn't find it
            msg_trader(ex, tid, "INVALID;", 40, 0);
            return -1;
        }
        //at this point, everything must be valid, so we can create the order.
        t->current_order_id++;
        qty = (int)strtol(strtok(NULL, " "), NULL, 0);
        price = (int)strtol(strtok(NULL, " "), NULL, 0);
        struct order *o = make_new_order(ex, is_buy, tid, product, qty, price, order_id);
        //notify user(s)
        sprintf(cmd_buf, "ACCEPTED %d;", order_id);
        msg_trader(ex, tid, cmd_buf, 50, 0);
        sprintf(cmd_buf, "MARKET %s %s %d %d;", is_buy ? "BUY" : "SELL", pd, qty, price);
        msg_trader(ex, tid, cmd_buf, 50, -1);
        //do matching
        match_orders(ex, product, o);
        //print orderbook
        print_orderbook(ex);
    } else if (strncmp(type, "AMEND", 5) == 0){
        //product not there
        qty = (int)strtol(strtok(NULL, " "), NULL, 0);
        price = (int)strtol(strtok(NULL, " "), NULL, 0);
        struct order *o = amend_order(ex, tid, order_id, qty, price);
        if (o == NULL){
            msg_trader(ex, tid, "INVALID;", 40, 0);
            return -1;
        }
        //notify users
        sprintf(cmd_buf, "AMENDED %d;", order_id);
        msg_trader(ex, tid, cmd_buf, 50, 0);
        sprintf(cmd_buf, "MARKET %s %s %d %d;",
            o->type == BUY ? "BUY" : "SELL", ex->item_names[o->product], qty, price);
        msg_trader(ex, tid, cmd_buf, 50, 1);
        //do orders
        match_orders(ex, o->product, o);
        print_orderbook(ex);
    } else if (strncmp(type, "CANCEL", 6) == 0){
        //variables are already there.
        struct order *o = cancel_order(ex, tid, order_id);
        if (o == NULL){
            msg_trader(ex, tid, "INVALID;", 40, 0);
            return -1;
        }
        //acknoledged. Notify users, then free memory! (important)
        sprintf(cmd_buf, "CANCELLED %d;", order_id);
        msg_trader(ex, tid, cmd_buf, 50, 0);
        sprintf(cmd_buf, "MARKET %s %s %d %d;",
            o->type == BUY ? "BUY" : "SELL", ex->item_names[o->product], 0, 0);
        msg_trader(ex, tid, cmd_buf, 50, 1);

        free(o);
        print_orderbook(ex);
    }

	//new thing: returns the product ID so gtk can update it correctly
    return product;
}

/**
* creates a new order and stores it in the order and user trees.
* don't worry, orders are freed either when they are filled or at teardown.
*/
struct order *make_new_order(struct exchange *ex, int is_buy, int tid, int product, int qty, int price, int order_id){
    struct order_node m = {.order = malloc(sizeof(struct order))};
    m.order->type = is_buy ? BUY : SELL;
    m.order->status = BUY; //not amend cancel done
    m.order->owner = tid;
    m.order->product = product;
    m.order->quantity = qty;
    m.order->price = price;
    m.order->local_id = order_id;
    m.order->global_id = ex->global_order_id;
    (ex->global_order_id)++;
    //register in buy/sell tree
    btree_set(is_buy ? ex->buy_tree[product] : ex->sell_tree[product], &m);
    //register in user tree
    btree_set(ex->traders[tid].orders, &m);
    //we are done.
    return m.order;
}

struct order_node *get_order_by_user(struct exchange *ex, int tid, int order_id){
    struct trader *t = ex->traders + tid;
    struct order ot = {.local_id = order_id};
    struct order_node p = {.order = &ot};
    struct order_node *o = btree_get(t->orders, &p);
    return o;
}

struct order *amend_order(struct exchange *ex, int tid, int order_id, int qty, int price){
    struct order_node *o = get_order_by_user(ex, tid, order_id);
    if (o == NULL) return NULL;
    /** order is valid. Amend must be successful.
    * 1: remove from order tree (don't need to remove from user tree)
    * 2: modify details, including time
    * 3: add to order tree
    */
    //1
    int product = o->order->product;
    struct btree *b = o->order->type == BUY ? ex->buy_tree[product] : ex->sell_tree[product];
    //2
    //if (o->order->quantity == qty && o->order->price == price) return NULL;
    btree_delete(b, o);
    o->order->quantity = qty;
    o->order->price = price;
    o->order->global_id = ex->global_order_id;
    (ex->global_order_id)++;
    //3
    btree_set(b, o);
    return o->order;
}

struct order *cancel_order(struct exchange *ex, int tid, int order_id){
    struct order_node *o = get_order_by_user(ex, tid, order_id);
    struct trader *t = ex->traders + tid;
    if (o == NULL) return NULL;
    //remove from both trees
    int product = o->order->product;
    struct btree *b = o->order->type == BUY ? ex->buy_tree[product] : ex->sell_tree[product];
    struct order *ret = o->order;
    btree_delete(b, o);
    btree_delete(t->orders, o);
    return ret;
}



/**
* 1: acquire min of sell and max of buy
* 2: is buy bigger than sell? if not we are done
* 3: carry out trade
* 3.1: find out mininum quantity between orders. Subtract both
* 3.2: generate prices and transaction fees (compare global id) (value is the value bought)
* 3.3: print out match information
* 3.4: update traders information, including sending messages
* 3.5: remove filled orders (remove from order tree, user tree, then free)
* 4: return to step 1
*/
void match_orders(struct exchange *ex, int product, struct order *new_added){
    struct order_node *min_sell, *max_buy;
    struct btree *buy = ex->buy_tree[product], *sell = ex->sell_tree[product];
    while (1){
        //1
        min_sell = btree_min(sell), max_buy = btree_min(buy); //max here same as min btw
        if (min_sell == NULL || max_buy == NULL) break; //one/both is empty
        //2
        struct order *s = min_sell->order, *b = max_buy->order;
        if (s->price > b->price) break; //price too huge
        //3.1
        int min_qty = s->quantity < b->quantity ? s->quantity : b->quantity;
        s->quantity -= min_qty, b->quantity -= min_qty;
        //3.2 matching price is price of older order!!!
        long value = (long)min_qty * (long)(b == new_added ? s->price : b->price);
        long s_trans_fee = value / 100;
        s_trans_fee += (value % 100) >= 50; //custom round lmao
        long b_trans_fee = 0; //who gets the fees
        if (s->global_id < b->global_id) b_trans_fee = s_trans_fee, s_trans_fee = 0;
        int s_tid = s->owner, b_tid = b->owner;
        //3.3 this should have old in the first and new in the second
        printf("[PEX] Match: Order ");
        if (b == new_added)
            printf("%d [T%d], New Order %d [T%d], ", s->local_id, s_tid, b->local_id, b_tid);
        else
            printf("%d [T%d], New Order %d [T%d], ", b->local_id, b_tid, s->local_id, s_tid);
        printf("value: $%ld, fee: $%ld.\n", value, s_trans_fee + b_trans_fee);
        //3.4
        struct trader *ts = ex->traders + s_tid, *tb = ex->traders + b_tid;
        //seller increases in cash but decreases in quantity
        ts->quantities[product] -= min_qty; 
        ts->prices[product] += value - s_trans_fee;
        //buyer decreases cash, increases quantity
        tb->quantities[product] += min_qty;
        tb->prices[product] -= value + b_trans_fee;
        //add fee to exchange too!
        ex->fees += s_trans_fee + b_trans_fee;
        //message them
        char buf[20];
        sprintf(buf, "FILL %d %d;", b->local_id, min_qty);
        msg_trader(ex, b_tid, buf, 20, 0);
        sprintf(buf, "FILL %d %d;", s->local_id, min_qty);
        msg_trader(ex, s_tid, buf, 20, 0);
        //3.5
        if (s->quantity <= 0) {
            struct order *o = min_sell->order;
            struct order_node p = {.order = o};
            btree_delete(sell, &p);
            btree_delete(ts->orders, &p);
            free(o);
        }
        if (b->quantity <= 0) {
            struct order *o = max_buy->order;
            struct order_node p = {.order = o};
            btree_delete(buy, &p);
            btree_delete(tb->orders, &p);
            free(o);
        }
    }
}


struct iter_helper{
    struct order_node *orders;
    int price;
    int levels;
    int count;
};

static bool order_iter(const void *a, void *udata) {
    const struct order_node *o = a;
    struct iter_helper *h = udata;
    //store for later processing
    h->orders[h->count] = *o;
    h->count++;
    if (o->order->price != h->price){
        h->price = o->order->price;
        h->levels++;
    }
    return true;
}

static void print_orderbook_lines(struct iter_helper *helper, int num_count, int is_buy){
    int price = -1, quantity = 0, count = 0;
    if (num_count == 0) return;
    for (int i = 0; i < num_count; i++){
        struct order_node o = helper->orders[i];
        if (o.order->price != price){
            //print out
            if (price != -1) printf("[PEX]\t\t%s %d @ $%d (%d order%s\n",
               is_buy ? "BUY" : "SELL", quantity, price, count, count > 1 ? "s)" : ")");
            quantity = o.order->quantity;
            price = o.order->price;
            count = 1;
        } else {
            quantity += o.order->quantity;
            count++;
        }
    } printf("[PEX]\t\t%s %d @ $%d (%d order%s\n",
        is_buy ? "BUY" : "SELL", quantity, price, count, count > 1 ? "s)" : ")");
}

void print_orderbook(struct exchange *ex){
    puts("[PEX]\t--ORDERBOOK--");
    //buy and sell orders both have largest first and smallest last, so be careful!
    for (int i = 0; i < ex->item_no; i++){
        int buy_count = btree_count(ex->buy_tree[i]), sell_count = btree_count(ex->sell_tree[i]);
        struct iter_helper helper_s = {NULL, -1, 0, 0};
        struct iter_helper helper_b = {NULL, -1, 0, 0};
        //sell first
        if (sell_count != 0){
            helper_s.orders = malloc(sell_count * sizeof(struct order_node));
            btree_descend(ex->sell_tree[i], NULL, order_iter, &helper_s);
        }
        if (buy_count != 0){
            helper_b.orders = malloc(buy_count * sizeof(struct order_node));
            btree_ascend(ex->buy_tree[i], NULL, order_iter, &helper_b);
        }
        printf("[PEX]\tProduct: %s; Buy levels: %d; Sell levels: %d\n",
            ex->item_names[i], helper_b.levels, helper_s.levels);
        print_orderbook_lines(&helper_s, sell_count, 0);
        print_orderbook_lines(&helper_b, buy_count, 1);

        free(helper_s.orders), free(helper_b.orders);
    }
    //now the positions
    puts("[PEX]\t--POSITIONS--");
    for (int i = 0; i < ex->trader_num; i++){
        printf("[PEX]\tTrader %d:", i);
        struct trader *t = ex->traders + i;
        for (int j = 0; j < ex->item_no; j++){
            if (j != 0) putchar(',');
            printf(" %s %d ($%ld)", ex->item_names[j], t->quantities[j], t->prices[j]);
        }
        putchar('\n');
    }
}







