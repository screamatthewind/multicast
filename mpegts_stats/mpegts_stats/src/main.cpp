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


#define PACKAGE_VERSION "1.0.8"

#define __STDC_FORMAT_MACROS 1
#define OMX_SKIP64BIT

#define MAX_STREAMS 100

//#include <gperftools/profiler.h>

#include <signal.h>
#include <getopt.h>
#include <string.h>

#include "mpegts_stats.h"

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

void shutdown()
{
	fprintf(stderr, "Shutting down streams\n");
	
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
	printf("Usage: %s [-fcbinsptzxv]\n", prognm);
	printf("  -f  --channels=CHANNEL_CSV  Path to channel listing file.  Default ../channel_list.csv\n");
	printf("  -c, --streams=NUM_STREAMS   Number of streams - default 1\n");
	printf("  -b, --base=BASE_URL         (eg. 239.0.2.) - default %s\n", base_url.c_str());
	printf("  -i, --ip=AP_ADDRESS         (eg. 239.0.2.1)\n");
	printf("  -n, --nic=NETWORK_IF        default: any\n");
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
	
	strcpy(new_ps_interface->if_name, "any");
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
	struct in_addr s1;

	get_interfaces();

	printf("\nAvailable interfaces:\n");

	LIST_FOREACH_SAFE(ps_interface, &ps_interfaces, link, ps_interface_tmp) {
		s1.s_addr = ps_interface->ip_address;
		printf("  %-8s %s\n", ps_interface->if_name, inet_ntoa(s1));
	}

	printf("\n");
}

int main(int argc, char* argv[])
{
	struct option long_options[] = {
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
	
	prognm = progname(argv[0]);

	snprintf(versionstring, sizeof(versionstring), "%s version %s", prognm, PACKAGE_VERSION);

	while ((ch = getopt_long(argc, argv, "f:c:b:i:n:s:p:t:z:x:v?", long_options, NULL)) != EOF) {

		switch (ch) {

		case 'f':
			channel_list_filename = string(optarg);
			break;
			
		case 'c':
			num_streams = atoi(optarg);
			if (num_streams > MAX_STREAMS)
			{
				printf("Too many streams.  Max = %d\n", MAX_STREAMS);
				usage();
			}
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
	
	if (strlen(if_name) > 0)
	{
		if (!is_valid_interface(if_name))
		{
			printf("%s is not a valid interface\n", if_name);
			list_interfaces();
			usage();
		}
	}
	else
		strcpy(if_name, "any");
	
	parms.num_streams = num_streams;
	parms.start_nibble = start_nibble;
	parms.base_url = base_url;
	parms.ip_address = ip_address;
	parms.port = port;
	parms.channel_list_filename = channel_list_filename;
	parms.channel_change_delay = channel_change_delay;
	parms.run_once = run_once;
	parms.timeout_exec_filename = timeout_exec_filename;
	
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
		printf("Exception %s during %s on line %d\n", e.what(), __func__, __LINE__);
	}
	catch (...) {
		printf("Unknown Exception during %s on line %d\n", __func__, __LINE__);
	}
}
