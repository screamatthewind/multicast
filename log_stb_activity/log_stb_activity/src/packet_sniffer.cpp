#include<errno.h>
#include<netdb.h>
#include<stdio.h> //For standard things
#include<stdlib.h>    //malloc
#include<string.h>    //strlen

#include<netinet/ip_icmp.h>   //Provides declarations for icmp header
#include<netinet/udp.h>       //Provides declarations for udp header
#include<netinet/tcp.h>       //Provides declarations for tcp header
#include<netinet/ip.h>        //Provides declarations for ip header
#include<netinet/in.h>
#include<netinet/igmp.h>
#include<netinet/if_ether.h>  //For ETH_P_ALL
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include<linux/if.h>
#include<net/ethernet.h>      //For ether_header
#include<sys/socket.h>
#include<arpa/inet.h>
#include<sys/ioctl.h>
#include<sys/time.h>
#include<sys/types.h>
#include<unistd.h>

#include "defs.h"
#include "tools.h"
#include "yuarel.h"
#include "pim.h"

bool ProcessPacket(unsigned char*, int);

#define MAX_DATA_WAIT_MS 60 * 1000

#define IGMP_MEMBERSHIP_QUERY       0x11
#define IGMP_V2_MEMBERSHIP_REPORT   0x16
#define IGMP_V2_LEAVE_GROUP         0x17
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

#define RECEIVE_BUFFER_SIZE 65536

struct ethhdr *eth = NULL;
struct iphdr *iph = NULL;
unsigned short iphdrlen = 0;

struct sockaddr_in igmp_source, igmp_dest, igmp_group;
struct sockaddr_in udp_source, udp_dest;

bool gotRequest = false;
bool gotMembershipReport = false;
bool gotData = false;

cTimeMs *m_dataTimer = NULL;
cTimeMs *m_timeoutTimer = NULL;

int sock_raw = 0;
uint64_t m_byte_ctr = 0;
int data_size = 0;
                       
unsigned char *buffer = NULL;

stb_data_t *m_stb_data;
int m_port;

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

#define LINESZ 1024

extern char timestamp[255];
extern char msg_buf[1024];

char ss1[255];
char ss2[255];
char ss3[255];

struct in_addr s1, s2, s3;

void log_detail(char *message)
{
	FILE *logFile = NULL;

	char hostname[255];
	char filename[LINESZ];
	time_t now = time(NULL);

	gethostname(hostname, LINESZ);

	strftime(timestamp, LINESZ, "%m%d%Y", localtime(&now));
	sprintf(filename, "/var/log/%s-log_stb_activity-detail-%s.log", hostname, timestamp);

	logFile = fopen(filename, "a");
	
	GetTimestamp((char *) &timestamp);
	fprintf(logFile, "%s %s\n", timestamp, message);
	
	if (logFile != NULL)
	{
		fflush(logFile);
		fclose(logFile);
		logFile = NULL;
	}
	
	// printf("%s\n", message);
}

static void clean_up()
{
	if (buffer != NULL)
	{
		free(buffer);
		buffer = NULL;
	}

	if (sock_raw > 0)
		close(sock_raw);
    
	if (m_dataTimer)
	{
		delete m_dataTimer;
		m_dataTimer = NULL;
	}
    
	if (m_timeoutTimer)
	{
		delete m_timeoutTimer;
		m_timeoutTimer = NULL;
	}
}

