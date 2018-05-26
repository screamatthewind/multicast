#include<netinet/in.h>
#include<errno.h>
#include<netdb.h>
#include<stdio.h> //For standard things
#include<stdlib.h>    //malloc
#include<string.h>    //strlen

#include<netinet/ip_icmp.h>   //Provides declarations for icmp header
#include<netinet/udp.h>       //Provides declarations for udp header
#include<netinet/tcp.h>       //Provides declarations for tcp header
#include<netinet/ip.h>        //Provides declarations for ip header
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
#include <inttypes.h>

#include "defs.h"
#include "tools.h"
#include "yuarel.h"

void ProcessPacket(unsigned char*, int);

#define MAX_DATA_WAIT_MS 60 * 1000

#define IGMP_MEMBERSHIP_QUERY   	0x11
#define IGMP_V3_MEMBERSHIP_REPORT	0x22
#define IGMP_V2_MEMBERSHIP_REPORT	0x16
#define IGMP_V2_LEAVE_GROUP		    0x17

#define RECEIVE_BUFFER_SIZE 65536

struct ethhdr *eth = NULL;
struct iphdr *iph = NULL;
unsigned short iphdrlen = 0;

struct sockaddr_in igmp_source, igmp_dest, igmp_group;
struct sockaddr_in udp_source, udp_dest;

bool gotMembershipReport = false;
bool gotData = false;

cTimeMs *m_timeoutTimer = new cTimeMs();
cTimeMs *m_dataTimer = new cTimeMs();

unsigned char *buffer = NULL;

stb_data_t *m_stb_data;
int sock_raw = 0;

uint64_t m_byte_ctr = 0;

#define LINESZ 1024

static char msg_buf[1024];

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

	static char timestamp[255];
	strftime(timestamp, LINESZ, "%m%d%Y", localtime(&now));
	sprintf(filename, "/var/log/%s-exercise_stb-detail-%s.log", hostname, timestamp);

	logFile = fopen(filename, "a");
	
	GetTimestamp((char *) &timestamp);
	fprintf(logFile, "%s >>>> %s\n", timestamp, message);
	
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
}

bool sniff_packets(stb_data_t *stb_data)
{
	int saddr_size, data_size;
	struct sockaddr saddr;
    
	m_stb_data = stb_data;
	
	for (int i = 0; i < MAX_STB_DATA_STATS; i++)
	{
		m_stb_data->stats[i].byte_ctr = 0;
		m_stb_data->stats[i].elapsed_ms = 0;
		m_stb_data->stats[i].entry_type = 0;
	}

	m_stb_data->status = -2;          // default is Timed Out
	m_stb_data->num_stats = 0;
	m_stb_data->group_ip = 0;
	
	gotMembershipReport = false;
	gotData = false;

	igmp_source.sin_addr.s_addr = 0;
	igmp_dest.sin_addr.s_addr = 0;
	igmp_group.sin_addr.s_addr = 0;

	udp_source.sin_addr.s_addr = 0;
	udp_dest.sin_addr.s_addr = 0;

	if (buffer == NULL)
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
	
	m_dataTimer->Set();
	m_timeoutTimer->Set(MAX_DATA_WAIT_MS);

	fd_set readset;
	struct timeval tv;
	int result;
		
	while (!m_stb_data->monitorTimer->TimedOut()) {

		if (m_timeoutTimer->TimedOut())
		{
			clean_up();

			m_stb_data->status = -2;
			return false;
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
			if (data_size == 0) {
				clean_up();
			
				m_stb_data->status = -3;
				return false;
			}
		}
		
		ProcessPacket(buffer, data_size);
		
		if (gotMembershipReport && gotData)
		{
			m_stb_data->status = 0;
			break;
		}
	}
	
	clean_up();
	
	return true;
}

void eth2text(char *eth_adr, char *text)
{
	sprintf(text, "%.2X-%.2X-%.2X-%.2X-%.2X-%.2X", eth_adr[0], eth_adr[1], eth_adr[2], eth_adr[3], eth_adr[4], eth_adr[5]);
}

