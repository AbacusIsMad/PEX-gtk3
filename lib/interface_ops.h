#ifndef INTERFACE_OPS

#include "exchange_core.h"
#include "operations.h"

int update_text_log(FILE *msg_fp, GtkScrolledWindow *text_log_scroll, GtkTextBuffer *text_log_content);
void spinButton_value_changed_cb(GtkSpinButton* self, gpointer user_data);
void comboBox_changed_cb(GtkComboBox *self, gpointer user_data);

#endif
