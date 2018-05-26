#include "igmp.h"

char   *igmp_recv_buf; /* input packet buffer               */
int     igmp_socket = 0; /* socket for all network I/O        */

uint32_t allhosts_group;   /* allhosts  addr in net order       */
uint32_t allrouters_group; /* All-Routers addr in net order     */
uint32_t allreports_group; /* All IGMP routers in net order     */

void init_igmp(void)
{
	struct ip *ip;
	char *router_alert;

	igmp_recv_buf = (char *) calloc(1, RECV_BUF_SIZE);

	if ((igmp_socket = socket(AF_INET, SOCK_RAW, IPPROTO_IGMP)) < 0)
		printf("Failed creating IGMP socket in init_igmp: %d\n", errno);

	k_hdr_include(igmp_socket, TRUE); /* include IP header when sending */
	k_set_rcvbuf(igmp_socket,
		SO_RECV_BUF_SIZE_MAX,
		SO_RECV_BUF_SIZE_MIN); /* lots of input buffering        */
	k_set_loop(igmp_socket, FALSE); /* disable multicast loopback     */

	allhosts_group   = htonl(INADDR_ALLHOSTS_GROUP);
	allrouters_group = htonl(INADDR_ALLRTRS_GROUP);
	allreports_group = htonl(INADDR_ALLRPTS_GROUP);
	
	if (register_input_handler(igmp_socket, igmp_read) < 0)
		printf("Failed registering igmp_read() as an input handler in init_igmp()\n");
}

static void accept_igmp(ssize_t recvlen)
{
	int ipdatalen, iphdrlen, igmpdatalen;
	uint32_t src, dst, group;
	struct ip *ip;
	struct igmp *igmp;
	int igmp_version = 3;

	if (recvlen < (ssize_t)sizeof(struct ip)) {
		printf("Received IGMP packet too short (%zd bytes) for IP header\n", recvlen);
		return;
	}

	ip  = (struct ip *)igmp_recv_buf;
	src = ip->ip_src.s_addr;
	dst = ip->ip_dst.s_addr;

	/* packets sent up from kernel to daemon have ip->ip_p = 0 */
	if (ip->ip_p == 0) {
#if 0                           /* XXX */
		if (src == 0 || dst == 0)
			logit(LOG_WARNING,
				0,
				"Kernel request not accurate, src %s dst %s",
				inet_fmt(src, s1, sizeof(s1)),
				inet_fmt(dst, s2, sizeof(s2)));
		else
#endif
			process_kernel_call();
		return;
	}

	iphdrlen  = ip->ip_hl << 2;
#if 0
#ifdef HAVE_IP_HDRINCL_BSD_ORDER
#ifdef __NetBSD__
	ipdatalen = ip->ip_len; /* The NetBSD kernel subtracts hlen for us, unfortunately. */
#else
	ipdatalen = ip->ip_len - iphdrlen;
#endif
#else
	ipdatalen = ntohs(ip->ip_len) - iphdrlen;
#endif
#else   /* !0 */
	ipdatalen = recvlen - iphdrlen;
#endif  /* O */

	if (iphdrlen + ipdatalen != recvlen) {
		printf("Received packet from %s shorter (%zd bytes) than hdr+data length (%u+%u)\n",
			inet_fmt(src, s1, sizeof(s1)),
			recvlen,
			iphdrlen,
			ipdatalen);
		return;
	}

	igmp        = (struct igmp *)(igmp_recv_buf + iphdrlen);
	group       = igmp->igmp_group.s_addr;
	igmpdatalen = ipdatalen - IGMP_MINLEN;

	if (igmpdatalen < 0) {
		printf("Received IP data field too short (%u bytes) for IGMP, from %s\n",
			ipdatalen,
			inet_fmt(src, s1, sizeof(s1)));
		return;
	}

	printf("Received %s from %-18s to %-18s\n",
		packet_kind(IPPROTO_IGMP, igmp->igmp_type, igmp->igmp_code),
		inet_fmt(src, s1, sizeof(s1)),
		inet_fmt(dst, s2, sizeof(s2)));

	switch (igmp->igmp_type) {
	case IGMP_MEMBERSHIP_QUERY:
		/* RFC 3376:7.1 */
		if (ipdatalen == 8) {
			if (igmp->igmp_code == 0)
				igmp_version = 1;
			else
				igmp_version = 2;
		}
		else if (ipdatalen >= 12) {
			igmp_version = 3;
		}
		else {
			printf(
			"Received invalid IGMP Membership query: Max Resp Code = %d, length = %d\n",
				igmp->igmp_code,
				ipdatalen);
		}
		// printf("IGMP_MEMBERSHIP_QUERY\n");
		return;

	case IGMP_V1_MEMBERSHIP_REPORT:
	case IGMP_V2_MEMBERSHIP_REPORT:
		// printf("IGMP_V2_MEMBERSHIP_REPORT\n");
		return;

	case IGMP_V2_LEAVE_GROUP:
		// printf("IGMP_V2_LEAVE_GROUP\n");
		return;

	case IGMP_V3_MEMBERSHIP_REPORT:
		// printf("IGMP_V3_MEMBERSHIP_REPORT\n");
		return;

	case IGMP_PIM:
		// printf("IGMP_PIM\n");
		return;    /* TODO: this is PIM v1 message. Handle it?. */

	case IGMP_MTRACE_RESP:
		printf("IGMP_MTRACE_RESP\n");
		return;    /* TODO: implement it */

	case IGMP_MTRACE:
		printf("IGMP_MTRACE\n");
		return;

	default:
		printf(
		"Ignoring unknown IGMP message type %x from %s to %s\n",
			igmp->igmp_type,
			inet_fmt(src, s1, sizeof(s1)),
			inet_fmt(dst, s2, sizeof(s2)));
		return;
	}
}

/* Read an IGMP message */
static void igmp_read(int i __attribute__((unused)), fd_set *rfd __attribute__((unused)))
{
	ssize_t len;
	socklen_t dummy = 0;

	while ((len = recvfrom(igmp_socket, igmp_recv_buf, RECV_BUF_SIZE, 0, NULL, &dummy)) < 0) {
		if (errno == EINTR)
			continue;           /* Received signal, retry syscall. */

		printf("Failed recvfrom() in igmp_read: %d\n", errno);
		return;
	}

	accept_igmp(len);
}