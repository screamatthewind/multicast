#ifndef KERNEL_H
#define KERNEL_H

#include "defs.h"

extern int mrt_table_id;

void process_kernel_call(void);
static void process_cache_miss(struct igmpmsg *igmpctl);

void k_hdr_include(int socket, int val);
void k_set_rcvbuf(int socket, int bufsize, int minsize);
void k_set_loop(int socket, int flag);
void k_init_pim(int socket);
void k_stop_pim(int socket);
void k_add_vif(int socket, vifi_t vifi, struct uvif *v);
void k_del_vif(int socket, vifi_t vifi, struct uvif *v __attribute__((unused)));
void k_leave(int socket, uint32_t grp, struct uvif *v);
void k_join(int socket, uint32_t grp, struct uvif *v);

#endif