void handle_igmp(unsigned char* igmp_recv_buf, int recvlen)
{
	struct igmp *igmp = (struct igmp *)(igmp_recv_buf + iphdrlen  +  sizeof(struct ethhdr));
    
	switch (igmp->igmp_type)
	{
	case IGMP_MEMBERSHIP_QUERY:
		if (m_stb_data->num_stats < MAX_STB_DATA_STATS)
		{
			m_stb_data->stats[m_stb_data->num_stats].elapsed_ms = m_dataTimer->Elapsed();
			m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_membership_query;
			m_stb_data->stats[m_stb_data->num_stats].byte_ctr = m_byte_ctr;
			m_byte_ctr = 0;

			m_stb_data->num_stats++;

		}    
		break;

	case IGMP_V3_MEMBERSHIP_REPORT:
		s1.s_addr = iph->saddr;
		strcpy(ss1, inet_ntoa(s1));

		s2.s_addr = iph->saddr;
		strcpy(ss2, inet_ntoa(s2));
		
		sprintf(msg_buf, "V3 Membership Report: src %s dst %s %" PRIu64 " ms", ss1, ss2, m_dataTimer->Elapsed());
		log_detail((char *) &msg_buf);
		break;

	case IGMP_V2_MEMBERSHIP_REPORT:

		igmp_source.sin_addr.s_addr = iph->saddr;
		igmp_dest.sin_addr.s_addr = iph->daddr;
		igmp_group.sin_addr.s_addr = igmp->igmp_group.s_addr;

		m_stb_data->group_ip = igmp->igmp_group.s_addr;
		
		if(m_stb_data->num_stats < MAX_STB_DATA_STATS)
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
		sprintf(msg_buf, "Send Join Message: group %s %" PRIu64 " ms", ss1, m_dataTimer->Elapsed());
		log_detail((char *) &msg_buf);

		break;

	case IGMP_V2_LEAVE_GROUP:
//		if (m_stb_data->num_stats < MAX_STB_DATA_STATS) {
//			m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_leave_group;
//			m_stb_data->stats[m_stb_data->num_stats].elapsed_ms = m_dataTimer->Elapsed();
//			m_stb_data->stats[m_stb_data->num_stats].byte_ctr = m_byte_ctr;
//			m_byte_ctr = 0;
//                    
//			m_stb_data->num_stats++;
//		}

		strcpy(ss1, inet_ntoa(igmp->igmp_group));
		sprintf(msg_buf, "Send Leave: group %s %" PRIu64 " ms", ss1, m_dataTimer->Elapsed());
		log_detail((char *) &msg_buf);

		break;
		
	default:
		printf("Unknown IGMP Type: %d\n", igmp->igmp_type);
	}
}

void handle_udp(unsigned char *Buffer, int Size)
{
	uint16_t source_port, dest_port;
    
	udp_source.sin_addr.s_addr = iph->saddr;
	udp_dest.sin_addr.s_addr = iph->daddr;

	struct udphdr *udph = (struct udphdr*)(Buffer + iphdrlen  + sizeof(struct ethhdr));
    
	source_port = ntohs(udph->source);
	dest_port = ntohs(udph->dest);
	
	if ((gotMembershipReport) && (udp_dest.sin_addr.s_addr == igmp_dest.sin_addr.s_addr) && (dest_port == m_stb_data->port)) {

		if (gotData == false)
		{
			if(m_stb_data->num_stats < MAX_STB_DATA_STATS)
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
			sprintf(msg_buf, "Got Data: group %s %" PRIu64 " ms", ss1, m_dataTimer->Elapsed());
			log_detail((char *) &msg_buf);
		}
		
		// m_timeoutTimer->Set(MAX_DATA_WAIT_MS);
	}
}

