#include "interface_ops.h"

extern struct exchange_arg_helper exchange_arg;
extern struct gui_helper gui_arg;


int update_text_log(FILE *msg_fp, GtkScrolledWindow *text_log_scroll, GtkTextBuffer *text_log_content){
	char buf[MAX_BUF_GUI];
	if (fgets(buf, MAX_BUF_GUI, msg_fp) == NULL) return -1; //just don't use the input
	//get scroll position before inserting
	
	GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(text_log_scroll);
	gdouble pos = gtk_adjustment_get_upper(adj) - gtk_adjustment_get_page_size(adj);
	gdouble cur = gtk_adjustment_get_value(adj);		
	bool isNearBottom = (pos - gtk_adjustment_get_value(adj) <= 50);
	
	GtkTextBuffer *tlc = text_log_content;
	GtkTextIter end;
	gtk_text_buffer_get_iter_at_offset (tlc, &end, -1);
	gtk_text_buffer_insert (tlc, &end, buf, -1);
	//automatically scroll to the bottom if already there
	if (cur == 0 || isNearBottom) {
		pos = gtk_adjustment_get_upper(adj) - gtk_adjustment_get_page_size(adj);
		gtk_adjustment_set_value(adj, pos);
	}
	
	return 0;
}

static bool traverse_order_tree(const void *item, void *udata){
	//get order id as string
	char num_buf[16];
	const struct order_node *o = item;
	sprintf(num_buf, "%d", o->order->local_id);
	//add the item to the listStore
	GtkTreeIter iter;
	GtkListStore *l = GTK_LIST_STORE(udata);
	gtk_list_store_append(l, &iter);
	gtk_list_store_set(l, &iter, 0, num_buf, -1);
	return true;
}

void populate_sender(bool exchange_changed){
	//figure out which ones have changed, and reget attributes for that if necessary.
	struct sel {
		int trader;
		char action[7];
		int order;
		int product;
		int quantity;
		int price;
	};
	static struct sel prev = {0, {0}, -1, 0, 1, 1};
	struct sel cur = {0, {0}, -1, 0, 1, 1};
	GtkTreeIter i;
	//trader
	cur.trader = gtk_spin_button_get_value_as_int(gui_arg.sender.trader_sel);
	bool trader_changed = cur.trader != prev.trader;
	//after comparison we store it
	prev.trader = cur.trader;
	//action
	GtkTreeModel *t = gtk_combo_box_get_model(gui_arg.sender.action_sel);
	gtk_combo_box_get_active_iter(gui_arg.sender.action_sel, &i);
	gchararray dest;
	gtk_tree_model_get(t, &i, 0, &(dest), -1);
	bool action_changed = strcmp(prev.action, dest) != 0;
	strcpy(prev.action, dest);
	//order
	bool order_changed = false; //a little more complicated
	
	if (exchange_changed || trader_changed || action_changed){
		order_changed = true;
		//fetch orders from exchange
		GtkListStore *list_store = gtk_list_store_new(1, G_TYPE_STRING);
		//amend and cancel requires all active orders
		if (!strcmp(prev.action, "AMEND") || !strcmp(prev.action, "CANCEL")){
			pthread_mutex_lock(exchange_arg.mutex);
			btree_ascend( exchange_arg.ex->traders[cur.trader].orders, NULL, traverse_order_tree, list_store);
			pthread_mutex_unlock(exchange_arg.mutex);
		} else { //otherwise just the latest number is enough.
			pthread_mutex_lock(exchange_arg.mutex);
			char buf[16];
			sprintf(buf, "%d", exchange_arg.ex->traders[cur.trader].current_order_id);
			pthread_mutex_unlock(exchange_arg.mutex);
			gtk_list_store_append(list_store, &i);
			gtk_list_store_set(list_store, &i, 0, buf, -1);
		}
		//disable dropdown by setting model to NULL if empty
		gtk_combo_box_set_model( gui_arg.sender.order_sel, GTK_TREE_MODEL(list_store));
		if (gtk_tree_model_iter_n_children( GTK_TREE_MODEL(list_store), NULL) == 0){
			gtk_combo_box_set_model( gui_arg.sender.order_sel, NULL);
		}
		//gtk_combo_box_set_active must not be in a mutex (weird)
		else gtk_combo_box_set_active ( gui_arg.sender.order_sel, 0);
		
		g_object_unref( G_OBJECT(list_store)); //model now owned by comboBox
	}
	//get current order (might not have order, in which case gtk_combo_box_get_active_iter returns FALSE
	if (gtk_combo_box_get_active_iter( gui_arg.sender.order_sel, &i) == FALSE) cur.order = -1;
	else {
		GtkTreeModel *l = gtk_combo_box_get_model( gui_arg.sender.order_sel);
		gtk_tree_model_get(l, &i, 0, &dest, -1);
		cur.order = (int)strtol(dest, NULL, 10);
	}
	//set order_changed to true here if it differs
	if (prev.order != cur.order) order_changed = true;
	prev.order = cur.order;
	
	//product
	if (order_changed){ //fetch product name
		static int order_state = 0;
		if (cur.order == -1){ //put nothing in product
			gtk_combo_box_set_model (gui_arg.sender.product_sel, NULL);
			order_state = 0;
			//not sure about the below. it looks bad
			//gtk_widget_hide( GTK_WIDGET(gui_arg.sender.product_sel)); //actualy make it invisible
			//gtk_widget_hide( GTK_WIDGET(gui_arg.sender.order_sel));
		} else {
			//gtk_widget_show( GTK_WIDGET(gui_arg.sender.product_sel));
			//gtk_widget_show( GTK_WIDGET(gui_arg.sender.order_sel));
			if (!strcmp(prev.action, "AMEND") || !strcmp(prev.action, "CANCEL")){
				//put one thing in product
				pthread_mutex_lock(exchange_arg.mutex);
				cur.product = get_order_by_user(exchange_arg.ex, cur.trader, cur.order)->order->product;
				pthread_mutex_unlock(exchange_arg.mutex);
				GtkListStore *l = gtk_list_store_new(1, G_TYPE_STRING);
				gtk_list_store_append(l, &i);
				gtk_list_store_set(l, &i, 0, exchange_arg.ex->item_names[cur.product], -1);
				gtk_combo_box_set_model( gui_arg.sender.product_sel, GTK_TREE_MODEL(l));
				g_object_unref( G_OBJECT(l));
				gtk_combo_box_set_active ( gui_arg.sender.product_sel, 0);
				order_state = 1;
				prev.product = cur.product;
			} else { //display everything
				gtk_combo_box_set_model( gui_arg.sender.product_sel,  
											GTK_TREE_MODEL(gui_arg.sender.static_product));
				cur.product = gtk_combo_box_get_active( gui_arg.sender.product_sel);
				if (order_state != 2) gtk_combo_box_set_active ( gui_arg.sender.product_sel, 0);
				order_state = 2;
				prev.product = cur.product;
			}
		}
	} else if (cur.order != -1) prev.product = gtk_combo_box_get_active( gui_arg.sender.product_sel);
	//no need for qty and price change either, but need to store
	prev.quantity = gtk_spin_button_get_value_as_int (gui_arg.sender.qty_sel);
	prev.price = gtk_spin_button_get_value_as_int (gui_arg.sender.price_sel);
	
	//compose message and write it to gui_arg.sender.preview
	bool valid_msg = true;
	if (cur.order == -1) valid_msg = false;
	char buf[90];
	if (valid_msg){
		if (!strcmp(prev.action, "CANCEL")){
			sprintf(gui_arg.sender.send_msg, "CANCEL %d;", prev.order);
		} else if (!strcmp(prev.action, "AMEND")){
			sprintf(gui_arg.sender.send_msg, "AMEND %d %d %d;",
				prev.order, prev.quantity, prev.price);
		}else{
			sprintf(gui_arg.sender.send_msg, "%s %d %s %d %d;", prev.action, prev.order,
				exchange_arg.ex->item_names[prev.product], prev.quantity, prev.price);
		}
		sprintf(buf, "<span>%d -> [PEX]: \"%s\"</span>", prev.trader, gui_arg.sender.send_msg);
	} else {
		sprintf(buf, "<span foreground=\"red\">order does not exist</span>");
	}
	gui_arg.sender.valid_msg = valid_msg;
	gtk_label_set_markup(gui_arg.sender.preview, buf);
  	gtk_label_set_use_markup(gui_arg.sender.preview, TRUE);
}

