#include "defs.h"
#include "kernel.h"
#include "utils.h"
#include "pim.h"

char    *pim_recv_buf;        /* input packet buffer   */
int      pim_socket = 0;      /* socket for PIM control msgs */
uint32_t allpimrouters_group; /* ALL_PIM_ROUTERS address in net order */

typedef struct pim pim_header_t;

void init_pim(void)
{
	struct ip *ip;

	/* Setup the PIM raw socket */
	if ((pim_socket = socket(AF_INET, SOCK_RAW, IPPROTO_PIM)) < 0)
		printf("Failed creating PIM socket: %d\n", errno);

	k_hdr_include(pim_socket, TRUE); /* include IP header when sending */
	k_set_rcvbuf(pim_socket,
		SO_RECV_BUF_SIZE_MAX,
		SO_RECV_BUF_SIZE_MIN); /* lots of input buffering        */
	k_set_loop(pim_socket, FALSE); /* disable multicast loopback     */

	allpimrouters_group = htonl(INADDR_ALL_PIM_ROUTERS);
	
	pim_recv_buf = (char *) calloc(1, RECV_BUF_SIZE);
	if (!pim_recv_buf)
		printf("Ran out of memory in init_pim()\n");

	if (register_input_handler(pim_socket, pim_read) < 0)
		printf("Failed registering pim_read() as an input handler\n");
}

/* Read a PIM message */
static void pim_read(int f __attribute__((unused)), fd_set *rfd __attribute__((unused)))
{
	ssize_t len;
	socklen_t dummy = 0;
	sigset_t block, oblock;

	while ((len = recvfrom(pim_socket, pim_recv_buf, RECV_BUF_SIZE, 0, NULL, &dummy)) < 0) {
		if (errno == EINTR)
			continue;           /* Received signal, retry syscall. */

		printf("Failed recvfrom() in pim_read: %d\n", errno);
		return;
	}

//	sigemptyset(&block);
//	sigaddset(&block, SIGALRM);
//	
//	if (sigprocmask(SIG_BLOCK, &block, &oblock) < 0)
//		printf("Error: %d during sigprocmask\n", errno);

	accept_pim(len);

//	sigprocmask(SIG_SETMASK, &oblock, (sigset_t *)NULL);
}

static void accept_pim(ssize_t recvlen)
{
	uint32_t src, dst;
	struct ip *ip;
	pim_header_t *pim;
	int iphdrlen, pimlen;
	char source[20], dest[20];

	if (recvlen < (ssize_t)sizeof(struct ip)) {
		printf("Received PIM packet too short (%u bytes) for IP header\n", recvlen);
		return;
	}

	ip          = (struct ip *)pim_recv_buf;
	src         = ip->ip_src.s_addr;
	dst         = ip->ip_dst.s_addr;
	iphdrlen    = ip->ip_hl << 2;

	pim         = (pim_header_t *)(pim_recv_buf + iphdrlen);
	pimlen      = recvlen - iphdrlen;

	/* Sanity check packet length */
	if (pimlen < (ssize_t)sizeof(*pim)) {
		printf(
			"IP data field too short (%d bytes) for PIM header, from %s to %s\n",
			pimlen,
			inet_fmt(src, source, sizeof(source)),
			inet_fmt(dst, dest, sizeof(dest)));
		return;
	}

	printf(
	"Received %s from %-18s to %-18s\n",
		packet_kind(IPPROTO_PIM, pim->pim_type, 0),
		inet_fmt(src, source, sizeof(source)),
		inet_fmt(dst, dest, sizeof(dest)));

	switch (pim->pim_type) {
	case PIM_HELLO:
		// printf("PIM_HELLO\n");
		break;

	case PIM_REGISTER:
		printf("PIM_REGISTER\n");
		break;

	case PIM_REGISTER_STOP:
		printf("PIM_REGISTER_STOP\n");
		break;

	case PIM_JOIN_PRUNE:
		// printf("PIM_JOIN_PRUNE\n");
		break;

	case PIM_BOOTSTRAP:
		printf("PIM_BOOTSTRAP\n");
		break;

	case PIM_ASSERT:
		// printf("PIM_ASSERT\n");
		break;

	case PIM_GRAFT:
	case PIM_GRAFT_ACK:
		printf(
			"ignore %s from %s to %s\n",
			packet_kind(IPPROTO_PIM, pim->pim_type, 0),
			inet_fmt(src, source, sizeof(source)),
			inet_fmt(dst, dest, sizeof(dest)));
		break;

	case PIM_CAND_RP_ADV:
		printf("PIM_CAND_RP_ADV\n");
		break;

	default:
		printf(
		"ignore unknown PIM message code %u from %s to %s",
			pim->pim_type,
			inet_fmt(src, source, sizeof(source)),
			inet_fmt(dst, dest, sizeof(dest)));
		break;
	}
}


