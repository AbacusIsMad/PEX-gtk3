#ifndef INTERFACE_MAIN
#define INTERFACE_MAIN

#include "pe_exchange.h"
#include "interface_setup.h"
#include "interface_ops.h"

int masq_trader(struct exchange *ex, int id, char *msg, int maxlen);
gboolean receive_update(GIOChannel *source, GIOCondition cond, gpointer d);

#endif
