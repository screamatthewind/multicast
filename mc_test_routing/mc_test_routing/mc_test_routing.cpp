#include <sys/socket.h>  
#include <arpa/inet.h>
#include <netinet/in.h> 
#include <netinet/ip.h>
#include <netinet/igmp.h>
#include <net/if.h>
#include <net/route.h>
#include <sys/ioctl.h>
#include <linux/mroute.h>
#include <errno.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SHE

#ifdef SHE
#define IIF 0
#define OIF 2
#else
#define IIF 1
#define OIF 0
#endif

using namespace std;

#define MAX_INET_BUF_LEN 19

char s1[MAX_INET_BUF_LEN];
char s2[MAX_INET_BUF_LEN];
char s3[MAX_INET_BUF_LEN];

char buf[1024];

/*
 * Convert an IP address in uint32_t (network) format into a printable string.
 */
char *inet_fmt(uint32_t addr, char *s, size_t len)
{
	uint8_t *a;

	a = (uint8_t *)&addr;
	snprintf(s, len, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);

	return s;
}

int main(int argc, char *argv[])
{
	struct mfcctl  mc;
	uint32_t source, group;
	vifi_t iif, oif;
	int igmp_socket;
	
	group = ntohl(4009755232); // 239.0.2.96
	source = ntohl(177864450); // 10.153.255.2
	iif = IIF;
	oif = OIF;
	
	if ((igmp_socket = socket(AF_INET, SOCK_RAW, IPPROTO_IGMP)) < 0)
	{
		perror("Failed creating IGMP socket");
		exit(0);
	}

	memset(&mc, 0, sizeof(mc));

	mc.mfcc_origin.s_addr   = source;
	mc.mfcc_mcastgrp.s_addr = group;
	mc.mfcc_parent          = iif;
	mc.mfcc_ttls[oif]        = 1;

	if (setsockopt(igmp_socket, IPPROTO_IP, MRT_ADD_MFC, (char *)&mc, sizeof(mc)) < 0) {
		perror("Failed to add MFC entry src");

		sprintf(buf, "socket error %d, src %s, grp %s\n", 
			errno,
			inet_fmt(mc.mfcc_origin.s_addr, s1, sizeof(s1)),
			inet_fmt(mc.mfcc_mcastgrp.s_addr, s2, sizeof(s2)));
		
		close(igmp_socket);
		exit(0);
	}

	memset(&mc, 0, sizeof(mc));

	mc.mfcc_origin.s_addr   = source;
	mc.mfcc_mcastgrp.s_addr = group;
	mc.mfcc_parent          = iif;
	mc.mfcc_ttls[oif]        = 1;

	if (setsockopt(igmp_socket, IPPROTO_IP, MRT_DEL_MFC, (char *)&mc, sizeof(mc)) < 0) {
		perror("Failed to delete MFC entry src");

		sprintf(buf, "socket error %d, src %s, grp %s\n", 
			errno,
			inet_fmt(mc.mfcc_origin.s_addr, s1, sizeof(s1)),
			inet_fmt(mc.mfcc_mcastgrp.s_addr, s2, sizeof(s2)));

		close(igmp_socket);
		exit(0);
	}

	close(igmp_socket);
	
	return 0;
}