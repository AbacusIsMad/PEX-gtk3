#include "interface_main.h"

extern struct exchange_arg_helper exchange_arg;
extern struct gui_helper gui_arg;

gboolean receive_update(GIOChannel *source, GIOCondition cond, gpointer d){
	GError *error = NULL;

	if (cond & G_IO_HUP) {
		puts("connection terminated");
		return (FALSE);
	}

	union {
		gchar chars[sizeof(int)];
		int signal;
	} buf;
	GIOStatus status;
	gsize bytes_read;
	
	while((status = g_io_channel_read_chars(source, buf.chars, sizeof(int),
						&bytes_read, &error)) == G_IO_STATUS_NORMAL) {
		g_assert(error == NULL);
		
		if (bytes_read != sizeof(int)){
			fprintf(stderr, "lost data in pipe (expected %ld, received %ld)\n", sizeof(int), bytes_read);
			continue;
		}
		
		printf("received trigger %d\n", buf.signal);
		
		
		//chuck it in the text log
		
		if (update_text_log(exchange_arg.msg_fp, gui_arg.text_log.scroll, gui_arg.text_log.content)){
			fprintf(stderr, "expected message from exchange from trigger %d, "
								"got error (likely EOF)\n", buf.signal);
			//don't do continue, because things might still get updated
		}
		
		if (buf.signal == -1) { //trader disconnect
		
		} else {
			populate_sender(true);
		}
		
		//TEMP: update the drawing surface
		gtk_widget_queue_draw (gui_arg.graph.graph);
		
		buf.signal = 0;
	}
	
	if (error != NULL) {
		fprintf(stderr, "reading pipe failed: %s\n", error->message);
		exit(1);
	}
	if (status == G_IO_STATUS_EOF){
		fprintf(stderr, "reading pipe failed: %s\n", error->message);
		exit(1);
	}
	
	g_assert(status == G_IO_STATUS_AGAIN);
	return (TRUE);
}

int masq_trader(struct exchange *ex, int id, char *msg, int maxlen){
	struct trader *t = ex->traders + id;
	if (t->disconnected) return -1;
	//(void)!write(t->fake_read, msg, strnlen(msg, maxlen));
    kill(t->pid, SIGUSR1); //usr1 for the sandbox trader is to pass along the message
    return 0;
}