void handle_tcp_packet(unsigned char* Buffer, int Size)
{
	struct tcphdr *tcph = (struct tcphdr *)(Buffer + iphdrlen + sizeof(struct ethhdr));
	int header_size =  sizeof(struct ethhdr) + iphdrlen + tcph->doff * 4;

	unsigned char *data = Buffer + header_size;
	int data_len = Size - header_size;
	
	data[data_len] = 0;

	if (!gotMembershipReport)
	{
		igmp_group.sin_addr.s_addr = iph->daddr;
		
		//		strcpy(s_igmp_group, inet_ntoa(igmp_group.sin_addr));
		//		strcpy(m_stb_data->group_ip, s_igmp_group);
		//		printf("%s\n", s_igmp_group);

		struct sockaddr_in src, dst;

		memset(&src, 0, sizeof(src));
		src.sin_addr.s_addr = iph->saddr;
    
		memset(&dst, 0, sizeof(dst));
		dst.sin_addr.s_addr = iph->daddr;
		
		uint16_t src_port, dst_port;
		
		src_port = ntohs(iph->saddr);
		dst_port = ntohs(iph->daddr);
		
		if ((src.sin_addr.s_addr == m_stb_data->portal_ip) || (dst.sin_addr.s_addr == m_stb_data->portal_ip))
		{
			if (strncmp((const char *) data, "GET", 3) == 0)
			{
				char action[MAX_URL_PARAM_SIZE];
				action[0] = 0;
				
				// printf("GET: %d\n", iph->id);

				if (get_real_action((char *) data, (char *) &action)) {
					if (strcmp(action, "play") == 0) 
						m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_play_request;
					
					else if (strcmp(action, "stop") == 0)
						m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_stop_request;
					
					else if (strcmp(action, "log") == 0)
						m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_log_request;

					else if (strcmp(action, "set_last_id") == 0)
						m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_set_last_id_request;

					else
						m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_unknown_request;
				}
				
				else if (get_action((char *) data, (char *) &action)) {
					if (strcmp(action, "play") == 0) 
						m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_play_request;
					
					else if (strcmp(action, "stop") == 0)
						m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_stop_request;
					
					else if (strcmp(action, "log") == 0)
						m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_log_request;

					else if (strcmp(action, "set_last_id") == 0)
						m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_set_last_id_request;

					else if (strcmp(action, "set_get_events") == 0)
						m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_set_events_request;

					else if (strcmp(action, "get_events") == 0)
						m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_get_events_request;

					else if (strcmp(action, "get_current") == 0)
						m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_get_current_request;

					else
					{
						m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_unknown_request;
						printf("Unknown action: %s\n", action);
					}
				}
				
				if (m_stb_data->num_stats < MAX_STB_DATA_STATS)
				{
					if (m_stb_data->stats[m_stb_data->num_stats].entry_type == 0)
					{
						if (strstr((const char *) data, "logo") != NULL)
							m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_get_logo_request;
						else
						{
							m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_unknown_request;
							data[50] = 0;
							printf("Unknown request: %s\n", data);
						}
					}
					
					m_stb_data->stats[m_stb_data->num_stats].elapsed_ms = m_dataTimer->Elapsed();
					m_stb_data->stats[m_stb_data->num_stats].byte_ctr = m_byte_ctr;
					m_byte_ctr = 0;

					m_stb_data->num_stats++;
				}

				char url[MAX_URL_PARAM_SIZE];
				if (get_play_req_url((char *) data, (char *) &url))
				{
					struct yuarel parsed_url[255];
					if (yuarel_parse((struct yuarel*) parsed_url, (char *) &url) == 0)
						inet_pton(AF_INET, parsed_url->host, (void *) &m_stb_data->group_ip);
				}
				
				switch (m_stb_data->stats[m_stb_data->num_stats-1].entry_type)
				{
				case entry_type_play_request:
						s1.s_addr = m_stb_data->group_ip;
						strcpy(ss1, inet_ntoa(s1));
						sprintf(msg_buf, "Send GET Play: group %s %" PRIu64 " ms", ss1, m_dataTimer->Elapsed());
						log_detail((char *) &msg_buf);
					break;
				
				case entry_type_stop_request:
					sprintf(msg_buf, "Stop %" PRIu64 " ms", m_dataTimer->Elapsed());
					log_detail((char *) &msg_buf);
					break;
				
				case entry_type_log_request:
					sprintf(msg_buf, "Log %" PRIu64 " ms", m_dataTimer->Elapsed());
					log_detail((char *) &msg_buf);
					break;
				
				case entry_type_get_logo_request:
					sprintf(msg_buf, "Logo %" PRIu64 " ms", m_dataTimer->Elapsed());
					log_detail((char *) &msg_buf);
					break;
				
				case entry_type_set_last_id_request:
					sprintf(msg_buf, "Set Last Id %" PRIu64 " ms", m_dataTimer->Elapsed());
					log_detail((char *) &msg_buf);
					break;
				
				case entry_type_set_events_request:
					sprintf(msg_buf, "Set Events %" PRIu64 " ms", m_dataTimer->Elapsed());
					log_detail((char *) &msg_buf);
					break;
				
				case entry_type_get_events_request:
					sprintf(msg_buf, "Get Events %" PRIu64 " ms", m_dataTimer->Elapsed());
					log_detail((char *) &msg_buf);
					break;
				
				case entry_type_get_current_request:
					sprintf(msg_buf, "Get Current %" PRIu64 " ms", m_dataTimer->Elapsed());
					log_detail((char *) &msg_buf);
					break;
				
				case entry_type_unknown_request:
					sprintf(msg_buf, "Unknown %" PRIu64 " ms", m_dataTimer->Elapsed());
					log_detail((char *) &msg_buf);
					break;
				}
			}
			
			else if (strncmp((const char *) data, "HTTP", 4) == 0)
			{
				// printf("HTTP: %d\n", iph->id);
				if (m_stb_data->num_stats < MAX_STB_DATA_STATS)
				{
					m_stb_data->stats[m_stb_data->num_stats].elapsed_ms = m_dataTimer->Elapsed();
					m_stb_data->stats[m_stb_data->num_stats].entry_type = entry_type_response;
					m_stb_data->stats[m_stb_data->num_stats].byte_ctr = m_byte_ctr;
					m_byte_ctr = 0;

					m_stb_data->num_stats++;
				}

				sprintf(msg_buf, "Got Response %" PRIu64 " ms", m_dataTimer->Elapsed());
				log_detail((char *) &msg_buf);
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
}

void ProcessPacket(unsigned char* buffer, int size)
{
	eth = (struct ethhdr *)buffer;
	iph = (struct iphdr *)(buffer  + sizeof(struct ethhdr));
	iphdrlen = iph->ihl * 4;

	switch (iph->protocol) //Check the Protocol and do accordingly...
		{
		case 1:  //ICMP Protocol
			break;
        
		case 2:  //IGMP Protocol
			handle_igmp(buffer, size);
			break;
        
		case 6:  //TCP Protocol
			handle_tcp_packet(buffer, size);
			break;
        
		case 17: //UDP Protocol
			handle_udp(buffer, size);
			break;
        
		default: //Some Other Protocol like ARP etc.
			break;
		}
}

