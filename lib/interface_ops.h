#ifndef INTERFACE_OPS

#include "exchange_core.h"
#include "operations.h"

int update_text_log(FILE *msg_fp, GtkScrolledWindow *text_log_scroll, GtkTextBuffer *text_log_content);
void spinButton_value_changed_cb(GtkSpinButton* self, gpointer user_data);
void comboBox_changed_cb(GtkComboBox *self, gpointer user_data);
void send_masquarade_cb(GtkWidget *widget, gpointer data);
void terminate_trader_cb(GtkWidget *widget, gpointer data);
void populate_sender(bool exchange_changed);

#endif
