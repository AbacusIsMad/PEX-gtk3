#include "interface_setup.h"

extern struct exchange_arg_helper exchange_arg;
extern struct gui_helper gui_arg;

void setup_text_log(GtkBuilder *builder){
	GtkScrolledWindow *text_log_scroll =
		(GtkScrolledWindow*)gtk_builder_get_object (builder, "text_log_scroll");
	gui_arg.text_log_scroll = text_log_scroll;
	
	GtkTextBuffer *text_log_content = (GtkTextBuffer*)gtk_builder_get_object(builder, "text_log_content");
	
	gui_arg.text_log_content = text_log_content;
}

static void set_combo_to_text(GtkWidget *combo){
	GtkCellRenderer *column = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(combo), column, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(combo), column, "text", 0, NULL);
}

static void terminate_trader_cb(GtkWidget *widget, gpointer data){
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

void setup_sender(GtkBuilder *builder){
	//the status message board
	GtkLabel *status = GTK_LABEL( gtk_builder_get_object (builder, "sender_status"));
	gui_arg.sender.status = status;
	
	//the preview - this should get regularly updated!
	GtkLabel *preview = GTK_LABEL( gtk_builder_get_object (builder, "sender_preview"));
	gui_arg.sender.preview = preview;

	//the action dropdown select
	GtkWidget *action = GTK_WIDGET( gtk_builder_get_object (builder, "sender_action"));
	set_combo_to_text(action);
	gtk_combo_box_set_active(GTK_COMBO_BOX(action), 0);
	g_signal_connect(action, "changed", G_CALLBACK(comboBox_changed_cb), NULL);
	gui_arg.sender.action_sel = GTK_COMBO_BOX(action);
	
	//the other selects
	GtkWidget *w = GTK_WIDGET( gtk_builder_get_object (builder, "sender_id")); //i know its a number but text
	set_combo_to_text(w);
	g_signal_connect(w, "changed", G_CALLBACK(comboBox_changed_cb), NULL);
	gui_arg.sender.order_sel = GTK_COMBO_BOX(w);
	w = GTK_WIDGET( gtk_builder_get_object (builder, "sender_product"));
	set_combo_to_text(w);
	g_signal_connect(w, "changed", G_CALLBACK(comboBox_changed_cb), NULL);
	gui_arg.sender.product_sel = GTK_COMBO_BOX(w);
	//get the product list setup
	GtkListStore *l = gtk_list_store_new (1, G_TYPE_STRING);
	GtkTreeIter iter;
	for (int i = 0; i < exchange_arg.ex->item_no; i++){
		gtk_list_store_append (l, &iter);
		gtk_list_store_set (l, &iter, 0, exchange_arg.ex->item_names[i], -1);
	}
  	gui_arg.sender.static_product = l;
	
	//set the upper bound of trader select to current trader num
	GtkSpinButton *trader_sel = GTK_SPIN_BUTTON( gtk_builder_get_object (builder, "sender_trader"));
	GtkAdjustment *adj = gtk_spin_button_get_adjustment (trader_sel);
	gtk_adjustment_set_upper (adj, exchange_arg.ex->trader_num - 1);
	g_signal_connect(trader_sel, "value-changed", G_CALLBACK(spinButton_value_changed_cb), NULL);
	gui_arg.sender.trader_sel = trader_sel;
	
	//the other spin buttons
	GtkSpinButton *s = GTK_SPIN_BUTTON( gtk_builder_get_object (builder, "sender_qty"));
	g_signal_connect(s, "value-changed", G_CALLBACK(spinButton_value_changed_cb), NULL);
  	gui_arg.sender.qty_sel = s;
  	s = GTK_SPIN_BUTTON( gtk_builder_get_object (builder, "sender_price"));
	g_signal_connect(s, "value-changed", G_CALLBACK(spinButton_value_changed_cb), NULL);
  	gui_arg.sender.price_sel = s;
	
	//the trader disconnect button
	GtkButton *remove = GTK_BUTTON( gtk_builder_get_object (builder, "sender_remove"));
	g_signal_connect (remove, "clicked", G_CALLBACK (terminate_trader_cb), NULL);
}


gboolean drawing_area_configure_cb (GtkWidget *widget, GdkEventConfigure *event){
	
	if (event -> type == GDK_CONFIGURE)
	{
		if (gui_arg.graph.surface != (cairo_surface_t *)NULL)
		{
			cairo_surface_destroy (gui_arg.graph.surface);
		}
		GtkAllocation allocation;
		gtk_widget_get_allocation (widget, &allocation);
		gui_arg.graph.surface = 
			cairo_image_surface_create (CAIRO_FORMAT_ARGB32, allocation.width, allocation.height);
		gui_arg.graph.width  = allocation.width;
		gui_arg.graph.height = allocation.height;
	}
	gui_arg.graph.context = cairo_create (gui_arg.graph.surface);

	return TRUE;
}

#define NUM_POINTS (1000u)
#define PERIOD (100u)
#include <math.h>
static inline float
sine_to_point (int x, int width, int height)
{
  return (height / 2.0) * sin (x * 2 * M_PI / (PERIOD)) + height / 2.0;
}

void drawing_area_draw_cb (GtkWidget *widget, cairo_t *context2, void *ptr){
  /* Copy the contents of the surface to the current context. */
  cairo_surface_t *surface = gui_arg.graph.surface;
  if (surface != (cairo_surface_t *)NULL) {
  	printf("drawing with dim %d, %d\n", gui_arg.graph.width, gui_arg.graph.height);
  	cairo_t *context = cairo_create (surface);

    /* Draw the background. */
    cairo_set_source_rgb (context, 1, 1, 1);
    cairo_rectangle (context, 0, 0, gui_arg.graph.width, gui_arg.graph.height);
    cairo_fill (context);

    /* Draw a moving sine wave. */
    cairo_set_source_rgb (context, 0.5, 0.5, 0);
    cairo_move_to(context, 0, sine_to_point (0, gui_arg.graph.width, gui_arg.graph.height));

    for (int i = 1; i < NUM_POINTS; i++)
    {
      cairo_line_to (context,  i, sine_to_point (i, gui_arg.graph.width, gui_arg.graph.height));
    }

    cairo_stroke (context);

    cairo_destroy (context);
  	
  	
    cairo_set_source_surface (context2, surface, 0, 0);
    cairo_paint (context2);
  }
}
