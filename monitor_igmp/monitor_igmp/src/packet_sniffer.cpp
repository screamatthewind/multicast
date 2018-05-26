#include "packet_sniffer.h"
#include "time.h"
#include "tools.h"

static LIST_HEAD(, ps_interface_t) ps_interfaces = LIST_HEAD_INITIALIZER();

#define RECEIVE_BUFFER_SIZE 65536
#define LINESZ 1024

struct ethhdr *eth = NULL;
struct iphdr *iph = NULL;
unsigned short iphdrlen = 0;
char *m_if_name;

char ss1[255];
char ss2[255];
char ss3[255];

struct in_addr s1, s2, s3;

char timestamp[255];
char msg_buf[1024];

int sock_raw = 0;                  
unsigned char *buffer = NULL;

bool ProcessPacket(unsigned char*, int);
extern void clean_up();

void igmp_clean_up()
{
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strcpy((char *) ifr.ifr_name, m_if_name);
	int err = ioctl(sock_raw, SIOCGIFINDEX, &ifr);
	if (err < 0) {
		perror("SIOCGIFINDEX");
	}

	struct packet_mreq      mr;
	memset(&mr, 0, sizeof(mr));
	mr.mr_ifindex = ifr.ifr_ifindex;
	mr.mr_type = PACKET_MR_PROMISC;
	err = setsockopt(sock_raw, SOL_PACKET, PACKET_DROP_MEMBERSHIP, &mr, sizeof(mr));
	if (err < 0) {
		perror("PACKET_MR_PROMISC");
	}
	
	close(sock_raw);
}

void log_message(char *message)
{
	FILE *logFile = NULL;

	char hostname[LINESZ];
	char filename[LINESZ];
	time_t now = time(NULL);

	gethostname(hostname, LINESZ);

	strftime(timestamp, LINESZ, "%m%d%Y", localtime(&now));
	sprintf(filename, "/var/log/%s-%s-monitor_igmp-%s.log", hostname, m_if_name, timestamp);

	logFile = fopen(filename, "a");
	
	GetTimestamp((char *) &timestamp);
	fprintf(logFile, "%s %s\n", timestamp, message);
	
	if (logFile != NULL)
	{
		fflush(logFile);
		fclose(logFile);
		logFile = NULL;
	}
	
	printf("%s\n", message);
}

void get_interfaces()
{
	struct ifaddrs *interfaces = NULL;
	ps_interface_t *ps_interface, *ps_interface_tmp;
	bool found_interface;	
	
	int success = getifaddrs(&interfaces);
	if (success == 0) {

		while (interfaces != NULL) {

			if ((interfaces->ifa_addr) && (interfaces->ifa_addr->sa_family == AF_INET))
			{			
				found_interface = false;
				LIST_FOREACH_SAFE(ps_interface, &ps_interfaces, link, ps_interface_tmp) {
					if (strcmp(interfaces->ifa_name, ps_interface->if_name) == 0)
					{
						found_interface = true;
						break;
					}
				}

				if (!found_interface)
				{
					ps_interface_t *new_ps_interface = (ps_interface_t *) malloc(sizeof(ps_interface_t));
				
					if ((interfaces->ifa_addr != 0) && (interfaces->ifa_flags & IFF_LOOPBACK) == 0)
					{
						new_ps_interface->ip_address = ((struct sockaddr_in*)interfaces->ifa_addr)->sin_addr.s_addr;
						strcpy(new_ps_interface->if_name, interfaces->ifa_name);
				
						new_ps_interface->udp_info = LIST_HEAD_INITIALIZER();
						
						LIST_INSERT_HEAD(&ps_interfaces, new_ps_interface, link);	
					}
				}
			}
			
			interfaces = interfaces->ifa_next;
		}
	}

	freeifaddrs(interfaces);
}

bool is_valid_interface(char *if_name)
{
	bool result = false;
	ps_interface_t *ps_interface, *ps_interface_tmp;

	get_interfaces();

	LIST_FOREACH_SAFE(ps_interface, &ps_interfaces, link, ps_interface_tmp) {
		if (strcmp(if_name, ps_interface->if_name) == 0)
		{
			result = true;
			break;
		}
	}

	return result;
}

void list_interfaces()
{
	ps_interface_t *ps_interface, *ps_interface_tmp;

	get_interfaces();

	printf("\nAvailable interfaces:\n");

	LIST_FOREACH_SAFE(ps_interface, &ps_interfaces, link, ps_interface_tmp) {
		s1.s_addr = ps_interface->ip_address;
		printf("  %-8s %s\n", ps_interface->if_name, inet_ntoa(s1));
	}

	printf("\n");
}

