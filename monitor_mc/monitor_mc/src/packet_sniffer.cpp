#include "packet_sniffer.h"
#include "time.h"
#include "tools.h"
#include "pim.h"

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
char tmp_buf[1024];

int sock_raw = 0;                  
unsigned char *buffer = NULL;
bool dump_first_line_only = true;

timeval now;
double cur_time, start_time, last_data_time, elapsed_time;
double last_udp_check_time = 0;

bool ProcessPacket(unsigned char*, int);
void check_udp_entries();
void delete_interfaces();

extern void clean_up();

void igmp_clean_up()
{
	if (strcmp(m_if_name, "any") != 0)
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
	}
	
	delete_interfaces();
	
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
	sprintf(filename, "/var/log/%s-%s-monitor_mc-%s.log", hostname, m_if_name, timestamp);

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

void delete_interfaces()
{
	ps_interface_t *ps_interface, *ps_interface_tmp;
	ps_udp_info_t *ps_udp_info, *ps_udp_info_tmp;

	LIST_FOREACH_SAFE(ps_interface, &ps_interfaces, link, ps_interface_tmp) {

		LIST_FOREACH_SAFE(ps_udp_info, &ps_interface->udp_info, link, ps_udp_info_tmp) {
			LIST_REMOVE(ps_udp_info, link);
			free(ps_udp_info);
			ps_udp_info = NULL;			
		}

		LIST_REMOVE(ps_interface, link);
		free(ps_interface);
		ps_interface = NULL;			
	}	
}

