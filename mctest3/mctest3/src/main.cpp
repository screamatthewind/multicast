// https : //github.com/janbar/demux-mpegts
/*
 *      Copyright (C) 2013 Jean-Luc Barriere
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#define PACKAGE_VERSION "1.0.9"

#define __STDC_FORMAT_MACROS 1
#define OMX_SKIP64BIT

#define MAX_STREAMS 100
#define MIN_TIME_US 1517997165188603ull // sanity check for date time

//#include <gperftools/profiler.h>

#include <signal.h>
#include <getopt.h>
#include <string.h>

#include "mctest3.h"

parms_t parms;

int num_streams = 1;
int start_nibble = 1;
string base_url = string("239.0.1.");
string ip_address;
int port = 7399;
char if_name[32];
string channel_list_filename;
string timeout_exec_filename;
int channel_change_delay = 0;
bool run_once = false;

cCondWait *signalShutdown[MAX_STREAMS];
int started_streams = 0;

static LIST_HEAD(, ps_interface_t) ps_interfaces = LIST_HEAD_INITIALIZER();

void logit(const char *format, ...) {
	char timestamp[128];
	char msg[255];
	char tmpBuf[255];
	va_list ap;
		
	va_start(ap, format);
	vsnprintf(msg, sizeof(msg), format, ap);
	va_end(ap);
	
	GetTimestamp((char *) &timestamp);
	sprintf(tmpBuf, "%s %s", timestamp, msg);
	Logger::instance().log(tmpBuf, Logger::kLogLevelInfo);

	fprintf(stderr, "%s\n", msg);
}

void shutdown()
{
	logit("Shutting down streams");
	
	for (int i = 0; i < started_streams; i++)
		signalShutdown[i]->Signal();

	cCondWait::SleepMs(3000);
}

void sigAbortHandler(int signum) {
	//ProfilerStop();
	
	shutdown();
	cout << "Abort signal (" << signum << ") received.\n";
	exit(6);  
}

void sigIntHandler(int signum) {
	//ProfilerStop();

	shutdown();
	cout << "Interrupt signal (" << signum << ") received.\n";
	exit(6);  
}

void sigSegvHandler(int signum) {
	//ProfilerStop();

	shutdown();
	cout << "Segment fault signal (" << signum << ") received.\n";
	exit(6);  
}

void sigTermHandler(int signum) {
	//ProfilerStop();

	shutdown();
	cout << "Terminate signal (" << signum << ") received.\n";
	exit(6);  
}

void sigStopHandler(int signum) {
	//ProfilerStop();

	shutdown();
	cout << "Stop signal (" << signum << ") received.\n";
	exit(6);  
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
	printf("Usage: %s [-afcbinsptzxv]\n", prognm);
	printf("  -a  --portal=PORTAL_ADRS    Address of portal.  Example - 192.168.44.143:8080\n");
	printf("  -f  --channels=CHANNEL_CSV  Path to channel listing file.  Default ../channel_list.csv\n");
	printf("  -c, --streams=NUM_STREAMS   Number of streams - default 1\n");
	printf("  -b, --base=BASE_URL         (eg. 239.0.2.) - default %s\n", base_url.c_str());
	printf("  -i, --ip=AP_ADDRESS         (eg. 239.0.2.1)\n");
	printf("  -n, --nic=NETWORK_IF        Used to test if network is available\n");
	printf("  -s, --start=START_NIBBLE    ip start nibble (eg 50 will start at <BASE_URL>.50");
	printf("  -p, --port=PORT             default %d\n", port);
	printf("  -t, --delay=CHANNEL_DELAY   Channel change delay - default (random)\n");
	printf("  -z, --once=FALSE            TRUE =  run once, FALSE = run forever - default FALSE\n");
	printf("  -x, --exec=SHELL_FILENAME   Execute program on timeout\n");
	printf("  -v, --version               Show version\n");
	printf("  -?, --help                  This message\n");
	printf("\n");
	
	exit(0);
}

void get_mac_address(char *if_name, char *mac_address)
{
	int s;
	struct ifreq buffer;
	char buf[32];

	mac_address[0] = 0;
	
	memset(&buffer, 0x00, sizeof(buffer));
	strcpy(buffer.ifr_name, if_name);

	s = socket(PF_INET, SOCK_DGRAM, 0);
	ioctl(s, SIOCGIFHWADDR, &buffer);

	close(s);
	
	for (s = 0; s < 6; s++)
	{
		sprintf(buf, "%.2X", (unsigned char)buffer.ifr_hwaddr.sa_data[s]);
		strcat(mac_address, buf);
		
		if (s < 5)
			strcat(mac_address, ":");
	}
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
					if ((interfaces->ifa_addr != 0) && (interfaces->ifa_flags & IFF_LOOPBACK) == 0)
					{
						ps_interface_t *new_ps_interface = (ps_interface_t *) malloc(sizeof(ps_interface_t));
						memset(new_ps_interface, 0, sizeof(ps_interface_t));
				
						new_ps_interface->ip_address = ((struct sockaddr_in*)interfaces->ifa_addr)->sin_addr.s_addr;
						strcpy(new_ps_interface->if_name, interfaces->ifa_name);
						
						LIST_INSERT_HEAD(&ps_interfaces, new_ps_interface, link);	
					}
				}
			}
			
			interfaces = interfaces->ifa_next;
		}
	}

	ps_interface_t *new_ps_interface = (ps_interface_t *) malloc(sizeof(ps_interface_t));
	memset(new_ps_interface, 0, sizeof(ps_interface_t));
	
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
	struct in_addr s1;

	get_interfaces();

	logit("Available interfaces:");

	LIST_FOREACH_SAFE(ps_interface, &ps_interfaces, link, ps_interface_tmp) {
		s1.s_addr = ps_interface->ip_address;
		logit("  %-8s %s", ps_interface->if_name, inet_ntoa(s1));
	}
}

void wait_for_network(char *if_name)
{
	int network_retries = 0;
	unsigned long long cur_time_us = 0;
	timeval cur_time;
	
	while (!CheckNetworkStatus(if_name))
	{
		if (network_retries++ > 60)
		{
			logit("Network is not available on interface %s", if_name);
			list_interfaces();
			usage();
		}
		
		cCondWait::SleepMs(1000);
	}

	network_retries = 0;
	while (!is_valid_interface(if_name))
	{
		if (network_retries++ > 60)
		{
			logit("%s is not a valid interface", if_name);
			list_interfaces();
			usage();
		}

		cCondWait::SleepMs(1000);
	}
	
	// sanity check for date/time
	network_retries = 0;
	do
	{
		gettimeofday(&cur_time, NULL);
		cur_time_us = (1000000ull * cur_time.tv_sec) + cur_time.tv_usec;

		if (network_retries++ > 60)
		{
			logit("Timed out waiting for time to be set on interface %s", if_name);
			list_interfaces();
			usage();
		}

		cCondWait::SleepMs(1000);
	} while (cur_time_us < MIN_TIME_US);
}

int main(int argc, char* argv[])
{
	struct option long_options[] = {
		{ "portal", 1, 0, 'a' },
		{ "channels", 1, 0, 'f' },
		{ "streams", 1, 0, 'c' },
		{ "base", 1, 0, 'b' },
		{ "ip", 1, 0, 'i' },
		{ "nic", 1, 0, 'n' },
		{ "start", 1, 0, 's' },
		{ "port", 1, 0, 'p' },
		{ "delay", 1, 0, 't' },
		{ "once", 1, 0, 'z' },
		{ "exec", 1, 0, 'x' },
		{ "version", 0, 0, 'v' },
		{ "help", 0, 0, '?' },
		{ NULL, 0, 0, 0 }
	};

	char versionstring[100];
	int   ch;
	char *p;
	int pos;
	
	prognm = progname(argv[0]);

	snprintf(versionstring, sizeof(versionstring), "%s version %s", prognm, PACKAGE_VERSION);

	parms.mac_address[0] = 0;
	parms.portal_ip[0] = 0;
	parms.portal_port = 0;
	
	while ((ch = getopt_long(argc, argv, "a:f:c:b:i:n:s:p:t:z:x:v?", long_options, NULL)) != EOF) {

		switch (ch) {

		case 'a':
			p = strchr(optarg, ':');
			if (p == 0)
			{
				logit("Invalid portal address");
				usage();
			}
			
			pos = p - optarg;
			optarg[pos] = 0;
			
			strcpy(parms.portal_ip, optarg);

			p++;
			parms.portal_port = atoi(p);
			
			break;
			
		case 'f':
			channel_list_filename = string(optarg);
			break;
			
		case 'c':
			num_streams = atoi(optarg);
			break;
			
		case 'b':
			base_url = string(optarg);
			break;

		case 'i':
			ip_address = string(optarg);
			break;

		case 'n':
			strcpy(if_name, optarg);
			break;

		case 's':
			start_nibble = atoi(optarg);
			break;

		case 'p':
			port = atoi(optarg);
			break;
			
		case 't':
			channel_change_delay = atoi(optarg);
			break;
			
		case 'z':
			if (strcmp(optarg, "TRUE"))
				run_once = true;
			else
				run_once = false;
			break;

		case 'x':
			timeout_exec_filename = string(optarg);
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
	
	if (strlen(if_name) == 0)
	{
		logit("Interface is required");
		list_interfaces();
		usage();
	}

	wait_for_network(if_name);

	parms.num_streams = num_streams;
	parms.start_nibble = start_nibble;
	parms.base_url = base_url;
	parms.ip_address = ip_address;
	parms.port = port;
	parms.channel_list_filename = channel_list_filename;
	parms.channel_change_delay = channel_change_delay;
	parms.run_once = run_once;
	parms.timeout_exec_filename = timeout_exec_filename;

	get_mac_address(if_name, parms.mac_address);
	
	strcpy(parms.if_name, if_name);
	
	try
	{
		configureSignalHandlers();
		
		Demux *demux[num_streams];
		
		// printf("Main PID: %d\n", getpid());

		for(int i = 0 ; i < num_streams ; i++)
		{
			printf("Starting an instance of Demux (%u)\n", i);
			Logger::instance().log("Starting an instance of Demux", Logger::kLogLevelInfo);

			parms.instance_num = i;
			
			signalShutdown[i] = new cCondWait();
			demux[i] = new Demux(signalShutdown[i], &parms);

			started_streams++;
			
			cCondWait::SleepMs(10 * 1000);
		}
		
		while (1)
			cCondWait::SleepMs(1000);
	
		return 0;
	}
	catch (const std::exception& e) {
		logit("Exception %s during %s on line %d\n", e.what(), __func__, __LINE__);
	}
	catch (...) {
		logit("Unknown Exception during %s on line %d\n", __func__, __LINE__);
	}
}