bool watch_for_changes(stb_data_t *stb_data, int port) 
{
	struct sockaddr saddr;
    
	m_stb_data = stb_data;
	m_port = port;
    
	m_stb_data->status = -2;                      // default is Timed Out
	m_stb_data->num_stats = 0;
	m_stb_data->group_ip = 0;

	for (int i = 0; i < MAX_STB_DATA_STATS; i++)
	{
		m_stb_data->stats[i].byte_ctr = 0;
		m_stb_data->stats[i].elapsed_ms = 0;
		m_stb_data->stats[i].entry_type = 0;
	}
	
	gotRequest = false;
	gotMembershipReport = false;
	gotData = false;

	if (!m_stb_data->get_play)
		gotRequest = true;
	
	igmp_source.sin_addr.s_addr = 0;
	igmp_dest.sin_addr.s_addr = 0;
	igmp_group.sin_addr.s_addr = 0;

	udp_source.sin_addr.s_addr = 0;
	udp_dest.sin_addr.s_addr = 0;
    
	m_dataTimer = new cTimeMs();
	m_timeoutTimer = new cTimeMs();

	// we previosly received a new request, but had not gotten any data
	// this section recovers from that
	if(m_stb_data->new_group_ip != 0)
	{
		// fprintf(stderr, "recovering from timeout\n");
		// fflush(stderr);
		
		m_stb_data->group_ip = m_stb_data->new_group_ip;
		m_stb_data->new_group_ip = 0;
        
		m_dataTimer->Set();
		m_timeoutTimer->Set(MAX_DATA_WAIT_MS);

		if (m_stb_data->num_stats < MAX_STB_DATA_STATS)
		{
			m_stb_data->stats[m_stb_data->num_stats].elapsed_ms = m_dataTimer->Elapsed();
			m_stb_data->stats[m_stb_data->num_stats].entry_type = m_stb_data->new_entry_type;
			m_stb_data->stats[m_stb_data->num_stats].byte_ctr = m_byte_ctr;
			m_byte_ctr = 0;
                    
			m_stb_data->num_stats++;
		}
        
		// fprintf(stderr, "watch_for_changes: gotRequest=true\n");
		gotRequest = true;
        
		if (m_stb_data->new_entry_type == entry_type_membership_report)
			gotMembershipReport = true;
		
		s1.s_addr = m_stb_data->group_ip;
		strcpy(ss1, inet_ntoa(s1));
		sprintf(msg_buf, "Recovering: group %s", ss1);
		log_detail((char *) &msg_buf);
	}

	// if (buffer == NULL)
	buffer = (unsigned char *) malloc(RECEIVE_BUFFER_SIZE);    
    
	sock_raw = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sock_raw < 0) {
		perror("Socket Error");
		m_stb_data->status = -1;
		return false;
	}

	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strcpy((char *) ifr.ifr_name, m_stb_data->if_name);
	int err = ioctl(sock_raw, SIOCGIFINDEX, &ifr);
	if (err < 0) {
		perror("SIOCGIFINDEX");
		return EXIT_FAILURE;
	}
	
	struct sockaddr_ll sll;
	memset(&sll, 0, sizeof(sll));
	sll.sll_family = AF_PACKET;
	sll.sll_ifindex = ifr.ifr_ifindex;
	sll.sll_protocol = htons(ETH_P_ALL);
	err = bind(sock_raw, (struct sockaddr *) &sll, sizeof(sll));
	if (err < 0) {
		perror("bind");
		return EXIT_FAILURE;
	}

	struct packet_mreq      mr;
	memset(&mr, 0, sizeof(mr));
	mr.mr_ifindex = ifr.ifr_ifindex;
	mr.mr_type = PACKET_MR_PROMISC;
	err = setsockopt(sock_raw, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr));
	if (err < 0) {
		perror("PACKET_MR_PROMISC");
		return EXIT_FAILURE;
	}

// -----------------------------------------
	
