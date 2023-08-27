#ifndef INTERFACE_SETUP
#define INTERFACE_SETUP

#include "exchange_core.h"
#include "interface_ops.h"

gboolean drawing_area_configure_cb (GtkWidget *widget, GdkEventConfigure *event);
void drawing_area_draw_cb (GtkWidget *widget, cairo_t *context, void *ptr);

void setup_text_log(GtkBuilder *builder);
void setup_sender(GtkBuilder *builder);

#endif
