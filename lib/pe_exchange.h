#ifndef PE_EXCHANGE_H
#define PE_EXCHANGE_H

#include "pe_common.h"
#include "exchange_core.h"
#include "btree.h"
#include "operations.h"

//mutex should be locked whenever a tree change operation happens
//write_pipe is done at the end of each operation to force cairo to update
int ex_main(int argc, char ** argv, struct exchange *ex, struct exchange_arg_helper *ex_arg);




#endif