//	struct ifreq ifopts;	
//	memset(&ifopts, 0, sizeof(struct ifreq));
//	strncpy(ifopts.ifr_name, m_stb_data->if_name, IFNAMSIZ - 1);
//	ioctl(sock_raw, SIOCGIFFLAGS, &ifopts);
//	
//	ifopts.ifr_flags |= IFF_PROMISC;
//	ioctl(sock_raw, SIOCSIFFLAGS, &ifopts);
//
//	/* Allow the socket to be reused - incase connection is closed prematurely */
//	int sockopt;
//	if (setsockopt(sock_raw, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof sockopt) == -1) {
//		perror("setsockopt");
//		close(sock_raw);
//		exit(EXIT_FAILURE);
//	}
//	
//	/* Bind to device */
//	if (setsockopt(sock_raw, SOL_SOCKET, SO_BINDTODEVICE, m_stb_data->if_name, IFNAMSIZ - 1) == -1) {
//		perror("SO_BINDTODEVICE");
//		close(sock_raw);
//		exit(EXIT_FAILURE);
//	}
	
	fd_set readset;
	struct timeval tv;
	int result;

	while (1)
	{
		if (m_stb_data->get_play) {
			if (gotRequest)
			{
				if (m_timeoutTimer->TimedOut())
				{
					clean_up();

					m_stb_data->status = -2;
					return false;
				}
			}
		}
		else
		{
			if (gotMembershipReport)
			{
				if (m_timeoutTimer->TimedOut())
				{
					clean_up();

					m_stb_data->status = -2;
					return false;
				}
			}
		}
        
		FD_ZERO(&readset);
		FD_SET(sock_raw, &readset);
    
		tv.tv_sec = 0;
		tv.tv_usec = 100000;
        
		result = select(sock_raw + 1, &readset, NULL, NULL, &tv);
		if (result == -1)
		{
			clean_up();
            
			m_stb_data->status = -5;
			return false;
		}
		else if (result == 0)
			continue;
        
		if (FD_ISSET(sock_raw, &readset)) {
			data_size = recv(sock_raw, (void*) buffer, RECEIVE_BUFFER_SIZE, 0);
			if (data_size < 0)
			{
				perror("Error during recv");
				// fprintf(stderr, "Error %d during recv\n", errno);
				// fflush(stderr);
				continue;
			}
			
			if (data_size == 0) {
				clean_up();
                   
				m_stb_data->status = -3;
				return false;
			}
          
			if (!m_stb_data->my_bytes)
				m_byte_ctr += data_size;
            
			if (!ProcessPacket(buffer, data_size))
			{
				m_stb_data->status = -2;             // timed out
				break;
			}
        
			if (gotRequest && gotMembershipReport && gotData)
			{
				// fprintf(stderr, "watch_for_changes: gotRequest && gotMembershipReport && gotData\n");
				m_stb_data->status = 0;
				break;
			}
		}
	}
    
	clean_up();

	return true;
}

bool handle_group_report(in_addr_t s_addr)
{				
	s1.s_addr = iph->saddr;
	s2.s_addr = iph->daddr;
	s3.s_addr = s_addr;
			
	strcpy(ss1, inet_ntoa(s1));
	strcpy(ss2, inet_ntoa(s2));
	strcpy(ss3, inet_ntoa(s3));
			
	// printf("handle_group_report: src %-15s dst %-15s grp %-15s\n", ss1, ss2, ss3);
	
	if(gotRequest)
	{
		if (m_stb_data->group_ip == 0) {

			m_byte_ctr = 0;
			m_stb_data->num_stats = 0;
			m_stb_data->group_ip = s_addr;
								
			m_dataTimer->Set();
			m_timeoutTimer->Set(MAX_DATA_WAIT_MS);
		}	
		
		if (m_stb_data->group_ip == s_addr)
		{
			igmp_source.sin_addr.s_addr = iph->saddr;
			igmp_dest.sin_addr.s_addr = iph->daddr;
			igmp_group.sin_addr.s_addr = s_addr;

			m_stb_data->group_ip = s_addr;
            
			if (gotRequest && (m_stb_data->num_stats < MAX_STB_DATA_STATS))
			{
				m_stb_data->stats[m_stb_data->num_stats].elapsed_ms = m_dataTimer->Elapsed();
				m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_membership_report;
				m_stb_data->stats[m_stb_data->num_stats].byte_ctr = m_byte_ctr;
				m_byte_ctr = 0;
                    
				m_stb_data->num_stats++;
			}    

			gotMembershipReport = true;
			gotData = false;

			s1.s_addr = m_stb_data->group_ip;
			strcpy(ss1, inet_ntoa(s1));
			sprintf(msg_buf, "Join: group %s", ss1);
			log_detail((char *) &msg_buf);
		}
		else
		{
			m_stb_data->new_group_ip = s_addr;
			m_stb_data->new_entry_type = entry_type_membership_report;

			// fprintf(stderr, "handle_group_report: got change\n");
                
			return false;
		}
	}

	return true;
}

