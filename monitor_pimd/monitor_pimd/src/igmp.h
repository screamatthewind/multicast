#ifndef IGMP_H
#define IGMP_H

#include "defs.h"
#include "utils.h"
#include "kernel.h"

extern char   *igmp_recv_buf; /* input packet buffer               */
extern int     igmp_socket; /* socket for all network I/O        */

extern uint32_t allhosts_group; /* allhosts  addr in net order       */
extern uint32_t allrouters_group; /* All-Routers addr in net order     */
extern uint32_t allreports_group; /* All IGMP routers in net order     */

void init_igmp(void);
static void accept_igmp(ssize_t recvlen);
static void igmp_read(int i __attribute__((unused)), fd_set *rfd __attribute__((unused)));

#endif