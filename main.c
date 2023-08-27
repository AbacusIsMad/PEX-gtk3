#include "main.h"

static pthread_t exchange_thread;
static pthread_mutex_t read_mutex;
static pthread_cond_t init_ready = PTHREAD_COND_INITIALIZER;
struct exchange *ex;

extern struct exchange_arg_helper exchange_arg;
extern struct gui_helper gui_arg;


static void
print_hello (GtkWidget *widget,
             gpointer   data)
{
  g_print ("Hello World\n");
}



static void
quit_cb (GtkWidget *widget, gpointer data)
{
  GtkWindow *window = data;

  gtk_window_close (window);
  exit(0);
}


static void activate(GtkApplication* app, gpointer user_data){
	GtkBuilder *builder = gtk_builder_new ();
	gtk_builder_add_from_file (builder, "pex.ui", NULL);

	/* Connect signal handlers to the constructed widgets. */
	GObject *window = gtk_builder_get_object (builder, "window");
	gtk_window_set_default_size (GTK_WINDOW (window), 800, 600);
	gtk_window_set_resizable(GTK_WINDOW(window),FALSE);
	gtk_window_set_application (GTK_WINDOW (window), app);
	
	GObject *button = gtk_builder_get_object (builder, "button1");
	g_signal_connect (button, "clicked", G_CALLBACK (print_hello), NULL);

	button = gtk_builder_get_object (builder, "button2");
	g_signal_connect (button, "clicked", G_CALLBACK (print_hello), NULL);

	button = gtk_builder_get_object (builder, "quit");
	g_signal_connect_swapped (button, "clicked", G_CALLBACK (quit_cb), window);
	
	setup_text_log(builder);
	
	GtkWidget *graph = (GtkWidget*)gtk_builder_get_object (builder, "graph");
	//begins
	g_signal_connect (graph, "configure-event", G_CALLBACK (drawing_area_configure_cb), NULL);
	//I just have to send the draw signal every time 
	g_signal_connect (graph, "draw", G_CALLBACK (drawing_area_draw_cb), NULL);
	gui_arg.graph.graph = graph;
	gui_arg.graph.surface = NULL;

	//use gtk_combo_box_set_model to change treemodels
	setup_sender(builder);
	
	gtk_window_present (GTK_WINDOW (window));
	g_object_unref (builder);

}

int main(int argc, char **argv){
	GtkApplication *app;
	int status;
	GError *error = NULL;
	//for communicating between exchange
	GIOChannel *g_signal_in;
	
	
	//ex = mmap(NULL, sizeof(struct exchange), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
	ex = malloc(sizeof(struct exchange));
	if (ex == MAP_FAILED || ex == NULL) perror("map failed"), exit(1);

	int trigger_pipe[2], msg_pipe[2];
	(void)!pipe2(trigger_pipe, O_CLOEXEC | O_NONBLOCK);
	(void)!pipe2(msg_pipe, O_CLOEXEC | O_NONBLOCK);
	
	//don't use fork() due to shared memory issues, use threads instead!
	pthread_mutex_init(&read_mutex, NULL); //coarse grained lock
	//connect message pipe to a stdio instance so I can call fgets on it
	FILE *msg_fp = fdopen(msg_pipe[0], "r");
	exchange_arg = (struct exchange_arg_helper){
		argc, argv, ex, trigger_pipe[1], msg_pipe[1], msg_fp, &read_mutex, &init_ready, false,
	};
	
	if (pthread_create(&exchange_thread, NULL, start_exchange, (void*)&exchange_arg)){
		perror("could not create exchange thread");
		exit(1);
	}
	
	app = gtk_application_new ("org.gtk.example", G_APPLICATION_FLAGS_NONE);

	//connecting fd to gio	
	g_signal_in = g_io_channel_unix_new(trigger_pipe[0]);
	g_io_channel_set_encoding(g_signal_in, NULL, &error);
	g_io_add_watch(g_signal_in, G_IO_IN | G_IO_PRI | G_IO_HUP, receive_update, NULL);
	

	g_signal_connect (app, "activate", G_CALLBACK (activate), NULL);
	
	//wait for init to completely finish before moving on, so I can access the ex structure.
	pthread_mutex_lock(exchange_arg.mutex);
	while (exchange_arg.ready_flag == false){
		pthread_cond_wait(exchange_arg.init_ready, exchange_arg.mutex);
	}
	pthread_mutex_unlock(exchange_arg.mutex);
	puts("exchange creation finished");
	//kill(exchange_arg.ex->traders[0].pid, SIGTERM);
	
	status = g_application_run (G_APPLICATION (app), 1, argv);
	g_object_unref (app);
	
	exc_teardown(ex);
	free(ex);
	return status;
}

void* start_exchange(void *arg){
	struct exchange_arg_helper *ex_arg = (struct exchange_arg_helper *)arg;
	sleep(1);
	
	ex_main(ex_arg->argc, ex_arg->argv, ex_arg->ex, ex_arg);
	puts("exchange finished");
	
	//thread sleep for a while, so the last message can propagate through
	pthread_mutex_t tmp = PTHREAD_MUTEX_INITIALIZER;
	static pthread_cond_t cond_tmp = PTHREAD_COND_INITIALIZER;
	pthread_mutex_lock(&tmp);
	struct timespec t;
	do {
		clock_gettime(CLOCK_REALTIME, &t);
		t.tv_sec += 1;
	} while(pthread_cond_timedwait(&cond_tmp, &tmp, &t) != ETIMEDOUT); //make sure to actually time out
	pthread_mutex_unlock(&tmp);
	
	close(ex_arg->trigger_pipe);
	close(ex_arg->msg_pipe);
	pthread_exit(0);
	return NULL;
}