bool handle_igmp(unsigned char* igmp_recv_buf, int recvlen)
{
	struct igmp *igmp = (struct igmp *)(igmp_recv_buf + iphdrlen  +  sizeof(struct ethhdr));
	struct igmpv3_report *report;
	struct igmpv3_grec *record;
	struct in_addr  rec_group;
	int num_groups, rec_type;

	switch (igmp->igmp_type)
	{
	case IGMP_MEMBERSHIP_QUERY:
		if (gotRequest && (m_stb_data->num_stats < MAX_STB_DATA_STATS))
		{
			m_stb_data->stats[m_stb_data->num_stats].elapsed_ms = m_dataTimer->Elapsed();
			m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_membership_query;
			m_stb_data->stats[m_stb_data->num_stats].byte_ctr = m_byte_ctr;
			m_byte_ctr = 0;

			m_stb_data->num_stats++;

		}    
		
		// log_detail((char *) & "Query");

		return true;
		break;

	case IGMP_V2_MEMBERSHIP_REPORT:

		return handle_group_report(igmp->igmp_group.s_addr);
		break;
        
	case IGMP_V3_MEMBERSHIP_REPORT:
        
		report = (struct igmpv3_report *) igmp;
		num_groups = ntohs(report->ngrec);
		
		// if (num_groups > 1)
		//	printf("IGMP_V3_MEMBERSHIP_REPORT: Too many groups\n");

		record = &report->grec[0];
		rec_type = record->grec_type;
		rec_group.s_addr = (in_addr_t)record->grec_mca;

		switch (rec_type) {
		case IGMP_MODE_IS_EXCLUDE:
			handle_group_report(rec_group.s_addr);
			break;

		case IGMP_CHANGE_TO_EXCLUDE_MODE:
			handle_group_report(rec_group.s_addr);
			break;
		}
		
	case IGMP_V2_LEAVE_GROUP:
		if (m_stb_data->num_stats < MAX_STB_DATA_STATS) {
			m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_leave_group;
			m_stb_data->stats[m_stb_data->num_stats].elapsed_ms = m_dataTimer->Elapsed();
			m_stb_data->stats[m_stb_data->num_stats].byte_ctr = m_byte_ctr;
			m_byte_ctr = 0;
                    
			m_stb_data->num_stats++;
		}
		
		// if (m_stb_data->group_ip == igmp->igmp_group.s_addr)
		s1.s_addr = m_stb_data->group_ip;
		strcpy(ss1, inet_ntoa(s1));
		sprintf(msg_buf, "Leave: group %s", ss1);
		log_detail((char *) &msg_buf);
		
		return true;
		break;
        
	default:
		printf("Unknown IGMP Type: %d\n", igmp->igmp_type);
		return true;
	}

	return true;
}

bool handle_udp(unsigned char *Buffer, int Size)
{
	uint16_t source_port, dest_port;
    
	udp_source.sin_addr.s_addr = iph->saddr;
	udp_dest.sin_addr.s_addr = iph->daddr;
    
	struct udphdr *udph = (struct udphdr*)(Buffer + iphdrlen  + sizeof(struct ethhdr));
    
	source_port = ntohs(udph->source);
	dest_port = ntohs(udph->dest);
    
	//	if (gotMembershipReport)
	//		printf("got it\n");
	
		if((gotMembershipReport) && (udp_dest.sin_addr.s_addr == igmp_dest.sin_addr.s_addr) && (dest_port == m_port)) {

		if (gotData == false)
		{
			if (m_stb_data->num_stats < MAX_STB_DATA_STATS)
			{
				m_stb_data->stats[m_stb_data->num_stats].elapsed_ms = m_dataTimer->Elapsed();
				m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_udp;
				m_stb_data->stats[m_stb_data->num_stats].byte_ctr = m_byte_ctr;
				m_byte_ctr = 0;
                    
				m_stb_data->num_stats++;
			}
            
			gotData = true;

			s1.s_addr = m_stb_data->group_ip;
			strcpy(ss1, inet_ntoa(s1));
			sprintf(msg_buf, "Data: group %s", ss1);
			log_detail((char *) &msg_buf);
		}
	}
    
	return true;
}