void get_interfaces()
{
	struct ifaddrs *interfaces = NULL;
	ps_interface_t *ps_interface, *ps_interface_tmp;
	bool found_interface;	
	
	delete_interfaces();
	
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
					if ((interfaces->ifa_addr != 0) && (interfaces->ifa_flags & IFF_LOOPBACK) == 0)
					{
						ps_interface_t *new_ps_interface = (ps_interface_t *) malloc(sizeof(ps_interface_t));
						memset(new_ps_interface, 0, sizeof(ps_interface_t));
				
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

	ps_interface_t *new_ps_interface = (ps_interface_t *) malloc(sizeof(ps_interface_t));
	memset(new_ps_interface, 0, sizeof(ps_interface_t));
	
	strcpy(new_ps_interface->if_name, "any");
	new_ps_interface->udp_info = LIST_HEAD_INITIALIZER();
						
	LIST_INSERT_HEAD(&ps_interfaces, new_ps_interface, link);	

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

bool monitor_multicast(char *if_name)
{
	m_if_name = if_name;
	
	log_message((char *) & "started");
	get_interfaces();
	
	buffer = (unsigned char *) malloc(RECEIVE_BUFFER_SIZE);    

	sock_raw = socket(PF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
	if (sock_raw < 0) {
		perror("Socket Error");
		return false;
	}

	int flag_on = 1; 
	setsockopt(sock_raw, SOL_SOCKET, SO_REUSEADDR, &flag_on, sizeof(flag_on));
		
	if (strcmp(m_if_name, "any") != 0)
	{
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
	}
	
	fd_set readset;
	struct timeval tv;
	int result;

	while (1)
	{
		FD_ZERO(&readset);
		FD_SET(sock_raw, &readset);
    
		tv.tv_sec = 0;
		tv.tv_usec = 100000;  // 100 ms
        
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
		
		gettimeofday(&now, NULL);
		cur_time = now.tv_sec + (now.tv_usec/1000000.0);

		elapsed_time = (int) (cur_time - last_udp_check_time);
		
		if (elapsed_time >= 1)
		{
			check_udp_entries();
		
			gettimeofday(&now, NULL);
			last_udp_check_time = now.tv_sec + (now.tv_usec / 1000000.0);
		}

	}
	
	return true;
}

void check_udp_entries()
{	
	ps_interface_t *ps_interface, *ps_interface_tmp;
	ps_udp_info_t *ps_udp_info, *ps_udp_info_tmp;

	LIST_FOREACH_SAFE(ps_interface, &ps_interfaces, link, ps_interface_tmp) {
		if (strcmp(m_if_name, ps_interface->if_name) == 0)
		{
			LIST_FOREACH_SAFE(ps_udp_info, &ps_interface->udp_info, link, ps_udp_info_tmp) {
				if (!ps_udp_info->closed)
				{
					last_data_time = ps_udp_info->last_data_time.tv_sec + (ps_udp_info->last_data_time.tv_usec / 1000000.0);
					elapsed_time = cur_time - last_data_time; 
					if (elapsed_time >= 1)
					{
						ps_udp_info->closed = true;
						gettimeofday(&ps_udp_info->stop_time, NULL);
						
						s1.s_addr = ps_udp_info->source;
						s2.s_addr = ps_udp_info->dest;
				
						strcpy(ss1, inet_ntoa(s1));
						strcpy(ss2, inet_ntoa(s2));
				
						start_time = ps_udp_info->start_time.tv_sec + (ps_udp_info->start_time.tv_usec / 1000000.0);
						elapsed_time = cur_time - start_time; 

						sprintf(msg_buf, "UDP STOPPED source %-16s dest %-16s elapsed %8.2f secs bytes %8d", ss1, ss2, elapsed_time, ps_udp_info->bytes); 
						log_message((char *) &msg_buf);
					}
				}
			}
		}
	}	
}

void handle_udp(unsigned char *Buffer, int Size)
{
	ps_interface_t *ps_interface, *ps_interface_tmp;
	ps_udp_info_t *ps_udp_info, *ps_udp_info_tmp;

	struct udphdr *udph = (struct udphdr*)(Buffer + iphdrlen  + sizeof(struct ethhdr));
	bool found_entry = false;
	
	LIST_FOREACH_SAFE(ps_interface, &ps_interfaces, link, ps_interface_tmp) {
		if (strcmp(m_if_name, ps_interface->if_name) == 0)
		{
			LIST_FOREACH_SAFE(ps_udp_info, &ps_interface->udp_info, link, ps_udp_info_tmp) {
				if ((ps_udp_info->source == iph->saddr) && (ps_udp_info->dest == iph->daddr) && (!ps_udp_info->closed))
				{
					ps_udp_info->bytes += udph->len;
					found_entry = true;

					gettimeofday(&ps_udp_info->last_data_time, NULL);
				}
			}
			
			if (!found_entry)
			{
				ps_udp_info = (ps_udp_info_t *) malloc(sizeof(ps_udp_info_t));
				memset(ps_udp_info, 0, sizeof(ps_udp_info_t));
				
				ps_udp_info->source = iph->saddr;
				ps_udp_info->dest = iph->daddr;
				ps_udp_info->bytes = udph->len;
				ps_udp_info->closed = false;
				
				gettimeofday(&ps_udp_info->start_time, NULL);
				gettimeofday(&ps_udp_info->last_data_time, NULL);

				LIST_INSERT_HEAD(&ps_interface->udp_info, ps_udp_info, link);	

				s1.s_addr = ps_udp_info->source;
				s2.s_addr = ps_udp_info->dest;
				
				strcpy(ss1, inet_ntoa(s1));
				strcpy(ss2, inet_ntoa(s2));
				
				sprintf(msg_buf, "UDP STARTED source %-16s dest %-16s", ss1, ss2); 
				log_message((char *) &msg_buf);
			}
		}
	}	
}

bool ignore_ip()
{
	uint32_t source = htonl(iph->saddr);
	uint32_t dest = htonl(iph->daddr);
	
	// 239.0.0.0 to 239.255.255.255
	if(((source >= 4009754624) && (source <= 4026531839)) || ((dest >= 4009754624) && (dest <= 4026531839)))
		return false;
	
	return true;
}

bool handle_group_report(in_addr_t group)
{				
	ps_interface_t *ps_interface, *ps_interface_tmp;
	ps_udp_info_t *ps_udp_info, *ps_udp_info_tmp;

	bool found_entry = false;

	s3.s_addr = group;
	strcpy(ss3, inet_ntoa(s3));

	LIST_FOREACH_SAFE(ps_interface, &ps_interfaces, link, ps_interface_tmp) {
		if (strcmp(m_if_name, ps_interface->if_name) == 0)
		{
			LIST_FOREACH_SAFE(ps_udp_info, &ps_interface->udp_info, link, ps_udp_info_tmp) {
				// if ((ps_udp_info->source == iph->saddr) && (ps_udp_info->dest == iph->daddr) && (!ps_udp_info->closed))
				if ((ps_udp_info->dest == iph->daddr) && (!ps_udp_info->closed))
				{
					found_entry = true;
					break;
				}
			}
		}
	}

	sprintf(msg_buf, "IGMP_V2_GROUP_REPORT: src %s dst %s grp %s has data: %s", ss1, ss2, ss3, found_entry ? "YES" : "NO");
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
	int rec_num_sources = 0;
	int rec_auxdatalen = 0;
	int record_size = 0;

	bool status = false;

	uint32_t group = igmp->igmp_group.s_addr;
	
	s3.s_addr = group;
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
		
		for (int i = 0; i < num_groups; i++) {

			rec_num_sources = ntohs(record->grec_nsrcs);
			rec_auxdatalen = record->grec_auxwords;
			record_size = sizeof(struct igmpv3_grec) + sizeof(uint32_t) * rec_num_sources + rec_auxdatalen;

			rec_type = record->grec_type;
			rec_group.s_addr = (in_addr_t)record->grec_mca;

			s3.s_addr = rec_group.s_addr;
			strcpy(ss3, inet_ntoa(s3));

			switch (rec_type) {
			case IGMP_DO_NOTHING:
				sprintf(msg_buf, "IGMP_DO_NOTHING: grp %s", ss3);
				log_message(msg_buf);
				break;
				
			case IGMP_MODE_IS_INCLUDE:
				sprintf(msg_buf, "IGMP_MODE_IS_INCLUDE (accept sources): grp %s", ss3);
				log_message(msg_buf);
				break;

			case IGMP_MODE_IS_EXCLUDE:
				handle_group_report(rec_group.s_addr);
				break;

			case IGMP_CHANGE_TO_INCLUDE_MODE:
				sprintf(msg_buf, "IGMP_CHANGE_TO_INCLUDE_MODE (accept sources): grp %s", ss3);
				log_message(msg_buf);
				break;

			case IGMP_CHANGE_TO_EXCLUDE_MODE:
				handle_group_report(rec_group.s_addr);
				break;

			case IGMP_ALLOW_NEW_SOURCES:
				sprintf(msg_buf, "IGMP_ALLOW_NEW_SOURCES (accept sources): grp %s", ss3);
				log_message(msg_buf);
				break;

			case IGMP_BLOCK_OLD_SOURCES:
				sprintf(msg_buf, "IGMP_BLOCK_OLD_SOURCES (leave and remove sources): grp %s", ss3);
				log_message(msg_buf);
				break;
								
			default:
				sprintf(msg_buf, "IGMP_V3_MEMBERSHIP_REPORT: Unknown type %d", rec_type);
				log_message(msg_buf);
			}
			
			record = (struct igmpv3_grec *)((uint8_t *)record + record_size);
		}
		
	case IGMP_V2_LEAVE_GROUP:
		sprintf(msg_buf, "IGMP_V2_LEAVE_GROUP: src %s dst %s grp %s", ss1, ss2, ss3);
		log_message((char *) &msg_buf);

		return true;
		break;

	case IGMP_PIM:		
		sprintf(tmp_buf, "IGMP_PIM_V1: src %s dst %s grp %s code", ss1, ss2, ss3);

		if (htonl(igmp->igmp_group.s_addr) == PIM_V1_VERSION)
		{
			switch (igmp->igmp_code)
			{
			case PIM_V1_QUERY:
				sprintf(msg_buf, "%s PIM_V1_QUERY", tmp_buf);
				break;
				
			case PIM_V1_REGISTER:
				sprintf(msg_buf, "%s PIM_V1_REGISTER", tmp_buf);
				break;
				
			case PIM_V1_REGISTER_STOP:
				sprintf(msg_buf, "%s PIM_V1_REGISTER_STOP", tmp_buf);
				break;
				
			case PIM_V1_JOIN_PRUNE:
				sprintf(msg_buf, "%s PIM_V1_JOIN_PRUNE", tmp_buf);
				break;
				
			case PIM_V1_RP_REACHABLE:
				sprintf(msg_buf, "%s PIM_V1_RP_REACHABLE", tmp_buf);
				break;
				
			case PIM_V1_ASSERT:
				sprintf(msg_buf, "%s PIM_V1_ASSERT", tmp_buf);
				break;
				
			case PIM_V1_GRAFT:
				sprintf(msg_buf, "%s PIM_V1_GRAFT", tmp_buf);
				break;
				
			case PIM_V1_GRAFT_ACK:
				sprintf(msg_buf, "%s PIM_V1_GRAFT_ACK", tmp_buf);
				break;
				
			case PIM_V1_MODE:
				sprintf(msg_buf, "%s PIM_V1_MODE", tmp_buf);
				break;
				
			default:
				sprintf(msg_buf, "%s Unknown Code", tmp_buf);
			}
		}
		else
			sprintf(msg_buf, "%s NOT V1", tmp_buf);
			
		log_message((char *) &msg_buf);

		return true;
		break;
		
	default:
		printf("Unknown IGMP Type: %d\n", igmp->igmp_type);
		return true;
	}

	return true;
}

bool handle_pim(unsigned char *buffer, int size)
{
	struct pim *pim_hdr = (struct pim *)(buffer + iphdrlen + sizeof(struct ethhdr));

	uint8_t *pim_msg = buffer + iphdrlen;
	int pim_msg_len = size - iphdrlen;
	int pim_type = PIM_MSG_HDR_GET_TYPE(pim_msg);

	sprintf(tmp_buf, "src %s dst %s grp %s", ss1, ss2, ss3);
	
	switch (pim_type)
	{
	case PIM_MSG_TYPE_HELLO:
		sprintf(msg_buf, "PIM_MSG_TYPE_HELLO: %s", tmp_buf);
		break;
		
	case PIM_MSG_TYPE_REGISTER:
		sprintf(msg_buf, "PIM_MSG_TYPE_REGISTER: %s", tmp_buf);
		break;

	case PIM_MSG_TYPE_REG_STOP:
		sprintf(msg_buf, "PIM_MSG_TYPE_REG_STOP: %s", tmp_buf);
		break;
		
	case PIM_MSG_TYPE_JOIN_PRUNE:
		sprintf(msg_buf, "PIM_MSG_TYPE_JOIN_PRUNE: %s", tmp_buf);
		break;
		
	case PIM_MSG_TYPE_BOOTSTRAP:
		sprintf(msg_buf, "PIM_MSG_TYPE_BOOTSTRAP: %s", tmp_buf);
		break;
		
	case PIM_MSG_TYPE_ASSERT:
		sprintf(msg_buf, "PIM_MSG_TYPE_ASSERT: %s", tmp_buf);
		break;
		
	case PIM_MSG_TYPE_GRAFT:
		sprintf(msg_buf, "PIM_MSG_TYPE_GRAFT: %s", tmp_buf);
		break;
		
	case PIM_MSG_TYPE_GRAFT_ACK:
		sprintf(msg_buf, "PIM_MSG_TYPE_GRAFT_ACK: %s", tmp_buf);
		break;
		
	case PIM_MSG_TYPE_CANDIDATE:
		sprintf(msg_buf, "PIM_MSG_TYPE_CANDIDATE: %s", tmp_buf);
		break;
		
	default:
		sprintf(msg_buf, "Unknown PIM Type(%d): %s", pim_type, tmp_buf);
	}

	log_message((char *) &msg_buf);

	return true;
}

void print_data (unsigned char* data , int Size)
{
    int i , j;
 
	if (!dump_first_line_only)
		msg_buf[0] = 0;
	
	for(i=0 ; i < Size ; i++) {
        if( i!=0 && i%16==0)   //if one line of hex printing is complete...
        {
	        // msg_buf[0] = 0;
	        
            strcat(msg_buf, "         ");
            for(j=i-16 ; j<i ; j++) {
	            if (data[j] >= 32 && data[j] <= 128)
	            {
		            sprintf(tmp_buf, "%c", (unsigned char)data[j]);  //if number or alphabet
					strcat(msg_buf, tmp_buf);
	            }
                else 
                  strcat(msg_buf, "."); //otherwise print a dot
            }
	   
			if (dump_first_line_only)
			    return; // only print first line
	        
            strcat(msg_buf, "\n");
        } 
        
        if(i%16==0) strcat(msg_buf, "   ");
	    {
		    sprintf(tmp_buf, " %02X", (unsigned int)data[i]);
		    strcat(msg_buf, tmp_buf);
	    }
        
        if( i==Size-1)  //print the last spaces
        {
            for(j=0;j<15-i%16;j++) {
                strcat(msg_buf, "   "); //extra spaces
            }
            
            strcat(msg_buf, "         ");
            
            for(j=i-i%16 ; j<=i ; j++) {
                if(data[j]>=32 && data[j]<=128) {
                    sprintf(tmp_buf, "%c",(unsigned char)data[j]);
					strcat(msg_buf, tmp_buf);
                }
                else {
                    strcat(msg_buf, ".");
                }
            }           
	        
			if (dump_first_line_only)
			    return; // only print first line
	        
            strcat(msg_buf,  "\n" );
        }
    }
}

bool handle_tcp(unsigned char *buffer, int size)
{
	struct tcphdr *tcph = (struct tcphdr *)(buffer + iphdrlen + sizeof(struct ethhdr));
	int header_size =  sizeof(struct ethhdr) + iphdrlen + tcph->doff * 4;

	unsigned char *data = buffer + header_size;
	int data_len = size - header_size;

	data[data_len] = 0;
	msg_buf[0] = 0;
	
	if (tcph->th_flags & TH_FIN)
		;
		// sprintf(msg_buf, "TCP: src %s dest %s len %d FIN", ss1, ss2, data_len);

	else if (tcph->th_flags & TH_SYN)
		;
		// sprintf(msg_buf, "TCP: src %s dest %s len %d SYN", ss1, ss2, data_len);
	
	else if (tcph->th_flags & TH_RST)
		sprintf(msg_buf, "TCP: src %s dest %s len %d RST", ss1, ss2, data_len);
	
	else if(tcph->th_flags & TH_PUSH)
	{
		sprintf(msg_buf, "TCP: src %s dest %s len %d PUSH", ss1, ss2, data_len);
		
		if (!dump_first_line_only)
			log_message((char *) msg_buf);

		print_data(data, data_len);
	}
	
	else if(tcph->th_flags & TH_ACK)
		;
		// sprintf(msg_buf, "TCP: src %s dest %s len %d ACK", ss1, ss2, data_len);
	
	else if (tcph->th_flags & TH_URG)
		sprintf(msg_buf, "TCP: src %s dest %s len %d URG", ss1, ss2, data_len);
	
	else
		sprintf(msg_buf, "TCP: src %s dest %s len %d UNKNOWN", ss1, ss2, data_len);
		

	if (strlen(msg_buf) > 0)
		log_message((char *) msg_buf);
	
	return true;
}

bool ProcessPacket(unsigned char* buffer, int size)
{
	unsigned char* data;
	
	eth = (struct ethhdr *)buffer;
	iph = (struct iphdr *)(buffer  + sizeof(struct ethhdr));
	iphdrlen = iph->ihl * 4;

	s1.s_addr = iph->saddr;
	s2.s_addr = iph->daddr;

	strcpy(ss1, inet_ntoa(s1));
	strcpy(ss2, inet_ntoa(s2));

	bool result = true;
	switch (iph->protocol) //Check the Protocol and do accordingly...
		{
		case 1:  //ICMP Protocol
			sprintf(msg_buf, "ICMP Protocol: src %s dest %s", ss1, ss2);

			if (!dump_first_line_only)
				log_message((char *) msg_buf);

			data = (unsigned char *)(buffer + iphdrlen + sizeof(struct ethhdr));
			print_data(data, size - iphdrlen);

			break;
        
		case 2:  //IGMP Protocol
			handle_igmp(buffer, size);
			break;
        
		case 6:  //TCP Protocol
			// handle_tcp(buffer, size);
			break;
        
		case 17: //UDP Protocol
			if (!ignore_ip())
			    handle_udp(buffer, size);
			break;
			
		case 89: // OSPFIGP Protocol
			sprintf(msg_buf, "OSPFIGP Protocol: src %s dest %s", ss1, ss2);

			if (!dump_first_line_only)
				log_message((char *) msg_buf);

			data = (unsigned char *)(buffer + iphdrlen + sizeof(struct ethhdr));
			print_data(data, size - iphdrlen);

			break;
			
		case 103: // PIM Protocol
			handle_pim(buffer, size);
			break;
        
		default: //Some Other Protocol like ARP etc.

			sprintf(msg_buf, "Unknown IP Protocol (%d): src %s dest %s", iph->protocol, ss1, ss2);
			
			if (!dump_first_line_only)
				log_message((char *) msg_buf);

			data = (unsigned char *)(buffer + iphdrlen + sizeof(struct ethhdr));
			print_data(data, size - iphdrlen);
			
			break;
		}
    
	return result;
}
