//https://github.com/direct-code-execution/ns-3-dce/blob/master/myscripts/mt2/mcsend.c

#include <sys/types.h>   /* for type definitions */
#include <sys/socket.h>  /* for socket API function calls */
#include <netinet/in.h>  /* for address structs */
#include <arpa/inet.h>   /* for sockaddr_in */
#include <stdio.h>       /* for printf() */
#include <stdlib.h>      /* for atoi() */
#include <string.h>      /* for strlen() */
#include <unistd.h>      /* for close() */

#define MAX_LEN  1024    /* maximum string size to send */
#define MIN_PORT 1024    /* minimum port allowed */
#define MAX_PORT 65535   /* maximum port allowed */

uint16_t udp_checksum(const void *buff, size_t len, in_addr_t src_addr, in_addr_t dest_addr)
{
	const uint16_t *buf = (const uint16_t *) buff;
	uint16_t *ip_src = (uint16_t *)&src_addr, *ip_dst = (uint16_t *)&dest_addr;
	uint32_t sum;
	size_t length = len;

	// Calculate the sum                                            //
	sum = 0;
	while (len > 1)
	{
		sum += *buf++;
		if (sum & 0x80000000)
			sum = (sum & 0xFFFF) + (sum >> 16);
		len -= 2;
	}

	if (len & 1)
		// Add the padding if the packet lenght is odd          //
		sum += *((uint8_t *)buf);

	// Add the pseudo-header                                        //
	sum += *(ip_src++);
	sum += *ip_src;

	sum += *(ip_dst++);
	sum += *ip_dst;

	sum += htons(IPPROTO_UDP);
	sum += htons(length);

	// Add the carries                                              //
	while(sum >> 16)
	        sum = (sum & 0xFFFF) + (sum >> 16);

	// Return the one's complement of sum                           //
	return ((uint16_t)(~sum));
}

int main(int argc, char *argv[]) {

	int sock; /* socket descriptor */
	char send_str[MAX_LEN]; /* string to send */
	struct sockaddr_in mc_addr; /* socket address structure */
	unsigned int send_len; /* length of string to send */
	char* mc_addr_str; /* multicast IP address */
	unsigned short mc_port; /* multicast port */
	unsigned char mc_ttl = 32; /* time to live (hop count) */

	/* validate number of arguments */
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <Multicast IP> <Multicast Port>\n", argv[0]);
		exit(1);
	}

	mc_addr_str = argv[1]; /* arg 1: multicast IP address */
	mc_port     = atoi(argv[2]); /* arg 2: multicast port number */

	/* validate the port range */
	if ((mc_port < MIN_PORT) || (mc_port > MAX_PORT)) {
		fprintf(stderr, "Invalid port number argument %d.\n", mc_port);
		fprintf(stderr, "Valid range is between %d and %d.\n", MIN_PORT, MAX_PORT);
		exit(1);
	}

	/* create a socket for sending to the multicast address */
	if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
		perror("socket() failed");
		exit(1);
	}
  
	/* set the TTL (time to live/hop count) for the send */
	if ((setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (void*) &mc_ttl, sizeof(mc_ttl))) < 0) {
		perror("setsockopt() failed");
		exit(1);
	} 
  
	/* construct a multicast address structure */
	memset(&mc_addr, 0, sizeof(mc_addr));
	mc_addr.sin_family      = AF_INET;
	mc_addr.sin_addr.s_addr = inet_addr(mc_addr_str);
	mc_addr.sin_port        = htons(mc_port);

	/* clear send buffer */
	memset(send_str, 0, sizeof(send_str));

	int number = 0;

	while (number < 100) {
  
		send_len = sprintf(send_str, "%03d\n", ++number);   //    strlen(send_str);

		/* send string to multicast address */
		if((sendto(sock, send_str, send_len, 0, (struct sockaddr *) &mc_addr, sizeof(mc_addr))) != send_len) {
			perror("sendto() sent incorrect number of bytes");
			exit(1);
		}

		/* clear send buffer */
		memset(send_str, 0, sizeof(send_str));
		usleep(50000);
	}

	close(sock);  

	exit(0);
}