bool handle_tcp_packet(unsigned char* Buffer, int Size)
{
	struct tcphdr *tcph = (struct tcphdr *)(Buffer + iphdrlen + sizeof(struct ethhdr));
	int header_size =  sizeof(struct ethhdr) + iphdrlen + tcph->doff * 4;

	unsigned char *data = Buffer + header_size;
	int data_len = Size - header_size;
    
	data[data_len] = 0;

	if (!gotMembershipReport)
	{
		igmp_group.sin_addr.s_addr = iph->daddr;
        
		struct sockaddr_in src, dst;

		memset(&src, 0, sizeof(src));
		src.sin_addr.s_addr = iph->saddr;
    
		memset(&dst, 0, sizeof(dst));
		dst.sin_addr.s_addr = iph->daddr;
        
		uint16_t src_port, dst_port;
        
		src_port = ntohs(iph->saddr);
		dst_port = ntohs(iph->daddr);
        
		int entry_type = entry_type_unknown_request;
        
		if ((src.sin_addr.s_addr == m_stb_data->portal_ip) || (dst.sin_addr.s_addr == m_stb_data->portal_ip))
		{
			if (strncmp((const char *) data, "GET", 3) == 0)
			{
				char temp_query[1024];
				temp_query[0] = 0;

				get_url_query((char *) data, (char *) &temp_query);

				// fprintf(stderr, "query %s\n", temp_query);
				//				
				// if (strstr(temp_query, "real_action=play") != NULL)
				//     printf("got it\n");
				
				char url[MAX_URL_PARAM_SIZE];
				if (get_play_req_url((char *) data, (char *) &url))
				{
					struct yuarel parsed_url[255];
					if (yuarel_parse((struct yuarel*) parsed_url, (char *) &url) == 0)
					{
						if ((m_stb_data->group_ip != 0) && (!gotMembershipReport || !gotData))
						{
							in_addr_t temp_ip;
							inet_pton(AF_INET, parsed_url->host, (void *) &temp_ip);

							// got new host, but no data -- trow a time out
							if(temp_ip != m_stb_data->group_ip)
							{
								m_stb_data->new_group_ip = temp_ip;
								m_stb_data->new_entry_type =  entry_type;
                            
								s1.s_addr = m_stb_data->new_group_ip;
								strcpy(ss1, inet_ntoa(s1));
								sprintf(msg_buf, "Request - Group Changed: group %s", ss1);
								log_detail((char *) &msg_buf);
								
								return false;
							}
							
							else
							{
								if (!gotRequest) {
									gotRequest = true;
	
									m_byte_ctr = 0;
									m_stb_data->num_stats = 0;
									m_stb_data->group_ip = temp_ip;
								
									m_dataTimer->Set();
									m_timeoutTimer->Set(MAX_DATA_WAIT_MS);

									s1.s_addr = m_stb_data->group_ip;
									strcpy(ss1, inet_ntoa(s1));
									sprintf(msg_buf, "Request 1: group %s", ss1);
									log_detail((char *) &msg_buf);
								}
							}
						}
						
						else if (m_stb_data->group_ip == 0)
						{
							if (!gotRequest) {
								gotRequest = true;
								
								m_byte_ctr = 0;
								m_stb_data->num_stats = 0;

								m_dataTimer->Set();
								m_timeoutTimer->Set(MAX_DATA_WAIT_MS);

								inet_pton(AF_INET, parsed_url->host, (void *) &m_stb_data->group_ip);
							
								char temp_query[1024];
								get_url_query((char *) data, (char *) &temp_query);

								s1.s_addr = m_stb_data->group_ip;
								strcpy(ss1, inet_ntoa(s1));
								sprintf(msg_buf, "Request 2: group %s", ss1);
								log_detail((char *) &msg_buf);
							}
						}
					}
				}

				char action[MAX_URL_PARAM_SIZE];
				action[0] = 0;
                
				if (get_real_action((char *) data, (char *) &action)) {
					if (strcmp(action, "play") == 0) 
						entry_type = entry_type_play_request;
                    
					else if (strcmp(action, "stop") == 0)
						entry_type = entry_type_stop_request;	
                    
					else if (strcmp(action, "log") == 0)
						entry_type = entry_type_log_request;

					else if (strcmp(action, "set_last_id") == 0)
						entry_type = entry_type_set_last_id_request;

					else
						entry_type = entry_type_unknown_request;
				}
                
				else if (get_action((char *) data, (char *) &action)) {
					
					if (strlen(action) == 0)
						fprintf(stderr, "no action: query=%s\n", temp_query);
					
					if (strcmp(action, "play") == 0) 
						entry_type = entry_type_play_request;
                    
					else if (strcmp(action, "stop") == 0)
						entry_type = entry_type_stop_request;
                    
					else if (strcmp(action, "log") == 0)
						entry_type = entry_type_log_request;

					else if (strcmp(action, "set_last_id") == 0)
						entry_type = entry_type_set_last_id_request;

					else if (strcmp(action, "set_get_events") == 0)
						entry_type = entry_type_set_events_request;

					else if (strcmp(action, "get_events") == 0)
						entry_type = entry_type_get_events_request;

					else if (strcmp(action, "get_current") == 0)
						entry_type = entry_type_get_current_request;

					else
					{
						entry_type = entry_type_unknown_request;
						printf("Unknown action: %s\n", action);
					}
				}
            
				if (m_stb_data->num_stats < MAX_STB_DATA_STATS)
				{
					if ((entry_type == 0) || (entry_type == entry_type_unknown_request))
					{
						if (strstr((const char *) data, "logo") != NULL)
							entry_type = entry_type_get_logo_request;
						else
						{
							entry_type = entry_type_unknown_request;
							data[50] = 0;
							printf("Unknown request: %s\n", data);
						}
					}
                    
					if (m_stb_data->num_stats < MAX_STB_DATA_STATS) {
						m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type;
						m_stb_data->stats[m_stb_data->num_stats].elapsed_ms = m_dataTimer->Elapsed();
						m_stb_data->stats[m_stb_data->num_stats].byte_ctr = m_byte_ctr;
						m_byte_ctr = 0;
                    
						m_stb_data->num_stats++;
					}
				}
				
				if (entry_type == entry_type_unknown_request)
					fprintf(stderr, "action=%s, query=%s\n", action, temp_query);
				
				switch (entry_type)
				{
				case entry_type_play_request:
						s1.s_addr = m_stb_data->group_ip;
						strcpy(ss1, inet_ntoa(s1));
						sprintf(msg_buf, "Play 3: group %s", ss1);
						log_detail((char *) &msg_buf);
					break;
				
				case entry_type_stop_request:
					log_detail((char *) & "Stop");
					break;
				
				case entry_type_log_request:
					log_detail((char *) & "Log");
					break;
				
				case entry_type_get_logo_request:
					log_detail((char *) & "Logo");
					break;
				
				case entry_type_set_last_id_request:
					log_detail((char *) & "Set Last Id");
					break;
				
				case entry_type_set_events_request:
					log_detail((char *) & "Set Events");
					break;
				
				case entry_type_get_events_request:
					log_detail((char *) & "Get Events");
					break;
				
				case entry_type_get_current_request:
					log_detail((char *) & "Get Current");
					break;
				
				case entry_type_unknown_request:
					log_detail((char *) & "Unknown");
					break;
				}
			}
            
			else if (strncmp((const char *) data, "HTTP", 4) == 0)
			{
				if (m_stb_data->num_stats < MAX_STB_DATA_STATS)
				{
					m_stb_data->stats[m_stb_data->num_stats].elapsed_ms = m_dataTimer->Elapsed();
					m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_response;
					m_stb_data->stats[m_stb_data->num_stats].byte_ctr = m_byte_ctr;
					m_byte_ctr = 0;
                    
					m_stb_data->num_stats++;
				}
				
				log_detail((char *) &"Response");
			}
    
			else
			{
				if (tcph->ack)
				{
					if (m_stb_data->num_stats < MAX_STB_DATA_STATS)
					{
						m_stb_data->stats[m_stb_data->num_stats].elapsed_ms = m_dataTimer->Elapsed();
						m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_ack;
						m_stb_data->stats[m_stb_data->num_stats].byte_ctr = m_byte_ctr;
						m_byte_ctr = 0;
                    
						m_stb_data->num_stats++;
					}
				}
                
				else if (tcph->syn)
				{
					if (m_stb_data->num_stats < MAX_STB_DATA_STATS)
					{
						m_stb_data->stats[m_stb_data->num_stats].elapsed_ms = m_dataTimer->Elapsed();
						m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_syn;
						m_stb_data->stats[m_stb_data->num_stats].byte_ctr = m_byte_ctr;
						m_byte_ctr = 0;
                    
						m_stb_data->num_stats++;
					}
				}

				else if (m_stb_data->num_stats < MAX_STB_DATA_STATS)
				{
					m_stb_data->stats[m_stb_data->num_stats].elapsed_ms = m_dataTimer->Elapsed();
					m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_other;
					m_stb_data->stats[m_stb_data->num_stats].byte_ctr = m_byte_ctr;
					m_byte_ctr = 0;
                    
					m_stb_data->num_stats++;
				}
			}
		}
	}
    
	return true;
}

