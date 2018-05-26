#include <inttypes.h>
#include <signal.h>
#include <getopt.h>

#include "defs.h"
#include "tools.h"
#include "thread.h"
#include "lirc_send.h"

#include "ChannelUtils.h"

#define PACKAGE_VERSION "1.0.8"
#define LINESZ 1024

#define DEFAULT_PORTAL_IP "173.225.90.3"
#define DEFAULT_PORT 7399

static char timestamp[255];
static char msg_buf[1024];

ChannelUtils *m_channelUtils;
bool debug_enabled = false;

int channel_num = -1;
int start_channel_num = -1;
int channel_change_delay = 5;   // default is 5 seconds
char portal_ip[INET_ADDRSTRLEN];
int run_for_mins = -1;

string ip_address;
stb_data_t stb_data;

void log_message(char *message)
{
	FILE *logFile = NULL;

	char hostname[255];
	char filename[LINESZ];
	time_t now = time(NULL);

	strftime(timestamp, LINESZ, "%m%d%Y", localtime(&now));

	gethostname(hostname, LINESZ);
	if (strcmp(hostname, "(none)") == 0)
		sprintf(filename, "/var/log/exercise_stb-%s.log", timestamp);
	else	
		sprintf(filename, "/var/log/%s-exercise_stb-%s.log", hostname, timestamp);

	logFile = fopen(filename, "a");
	
//	GetTimestamp((char *) &timestamp);
//	fprintf(logFile, "%s %s\n", timestamp, message);

	fprintf(logFile, "%s\n", message);
	printf("%s\n", message);
	
	if (logFile != NULL)
	{
		fflush(logFile);
		fclose(logFile);
		logFile = NULL;
	}
}

static void clean_up()
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
	printf("Usage: %s [-csipnutrv]\n", prognm);
	printf("  -c --channel=CHANNEL_NUM    channel number\n");
	printf("  -s --start=START_CHANNEL    channel number to start with - default (first)\n");
	printf("  -i, --ip=AP_ADDRESS         (eg. 239.0.2.1)\n");
	printf("  -p, --port=PORT             default %d\n", stb_data.port);
	printf("  -n  --nic=NETWORK_IF        network interface name - default ANY\n");
	printf("  -u  --portal=PORTAL_IP      address of portal - default %s\n", portal_ip);
	printf("  -t, --delay=CHANNEL_DELAY   Channel change delay in secs - default %d\n", channel_change_delay);
	printf("  -r, --runfor=RUN_MINS       Number of minutes to run - default = forever\n");
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
	char hostname[255];
	char filename[LINESZ];
	time_t now = time(NULL);

	configureSignalHandlers();

	inet_pton(AF_INET, portal_ip, (void *) &stb_data.portal_ip);

	m_channelUtils = new ChannelUtils();
	
	log_message((char *) & "Started");
	output_header();
	
	// make sure we are in television mode
	lirc_send_key(string(KEY_TV));
	cCondWait::SleepMs(100);

	if (start_channel_num > 0)
		m_channelUtils->getChannelByNumber(string(itoa(start_channel_num)));
	else
		m_channelUtils->getFirstChannel();
	
	lirc_change_channel(StrToNum(m_channelUtils->getChannelNumber().c_str()));
}

void output_stats()
{
	char url[255];
	char status[32];
	char s_group_ip[INET_ADDRSTRLEN];
		
	inet_ntop(AF_INET, &stb_data.group_ip, s_group_ip, INET_ADDRSTRLEN);
	
	sprintf(url, "udp://%s:%d", s_group_ip, stb_data.port);
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
		sprintf(tmp_buf2, "Unknown (udp://%s:%d)", s_group_ip, stb_data.port);
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
		{ "channel", 1, 0, 'c' },
		{ "start", 1, 0, 's' },
		{ "ip", 1, 0, 'i' },
		{ "port", 1, 0, 'p' },
		{ "nic", 1, 0, 'n' },
		{ "portal", 1, 0, 'u' },
		{ "delay", 1, 0, 't' },
		{ "runfor", 1, 0, 'r' },
		{ "version", 0, 0, 'v' },
		{ "help", 0, 0, '?' },
		{ NULL, 0, 0, 0 }
	};

	char versionstring[100];
	int   ch;
	
	prognm = progname(argv[0]);
	snprintf(versionstring, sizeof(versionstring), "%s version %s", prognm, PACKAGE_VERSION);

	memset(&stb_data, 0, sizeof(stb_data));
	
	strcpy(portal_ip, DEFAULT_PORTAL_IP);
	stb_data.port = DEFAULT_PORT;
	
	while ((ch = getopt_long(argc, argv, "c:s:i:p:n:u:t:r:v?", long_options, NULL)) != EOF) {

		switch (ch) {

		case 'c':
			channel_num = atoi(optarg);
			break;
			
		case 's':
			start_channel_num = atoi(optarg);
			break;
		
		case 'i':
			ip_address = string(optarg);
			break;

		case 'p':
			stb_data.port = atoi(optarg);
			break;

		case 'n':
			strcpy(stb_data.if_name, optarg);
			break;

		case 'u':
			strcpy(portal_ip, optarg);
			break;
			
		case 't':
			channel_change_delay = atoi(optarg);
			break;
			
		case 'r':
			run_for_mins = atoi(optarg);
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
	
	stb_data.monitorTimer = new cTimeMs();
	cTimeMs *m_runForTimer = new cTimeMs();

	if (ip_address.length() > 0) {

		char url[255];

		sprintf(url, "udp://%s:%d", ip_address.c_str(), stb_data.port);
		m_channelUtils->setUrl(string(url), false);

		string channel_number = m_channelUtils->getChannelNumber();
		lirc_change_channel(StrToNum(channel_number.c_str()));
		
		stb_data.monitorTimer->Set(channel_change_delay * 1000);
		sniff_packets(&stb_data);

		output_stats();
		clean_up();
	}

	if (channel_num > 0) {
		
		m_channelUtils->getChannelByNumber(string(itoa(channel_num)));
		lirc_change_channel(channel_num);

		stb_data.monitorTimer->Set(channel_change_delay * 1000);
		sniff_packets(&stb_data);

		output_stats();
		clean_up();
	}
	
	if (run_for_mins > 0)
		m_runForTimer->Set(run_for_mins * 60 * 1000);

	init();

	while (1) {

		stb_data.monitorTimer->Set(channel_change_delay * 1000);

		// set log time to be just before we start sniffing packets
		GetTimestamp((char *) &timestamp);
		
		sniff_packets(&stb_data);
		output_stats();

		if ((run_for_mins > 0) && (m_runForTimer->TimedOut()))
			break;
		
		cCondWait::SleepMs(stb_data.monitorTimer->Remainder());

		lirc_channel_up();
	}	

	clean_up();
	
	return 0;
}