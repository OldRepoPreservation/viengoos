#ifndef VIENGOOS_AGER_H
#define VIENGOOS_AGER_H

#include <l4/thread.h>

/* The ager thread.  */
void ager_loop (l4_thread_id_t main_thread);

#endif
