#include "defs.h"
#include "kernel.h"
#include "utils.h"
#include "igmp.h"
#include "pim.h"
#include "vif.h"

struct sigaction sa;

int do_vifs = 1;
int retry_forever = 0;
struct rp_hold *g_rp_hold = NULL;

void close() 
{
	printf("Closing...\n");
	
	stop_all_vifs();
	
	if (igmp_socket != 0)
	{
		if (pim_socket != 0)
			k_stop_pim(igmp_socket);

		close(igmp_socket);
		igmp_socket = 0;
	}
	
	if (pim_socket != 0)
	{
		close(pim_socket);
		pim_socket = 0;
	}

	/*
	 * When IOCTL_OK_ON_RAW_SOCKET is defined, 'udp_socket' is equal
	 * 'to igmp_socket'. Therefore, 'udp_socket' should be closed only
	 * if they are different.
	 */
#ifndef IOCTL_OK_ON_RAW_SOCKET
	if (udp_socket != 0)
	{
		close(udp_socket);
		udp_socket = 0;
	}
#endif

	nhandlers = 0;
	
	printf("Exited\n");

	exit(0);
}

static void sig_handler(int sig)
{
	switch (sig) {
	case SIGALRM:
		printf("SIGALRM\n");
		break;

	case SIGINT:
		printf("SIGINT\n");
		close();
		break;

	case SIGTERM:
		printf("SIGTERM\n");
		close();
		break;

	case SIGHUP:
		printf("SIGHUP\n");
		close();
		break;

	case SIGUSR1:
		printf("SIGUSR1\n");
		break;

	case SIGUSR2:
		printf("SIGUSR2\n");
		break;

	case SIGTSTP:
		printf("SIGTSTP\n");
		close();
		break;
	}
}

//static void add_static_rp(void)
//{
//	struct rp_hold *rph = g_rp_hold;
//
//	while (rph) {
//		add_rp_grp_entry(&cand_rp_list,
//			&grp_mask_list,
//			rph->address,
//			1,
//			(uint16_t)0xffffff,
//			rph->group,
//			rph->mask,
//			curr_bsr_hash_mask,
//			curr_bsr_fragment_tag);
//		rph = rph->next;
//	}
//}
//
//static void del_static_rp(void)
//{
//	struct rp_hold *rph = g_rp_hold;
//
//	while (rph) {
//		delete_rp(&cand_rp_list, &grp_mask_list, rph->address);
//		rph = rph->next;
//	}
//}

int main(int argc, char *argv[])
{
	fd_set rfds, readers;
	int nfds, i, n;
	struct timeval tv;
	
	if (geteuid() != 0)
	{
		printf("Need root privileges to start.");
		exit(1);
	}
	
	init_igmp();
	init_pim();
	init_vifs();
	
//	init_rp_and_bsr(); /* Must be after init_vifs() */
//	add_static_rp(); /* Must be after init_vifs() */

	sa.sa_handler = sig_handler;
	sa.sa_flags = 0; /* Interrupt system calls */
	
	sigemptyset(&sa.sa_mask);

	sigaction(SIGALRM, &sa, NULL);
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGUSR1, &sa, NULL);
	sigaction(SIGUSR2, &sa, NULL);
	sigaction(SIGTSTP, &sa, NULL);
	
	FD_ZERO(&readers);
	FD_SET(igmp_socket, &readers);

	nfds = igmp_socket + 1;
	for (i = 0; i < nhandlers; i++) {
		FD_SET(ihandlers[i].fd, &readers);
		if (ihandlers[i].fd >= nfds)
			nfds = ihandlers[i].fd + 1;
	}
	
	while (1) {
		memcpy(&rfds, &readers, sizeof(rfds));

		tv.tv_sec = 1;
		tv.tv_usec = 0;

		if ((n = select(nfds, &rfds, NULL, NULL, &tv)) < 0) {
			if (errno != EINTR) /* SIGALRM is expected */
			printf("select failed: %d\n", errno);
			continue;
		}

		if (n > 0) {
			
			for (i = 0; i < nhandlers; i++) {
				if (FD_ISSET(ihandlers[i].fd, &rfds)) {
					(*ihandlers[i].func)(ihandlers[i].fd, &rfds);
				}
			}
		}

	} /* Main loop */

	return 0;
}