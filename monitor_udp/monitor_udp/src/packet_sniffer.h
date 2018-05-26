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

typedef struct ps_udp_info_t {
	LIST_ENTRY(ps_udp_info_t) link;
	
	uint32_t source;
	uint32_t dest;
	timeval start_time;
	timeval stop_time;
	timeval last_data_time;
	uint32_t bytes;	
	bool closed;
} ps_udp_info_t;

typedef struct ps_interface_t {
	LIST_ENTRY(ps_interface_t) link;
	LIST_HEAD(, ps_udp_info_t) udp_info;
	
	uint32_t ip_address;
	char     if_name[32];   
} jpi_mrt_channel_t;

bool monitor_udp(char *if_name);
bool is_valid_interface(char *if_name);
void list_interfaces();

extern unsigned char *buffer;
extern int sock_raw;

#endif