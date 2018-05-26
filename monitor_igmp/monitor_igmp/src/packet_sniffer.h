#ifndef PACKET_SNIFFER_H
#define PACKET_SNIFFER_H

#include "queue.h"
#include <errno.h>
#include <netdb.h>
#include <stdio.h> //For standard things
#include <stdlib.h>    //malloc
#include <string.h>    //strlen
#include <signal.h>
#include <getopt.h>

#include <ifaddrs.h>
#include <netinet/ip_icmp.h>   //Provides declarations for icmp header
#include <netinet/udp.h>       //Provides declarations for udp header
#include <netinet/tcp.h>       //Provides declarations for tcp header
#include <netinet/ip.h>        //Provides declarations for ip header
#include <netinet/in.h>
#include <netinet/igmp.h>
#include <netinet/if_ether.h>  //For ETH_P_ALL
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/if.h>
#include <net/ethernet.h>      //For ether_header
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define IGMP_V3_MEMBERSHIP_REPORT   0x22

#ifndef IGMP_MODE_IS_INCLUDE
#define IGMP_DO_NOTHING			    0	/* don't send a record */
#define IGMP_MODE_IS_INCLUDE		1	/* MODE_IN */
#define IGMP_MODE_IS_EXCLUDE		2	/* MODE_EX */
#define IGMP_CHANGE_TO_INCLUDE_MODE	3	/* TO_IN */
#define IGMP_CHANGE_TO_EXCLUDE_MODE	4	/* TO_EX */
#define IGMP_ALLOW_NEW_SOURCES		5	/* ALLOW_NEW */
#define IGMP_BLOCK_OLD_SOURCES		6	/* BLOCK_OLD */
#endif

typedef struct ps_interface_t {
	LIST_ENTRY(ps_interface_t) link;
	LIST_HEAD(, ps_udp_info_t) udp_info;
	
	uint32_t ip_address;
	char     if_name[32];   
} jpi_mrt_channel_t;

struct igmpv3_grec {
	uint8_t  grec_type;
	uint8_t  grec_auxwords;
	uint16_t grec_nsrcs;
	uint32_t grec_mca;
	uint32_t grec_src[0];
};

struct igmpv3_report {
	uint8_t  type;
	uint8_t  resv1;
	uint16_t csum;
	uint16_t resv2;
	uint16_t ngrec;
	struct igmpv3_grec grec[0];
};

bool monitor_igmp(char *if_name);
bool is_valid_interface(char *if_name);
void list_interfaces();

extern unsigned char *buffer;
extern int sock_raw;

#endif