bool monitor_igmp(char *if_name)
{
	m_if_name = if_name;
	
	log_message((char *) &"started");
	get_interfaces();
	
	buffer = (unsigned char *) malloc(RECEIVE_BUFFER_SIZE);    

	sock_raw = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sock_raw < 0) {
		perror("Socket Error");
		return false;
	}

	int flag_on = 1; 
	setsockopt(sock_raw, SOL_SOCKET, SO_REUSEADDR, &flag_on, sizeof(flag_on));
		
	struct ifreq ifr;
	memset(&ifr, 0, sizeof(ifr));
	strcpy((char *) ifr.ifr_name, if_name);
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
	
	fd_set readset;
	struct timeval tv;
	int result;

	while (1)
	{
		FD_ZERO(&readset);
		FD_SET(sock_raw, &readset);
    
		tv.tv_sec = 0;
		tv.tv_usec = 100000; // 100 ms
        
		result = select(sock_raw + 1, &readset, NULL, NULL, &tv);
		if (result == -1)
		{
			clean_up();
			return false;
		}
		
		else if (result == 0)
			continue;
        
		if (FD_ISSET(sock_raw, &readset)) {
			int bytes_read = recv(sock_raw, (void*) buffer, RECEIVE_BUFFER_SIZE, 0);
			if (bytes_read < 0)
			{
				perror("Error during recv");
				continue;
			}
			
			if (bytes_read == 0) {
				clean_up();
                   
				return false;
			}
          
			if (!ProcessPacket(buffer, bytes_read))
			{
				break;
			}
		}
	}
	
	return true;
}

#define NELEMS(x)  (sizeof(x) / sizeof((x)[0]))

bool ignore_ip()
{
	uint32_t source = htonl(iph->saddr);
	uint32_t dest = htonl(iph->daddr);
	
	// 239.0.0.0 to 239.255.255.255
	if (((source >= 4009754624) && (source <= 4026531839)) || ((dest >= 4009754624) && (dest <= 4026531839)))
		return false;
	
	return true;
}

bool handle_group_report(in_addr_t group)
{				
	s3.s_addr = group;
	strcpy(ss3, inet_ntoa(s3));

	sprintf(msg_buf, "IGMP_V2_GROUP_REPORT: src %s dst %s grp %s", ss1, ss2, ss3);
	log_message((char *) &msg_buf);

	return true;
}

bool handle_igmp(unsigned char* igmp_recv_buf, int recvlen)
{
	struct igmp *igmp = (struct igmp *)(igmp_recv_buf + iphdrlen  +  sizeof(struct ethhdr));
	struct igmpv3_report *report;
	struct igmpv3_grec *record;
	struct in_addr  rec_group;
	int num_groups, rec_type;

	uint32_t group = igmp->igmp_group.s_addr;
	
	s1.s_addr = iph->saddr;
	s2.s_addr = iph->daddr;
	s3.s_addr = group;
			
	strcpy(ss1, inet_ntoa(s1));
	strcpy(ss2, inet_ntoa(s2));
	strcpy(ss3, inet_ntoa(s3));
	
	switch (igmp->igmp_type)
	{
	case IGMP_MEMBERSHIP_QUERY:
		sprintf(msg_buf, "IGMP_MEMBERSHIP_QUERY: src %s dst %s grp %s", ss1, ss2, ss3);
		log_message((char *) &msg_buf);
		break;

	case IGMP_V2_MEMBERSHIP_REPORT:

		handle_group_report(igmp->igmp_group.s_addr);
		break;
        
	case IGMP_V3_MEMBERSHIP_REPORT:
        
		report = (struct igmpv3_report *) igmp;
		num_groups = ntohs(report->ngrec);

		sprintf(msg_buf, "IGMP_V3_MEMBERSHIP_REPORT: src %s dst %s grp %s groups %d", ss1, ss2, ss3, num_groups);
		log_message((char *) &msg_buf);
		
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
		sprintf(msg_buf, "IGMP_V2_LEAVE_GROUP: src %s dst %s grp %s", ss1, ss2, ss3);
		log_message((char *) &msg_buf);

		return true;
		break;

	case IGMP_PIM:
		sprintf(msg_buf, "IGMP_PIM: src %s dst %s grp %s", ss1, ss2, ss3);
		log_message((char *) &msg_buf);

		return true;
		break;
		
	default:
		printf("Unknown IGMP Type: %d\n", igmp->igmp_type);
		return true;
	}

	return true;
}

bool ProcessPacket(unsigned char* buffer, int size)
{
	eth = (struct ethhdr *)buffer;
	iph = (struct iphdr *)(buffer  + sizeof(struct ethhdr));
	iphdrlen = iph->ihl * 4;

	bool result = true;
	switch (iph->protocol) //Check the Protocol and do accordingly...
	{
		case 1:  //ICMP Protocol
		    break;
        
		case 2:  //IGMP Protocol
			// if (!ignore_ip())
			    handle_igmp(buffer, size);
			break;
        
		case 6:  //TCP Protocol
			break;
        
		case 17: //UDP Protocol
			break;
			
		case 89: // OSPFIGP Protocol
			break;
			
		case 103: // PIM Protocol
			break;
        
		default: //Some Other Protocol like ARP etc.
			// printf("ProcessPacket: Uknown IP Protocol %d\n", iph->protocol);
			break;
	}
    
	return result;
}
