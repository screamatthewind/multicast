#include <inttypes.h>
#include <signal.h>
#include <iostream>
#include <getopt.h>
#include <string.h>

#include "defs.h"
#include "tools.h"
#include "thread.h"

#include "ChannelUtils.h"

#define PACKAGE_VERSION "1.0.8"
#define LINESZ 1024

#define DEFAULT_PORTAL_IP "209.119.193.130"

ChannelUtils *m_channelUtils;

int port = 7399;
stb_data_t stb_data;
bool debug_enabled = false;

char timestamp[255];
char msg_buf[1024];

char portal_ip[INET_ADDRSTRLEN];
char ip_address[INET_ADDRSTRLEN];

using namespace std;

void log_message(char *message)
{
	FILE *logFile = NULL;

	char hostname[255];
	char filename[LINESZ];
	time_t now = time(NULL);

	gethostname(hostname, LINESZ);

	strftime(timestamp, LINESZ, "%m%d%Y", localtime(&now));
	sprintf(filename, "/var/log/%s-log_stb_activity-%s.log", hostname, timestamp);

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

void clean_up()
{
	log_message((char *) & "Exited");

	exit(0);  
}

void sigAbortHandler(int signum) {
	clean_up();
}

void sigIntHandler(int signum) {
	clean_up();
}

void sigSegvHandler(int signum) {
	clean_up();
}

void sigTermHandler(int signum) {
	clean_up();
}

void sigStopHandler(int signum) {
	clean_up();
}

void configureSignalHandlers()
{
	signal(SIGABRT, sigAbortHandler);  
	signal(SIGINT, sigIntHandler);  
	signal(SIGSEGV, sigSegvHandler);  
	signal(SIGTERM, sigTermHandler);  
	signal(SIGTSTP, sigStopHandler);
}

char *prognm      = NULL;

static char *progname(char *arg0)
{
	char *nm;

	nm = strrchr(arg0, '/');
	if (nm)
		nm++;
	else
		nm = arg0;

	return nm;
}

void usage()
{
	printf("Usage: %s [-inpmgdv]\n", prognm);
	printf("  -i  --address=IP_ADDRESS    ip address to listen\n");
	printf("  -n  --nic=NETWORK_IF        network interface name - default ANY\n");
	printf("  -p  --portal=PORTAL_IP      address of portal - default %s\n", portal_ip);
	printf("  -m  --mybytes               show only my bytes - default: show all bytes\n");
	printf("  -g  --noget                 don't wait for get (play) message\n");
	printf("  -d  --debug                 show all packet types\n");
	printf("  -v, --version               Show version\n");
	printf("  -?, --help                  This message\n");
	printf("\n");
	
	exit(0);
}

void output_header()
{
	GetTimestamp((char *) &timestamp);
	sprintf(msg_buf, "%s %-59s %-18s %-9s %s", timestamp, "Channel", "Status", "Elapsed", "Detail");
	log_message(msg_buf);
}

void init()
{
	configureSignalHandlers();

	inet_pton(AF_INET, portal_ip, (void *) &stb_data.portal_ip);

	if (strlen(ip_address) > 0)
		inet_pton(AF_INET, ip_address, (void *) &stb_data.ip_address);
	
	log_message((char *) & "Started");
	output_header();
	
	m_channelUtils = new ChannelUtils();
}

void output_stats()
{
	char url[255];
	char status[32];
	char s_group_ip[INET_ADDRSTRLEN];
		
	inet_ntop(AF_INET, &stb_data.group_ip, s_group_ip, INET_ADDRSTRLEN);
	
	sprintf(url, "udp://%s:%d", s_group_ip, port);
	m_channelUtils->setUrl(string(url), false);
		
	switch (stb_data.status)
	{
	case 0:
		strcpy(status, "Ok");
		break;
	case -1:
		strcpy(status, "Socket Error");
		break;
	case -2:
		strcpy(status, "Timed Out");
		break;
	case -3:
		strcpy(status, "Recvfrom Error");
		break;
	case -4:
		strcpy(status, "Can't Happen");
		break;
	case -5:
		strcpy(status, "Socket Closed");
		break;
	}
	
	char tmp_buf[1024];
	char tmp_buf2[1024];
		
	sprintf(tmp_buf, "%s %-60s", timestamp, m_channelUtils->getChannelInfo().c_str());

	if (strstr(tmp_buf, "Unknown") != NULL)
	{
		sprintf(tmp_buf2, "Unknown (udp://%s:%d)", s_group_ip, port);
		sprintf(tmp_buf, "%s %-60s", timestamp, tmp_buf2);
	}
		
	strcpy(msg_buf, tmp_buf);
	
	bool gotMembershipReport = false;
	bool gotData = false;

	uint64_t elapsed_time = 99999;   // default is Timed Out

	for(int i = 0 ; i < stb_data.num_stats ; i++)
	{
		if (stb_data.stats[i].entry_type == entry_type_udp)
		{
			elapsed_time = stb_data.stats[i].elapsed_ms;
			break;
		}
	}	
	
	sprintf(tmp_buf, "%-16s %6" PRIu64 " ms  ", status, elapsed_time);
	strcat(msg_buf, tmp_buf);
	
	for (int i = 0; i < stb_data.num_stats; i++)
	{
		bool show_bytes = true;
	
		if (stb_data.stats[i].entry_type == entry_type_membership_report) {
			sprintf(tmp_buf, " %-10s %6" PRIu64 " ms", "join", stb_data.stats[i].elapsed_ms);
			strcat(msg_buf, tmp_buf);
			
			gotMembershipReport = true;
		}
		
		else if (stb_data.stats[i].entry_type == entry_type_membership_query) {
			if (debug_enabled)
			{
				sprintf(tmp_buf, " %-10s %6" PRIu64 " ms", "query", stb_data.stats[i].elapsed_ms);
				strcat(msg_buf, tmp_buf);
			}
			else
				show_bytes = false;
		}

		else if (stb_data.stats[i].entry_type == entry_type_leave_group) {
			sprintf(tmp_buf, " %-10s %6" PRIu64 " ms", "leave", stb_data.stats[i].elapsed_ms);
			strcat(msg_buf, tmp_buf);
		}

		else if (stb_data.stats[i].entry_type == entry_type_udp) {
			sprintf(tmp_buf, " %-10s %6" PRIu64 " ms", "data", stb_data.stats[i].elapsed_ms);
			strcat(msg_buf, tmp_buf);
			
			gotData = true;
		}
		
		else if (stb_data.stats[i].entry_type == entry_type_play_request) {
			sprintf(tmp_buf, " %-10s %6" PRIu64 " ms", "play req", stb_data.stats[i].elapsed_ms);
			strcat(msg_buf, tmp_buf);
		}

		else if (stb_data.stats[i].entry_type == entry_type_stop_request) {
			if (debug_enabled)
			{
				sprintf(tmp_buf, " %-10s %6" PRIu64 " ms", "stop req", stb_data.stats[i].elapsed_ms);
				strcat(msg_buf, tmp_buf);
			}
			else
				show_bytes = false;
		}

		else if (stb_data.stats[i].entry_type == entry_type_log_request) {
			if (debug_enabled)
			{
				sprintf(tmp_buf, " %-10s %6" PRIu64 " ms", "log req", stb_data.stats[i].elapsed_ms);
				strcat(msg_buf, tmp_buf);
			}
			else
				show_bytes = false;
		}

		else if (stb_data.stats[i].entry_type == entry_type_get_logo_request) {
			if (debug_enabled)
			{
				sprintf(tmp_buf, " %-10s %6" PRIu64 " ms", "logo req", stb_data.stats[i].elapsed_ms);
				strcat(msg_buf, tmp_buf);
			}
			else
				show_bytes = false;
		}

		else if (stb_data.stats[i].entry_type == entry_type_set_last_id_request) {
			if (debug_enabled)
			{
				sprintf(tmp_buf, " %-10s %6" PRIu64 " ms", "set id", stb_data.stats[i].elapsed_ms);
				strcat(msg_buf, tmp_buf);
			}
			else
				show_bytes = false;
		}

		else if (stb_data.stats[i].entry_type == entry_type_get_events_request) {
			if (debug_enabled)
			{
				sprintf(tmp_buf, " %-10s %6" PRIu64 " ms", "get evts", stb_data.stats[i].elapsed_ms);
				strcat(msg_buf, tmp_buf);
			}
			else
				show_bytes = false;
		}

		else if (stb_data.stats[i].entry_type == entry_type_set_events_request) {
			if (debug_enabled)
			{
				sprintf(tmp_buf, " %-10s %6" PRIu64 " ms", "set evts", stb_data.stats[i].elapsed_ms);
				strcat(msg_buf, tmp_buf);
			}
			else
				show_bytes = false;
		}

		else if (stb_data.stats[i].entry_type == entry_type_get_current_request) {
			if (debug_enabled)
			{
				sprintf(tmp_buf, " %-10s %6" PRIu64 " ms", "get cur", stb_data.stats[i].elapsed_ms);
				strcat(msg_buf, tmp_buf);
			}
			else
				show_bytes = false;
		}

		else if (stb_data.stats[i].entry_type == entry_type_unknown_request) {
			sprintf(tmp_buf, " %-10s %6" PRIu64 " ms", "unk req", stb_data.stats[i].elapsed_ms);
			strcat(msg_buf, tmp_buf);
		}
		
		else if (stb_data.stats[i].entry_type == entry_type_response) {
			if (debug_enabled)
			{
				sprintf(tmp_buf, " %-10s %6" PRIu64 " ms", "resp", stb_data.stats[i].elapsed_ms);
				strcat(msg_buf, tmp_buf);
			}
			else
				show_bytes = false;
		}

		else if (stb_data.stats[i].entry_type == entry_type_ack) {
			if (debug_enabled)
			{
				sprintf(tmp_buf, " %-10s %6" PRIu64 " ms", "ack", stb_data.stats[i].elapsed_ms);
				strcat(msg_buf, tmp_buf);
			}
			else
				show_bytes = false;
		}

		else if (stb_data.stats[i].entry_type == entry_type_syn) {
			if (debug_enabled)
			{
				sprintf(tmp_buf, " %-10s %6" PRIu64 " ms", "syn", stb_data.stats[i].elapsed_ms);
				strcat(msg_buf, tmp_buf);
			}
			else
				show_bytes = false;
		}

		else if (stb_data.stats[i].entry_type == entry_type_other) {
			sprintf(tmp_buf, " %-10s %6" PRIu64 " ms", "other", stb_data.stats[i].elapsed_ms);
			strcat(msg_buf, tmp_buf);
		}

		else {
			sprintf(tmp_buf, " %-10s %6" PRIu64 " ms", "unknown", stb_data.stats[i].elapsed_ms);
			strcat(msg_buf, tmp_buf);
		}

		if (show_bytes)
		{
			if (stb_data.stats[i].byte_ctr > 9999999)
				sprintf(tmp_buf, " %8d bytes", 9999999);
			else
				sprintf(tmp_buf, " %8" PRIu64 " bytes,", stb_data.stats[i].byte_ctr);
		
			strcat(msg_buf, tmp_buf);
		}
		
		if (gotMembershipReport && gotData)
			break;
	}

	log_message(msg_buf);
}

bool is_valid_interface()
{
	bool result = false;

	struct ifaddrs *interfaces = NULL;
	struct ifaddrs *temp = NULL;
	
	int success = getifaddrs(&interfaces);
	if (success == 0) {

		temp = interfaces;
		
		while (temp != NULL) {
			if ((temp->ifa_addr) && (temp->ifa_addr->sa_family == AF_INET)) {
				if (strcmp(temp->ifa_name, stb_data.if_name) == 0) {
					result = true;
					break;
				}
			}
			
			temp = temp->ifa_next;
		}
	}

	freeifaddrs(interfaces);
	
	return result;
}

bool list_interfaces()
{
	bool result = false;
	struct ifaddrs *interfaces = NULL;
	
	int success = getifaddrs(&interfaces);
	if (success == 0) {

		printf("\nAvailable interfaces:\n");
		
		while (interfaces != NULL) {
			if ((interfaces->ifa_addr) && (interfaces->ifa_addr->sa_family == AF_INET))
				printf("  %-8s %s\n", interfaces->ifa_name, inet_ntoa(((struct sockaddr_in*)interfaces->ifa_addr)->sin_addr));
			
			interfaces = interfaces->ifa_next;
		}
	}

	freeifaddrs(interfaces);
	
	printf("\n");
	
	return result;
}

int main(int argc, char *argv[])
{
	struct option long_options[] = {
		{ "address", 1, 0, 'i' },
		{ "nic", 1, 0, 'n' },
		{ "portal", 1, 0, 'u' },
		{ "mybytes", 0, 0, 'm' },
		{ "noget", 0, 0, 'g' }, 
		{ "debug", 0, 0, 'd' },	 
		{ "version", 0, 0, 'v' },
		{ "help", 0, 0, '?' },
		{ NULL, 0, 0, 0 }
	};

	char versionstring[100];
	int   ch;
	
	prognm = progname(argv[0]);
	snprintf(versionstring, sizeof(versionstring), "%s version %s", prognm, PACKAGE_VERSION);

	strcpy(portal_ip, DEFAULT_PORTAL_IP);

	if (geteuid() != 0)
	{
		printf("Need root privileges to start\n");
		usage();
	}
	
	memset(&stb_data, 0, sizeof(stb_data_t));
	stb_data.get_play = true;
	
	while ((ch = getopt_long(argc, argv, "i:n:p:mgdv?", long_options, NULL)) != EOF) {

		switch (ch) {

		case 'i':
			strcpy(ip_address, optarg);
			break;
			
		case 'n':
			strcpy(stb_data.if_name, optarg);
			break;
			
		case 'p':
			strcpy(portal_ip, optarg);
			break;
			
		case 'm':
			stb_data.my_bytes = true;
		break;
			
		case 'g':
			stb_data.get_play = false;
		break;
			
		case 'd':
			debug_enabled = true;
		break;

		case 'v':
			printf("%s\n", versionstring);
			return 0;
			
		case '?':
			usage();

		default:
			usage();
		}
	}

	if (strlen(stb_data.if_name) == 0)
	{
		printf("Interface option is required\n");
		list_interfaces();
		usage();
	}

	else if (!is_valid_interface())
	{
		printf("%s is not a valid interface\n", stb_data.if_name);
		list_interfaces();
		usage();
	}
	
	init();
	
	while (1) {
		watch_for_changes(&stb_data, port);
		output_stats();
	}	
	
	return 0;
}