void send_masquarade_cb(GtkWidget *widget, gpointer data){
	int num = gtk_spin_button_get_value_as_int (gui_arg.sender.trader_sel);
	struct trader *t = exchange_arg.ex->traders + num;
	char *txt;
	if (t->disconnected){
		txt = "<span foreground=\"red\">cannot masquarade: trader disconnected</span>";
	} else if (!gui_arg.sender.valid_msg) {
		txt = "<span foreground=\"red\">cannot masquarade: message not valid</span>";
	} else {
		write(t->fake_read, gui_arg.sender.send_msg, strlen(gui_arg.sender.send_msg));
		kill(t->pid, SIGUSR1);
		txt = "<span>message sent.</span>";
	}
	gtk_label_set_markup(gui_arg.sender.status, txt);
  	gtk_label_set_use_markup(gui_arg.sender.status, TRUE);
}

void terminate_trader_cb(GtkWidget *widget, gpointer data){
	char buf[70];
	//get selected trader number
	int num = gtk_spin_button_get_value_as_int (gui_arg.sender.trader_sel);
	struct trader *t = exchange_arg.ex->traders + num;
	if (t->disconnected){
		sprintf(buf, "<span foreground=\"red\">trader %d already disconnected</span>", num);
	} else {
		kill(t->pid, SIGTERM);
		sprintf(buf, "<span>trader %d removed</span>", num);
	}
  	gtk_label_set_markup(gui_arg.sender.status, buf);
  	gtk_label_set_use_markup(gui_arg.sender.status, TRUE);
}

void spinButton_value_changed_cb(GtkSpinButton *self, gpointer user_data){
	populate_sender(false);
}

void comboBox_changed_cb(GtkComboBox *self, gpointer user_data){
	//delegate work to populate_sender instead, so each item can be examined
	populate_sender(false);
}

