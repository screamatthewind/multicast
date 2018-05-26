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

timeval now;
double cur_time, start_time, last_data_time, elapsed_time;
double last_udp_check_time = 0;

void check_udp_entries();
bool ProcessPacket(unsigned char*, int);
extern void clean_up();

uint32_t ignore_addresses[] = {
   0,  // 0.0.0.0
   67373064, // 8.8.4.4
   134744072, // 8.8.8.8
   16881930, // 10.153.1.1
   4294967295 // 255.255.255.255
};

void log_message(char *message)
{
	FILE *logFile = NULL;

	char hostname[LINESZ];
	char filename[LINESZ];
	time_t now = time(NULL);

	gethostname(hostname, LINESZ);

	strftime(timestamp, LINESZ, "%m%d%Y", localtime(&now));
	sprintf(filename, "/var/log/%s-%s-monitor_udp-%s.log", hostname, m_if_name, timestamp);

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

bool monitor_udp(char *if_name)
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

						sprintf(msg_buf, "stopped source %-16s dest %-16s elapsed %8.2f secs bytes %8d", ss1, ss2, elapsed_time, ps_udp_info->bytes); 
						log_message((char *) &msg_buf);
					}
				}
			}
		}
	}	
}

#define NELEMS(x)  (sizeof(x) / sizeof((x)[0]))

bool ignore_ip()
{
	uint32_t source = htonl(iph->saddr);
	uint32_t dest = htonl(iph->daddr);
	
	// 239.0.0.0 to 239.255.255.255
	if (((source >= 4009754624) && (source <= 4026531839)) || ((dest >= 4009754624) && (dest <= 4026531839)))
		return false;
	
//	int n = NELEMS(ignore_addresses);
//	
//	for (int i = 0; i < n; i++)
//	{
//		if (iph->saddr == ignore_addresses[i])
//			return true;
//
//		if (iph->daddr == ignore_addresses[i])
//			return true;
//	}
	
	return true;
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
				
				sprintf(msg_buf, "started source %-16s dest %-16s", ss1, ss2); 
				log_message((char *) &msg_buf);
			}
		}
	}	
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
			break;
        
		case 6:  //TCP Protocol
			break;
        
		case 17: //UDP Protocol
			if (!ignore_ip())
			    handle_udp(buffer, size);
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
