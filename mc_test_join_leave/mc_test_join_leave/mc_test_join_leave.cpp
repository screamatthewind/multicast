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

using namespace std;

#define IP_ADDRESS "239.0.2.96"

int main(int argc, char *argv[])
{
	int sock_desc;
	
	if ((sock_desc = socket(PF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP)) < 0) {
		perror("Failed creating IGMP socket");
		exit(0);
	}
	
	struct ip_mreq multicastRequest;

	multicastRequest.imr_multiaddr.s_addr = inet_addr(IP_ADDRESS);
	multicastRequest.imr_interface.s_addr = htonl(INADDR_ANY);
	
	if (setsockopt(sock_desc, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *) &multicastRequest, sizeof(multicastRequest)) < 0) {
		perror("Failed to add membership");
		
		close(sock_desc);
		exit(0);
	}

	if (setsockopt(sock_desc, IPPROTO_IP, IP_DROP_MEMBERSHIP, (void *) &multicastRequest, sizeof(multicastRequest)) < 0) {
		perror("Failed sock_desc drop membership");

		close(sock_desc);
		exit(0);
	}

	close(sock_desc);
	
	return 0;
}