struct pim {
#ifdef _PIM_VT
	uint8_t         pim_vt; /* PIM version and message type */
#else /* ! _PIM_VT   */
#if BYTE_ORDER == BIG_ENDIAN
	u_int           pim_vers : 4, /* PIM protocol version         */
	                pim_type : 4; /* PIM message type             */
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
	u_int           pim_type : 4, /* PIM message type             */
	                pim_vers : 4; /* PIM protocol version         */
#endif
#endif /* ! _PIM_VT  */
	uint8_t         pim_reserved; /* Reserved                     */
	uint16_t        pim_cksum; /* IP-style checksum            */
};

bool handle_pim(unsigned char *buffer, int size)
{
	struct pim *pim_hdr = (struct pim *)(buffer + iphdrlen + sizeof(struct ethhdr));

	//	uint8_t *pim_msg = buffer + iphdrlen;
	//	int pim_msg_len = size - iphdrlen;;
	//	int pim_type = PIM_MSG_HDR_GET_TYPE(pim_msg);

	// printf("handle_pim: pim_type %d\n", pim_hdr->pim_type);
	
	return true;
}

bool ProcessPacket(unsigned char* buffer, int size)
{
	eth = (struct ethhdr *)buffer;
	iph = (struct iphdr *)(buffer  + sizeof(struct ethhdr));
	iphdrlen = iph->ihl * 4;

	bool result = true;

	if (m_stb_data->ip_address != 0)
	{
		if ((iph->daddr != m_stb_data->ip_address) && (iph->saddr != m_stb_data->ip_address) && (iph->daddr != m_stb_data->group_ip) && (iph->saddr != m_stb_data->group_ip))
		{
			//			struct in_addr s1, s2, s3;
			//			
			//			s1.s_addr = iph->saddr;
			//			s2.s_addr = iph->daddr;
			//			s3.s_addr = m_stb_data->group_ip;
			//			
			//			char ss1[255];
			//			char ss2[255];
			//			char ss3[255];
			//
			//			strcpy(ss1, inet_ntoa(s1));
			//			strcpy(ss2, inet_ntoa(s2));
			//			strcpy(ss3, inet_ntoa(s3));
			//			
			//			printf("src %-15s dst %-15s grp %-15s\n", ss1, ss2, ss3);

						return result;
		}
	}

	//	unsigned char eth_src[255];
	//	unsigned char eth_dst[255];
	//	
	//	eth2text((unsigned char *) eth->h_source, (unsigned char *) &eth_src);
	//	eth2text((unsigned char *) eth->h_dest, (unsigned char *) &eth_dst);
	//
	//	struct in_addr s1, s2;
	//			
	//	s1.s_addr = iph->saddr;
	//	s2.s_addr = iph->daddr;
	//			
	//	char ss1[255];
	//	char ss2[255];
	//	char ss3[255];
	//
	//	strcpy(ss1, inet_ntoa(s1));
	//	strcpy(ss2, inet_ntoa(s2));
	//
	//	printf("%s(%-16s) %s(%-16s)\n", eth_src, ss1, eth_dst, ss2);
	
		if(m_stb_data->my_bytes)
			m_byte_ctr += data_size;
	
	switch (iph->protocol) //Check the Protocol and do accordingly...
		{
		case 1:  //ICMP Protocol
			// printf("ProcessPacket: ICMP Protocol\n");
		    break;
        
		case 2:  //IGMP Protocol
		    result = handle_igmp(buffer, size);
			break;
        
		case 6:  //TCP Protocol
		    result = handle_tcp_packet(buffer, size);
			break;
        
		case 17: //UDP Protocol
		    result = handle_udp(buffer, size);
			break;
			
		case 89: // OSPFIGP Protocol
			// printf("ProcessPacket: Protocol Protocol\n");
			break;
			
		case 103: // PIM Protocol
			result = handle_pim(buffer, size);
			break;
        
		default: //Some Other Protocol like ARP etc.
			// printf("ProcessPacket: Uknown IP Protocol %d\n", iph->protocol);
			break;
		}
    
	return result;
}

