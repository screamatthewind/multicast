#ifndef DEFS_H
#define DEFS_H

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

typedef struct parms_t {
	int num_streams;
	int start_nibble;
	string base_url;
	string ip_address;
	int port;
	char if_name[32];
	string channel_list_filename;
	string timeout_exec_filename;
	int channel_change_delay;
	bool run_once;
} parms_t;


#endif
