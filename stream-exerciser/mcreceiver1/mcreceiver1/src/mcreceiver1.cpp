#include <iostream>          // For cout and cerr
#include <sstream>
#include <cstdlib>           // For atoi()

#include <getopt.h>

#include "PracticalSocket.h" // For UDPSocket and SocketException
#include "StreamReader.h"

#define PACKAGE_VERSION "1.0.8"

int num_streams = 1;
int start_nibble = 1;
string base_url = string("239.0.1.");
string ip_address;
int port = 5001;
string iif_address;

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
	printf("Usage: %s [-nbispov]\n", prognm);
	printf("  -n, --streams=NUM_STREAMS Number of streams - default 1\n");
	printf("  -b, --base=BASE_URL       (eg. 239.0.2.) - default %s\n", base_url.c_str());
	printf("  -i, --ip=AP_ADDRESS       (eg. 239.0.2.1)\n");
	printf("  -s, --start=START_NIBBLE  ip start nibble (eg 50 will start at <BASE_URL>.50");
	printf("  -p, --port=PORT           default %d\n", port);
	printf("  -f, --iif=IIF             Input interface name - default IP_ANY\n");
	printf("  -v, --version             Show version\n");
	printf("  -?, --help                This message\n");
	printf("\n");
	
	exit(0);
}

string getIPAddress(string ifName) {
	
	string ipAddress;

	struct ifaddrs *interfaces = NULL;
	struct ifaddrs *temp_addr = NULL;
	
	int success = getifaddrs(&interfaces);
	if (success == 0) {

		temp_addr = interfaces;
		while (temp_addr != NULL) {
			if (temp_addr->ifa_addr->sa_family == AF_INET) {
				if (strcmp(temp_addr->ifa_name, ifName.c_str()) == 0) {
					ipAddress = inet_ntoa(((struct sockaddr_in*)temp_addr->ifa_addr)->sin_addr);
					break;
				}
			}
			
			temp_addr = temp_addr->ifa_next;
		}
	}

	freeifaddrs(interfaces);
	
	return ipAddress;
}

int main(int argc, char *argv[]) {

	struct option long_options[] = {
		{ "streams", 1, 0, 'n' },
		{ "base", 1, 0, 'b' },
		{ "ip", 1, 0, 'i' },
		{ "start", 1, 0, 's' },
		{ "port", 1, 0, 'p' },
		{ "iif", 1, 0, 'f' },
		{ "version", 0, 0, 'v' },
		{ "help", 0, 0, '?' },
		{ NULL, 0, 0, 0 }
	};

	char versionstring[100];
	int   ch;
	
	prognm = progname(argv[0]);

	snprintf(versionstring, sizeof(versionstring), "%s version %s", prognm, PACKAGE_VERSION);

	while ((ch = getopt_long(argc, argv, "n:b:i:s:f:v?", long_options, NULL)) != EOF) {

		switch (ch) {

		case 'n':
			num_streams = atoi(optarg);
			break;
			
		case 'b':
			base_url = string(optarg);
			break;

		case 'i':
			ip_address = string(optarg);
			break;

		case 's':
			start_nibble = atoi(optarg);
			break;

		case 'p':
			port = atoi(optarg);
			break;
			
		case 'f':
			iif_address = getIPAddress(string(optarg));
			
			if (iif_address.empty())
			{
				printf("Unable to get ip address for oif %s\n\n", optarg);
				usage();
			}
			
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
	
	if (num_streams > 254)
	{
		printf("num_streams cannot exceed 255\n");
		exit(0);
	}
		
	if (!ip_address.empty()) {
		num_streams = 1;
		printf("%-15s: %s\n", "IP Address", ip_address.c_str());
	}
	else
		printf("%-15s: %s\n", "Base Url", base_url.c_str());

	printf("%-15s: %d\n", "Num Streams", num_streams);

	if (!iif_address.empty())
		printf("%-15s: %s\n", "IIF IP", iif_address.c_str());
	
	printf("%-15s: %d\n\n", "Port", port);
	
	srand(time(NULL));
	struct timespec req;
	
	StreamReader *streamer[num_streams];
	
	for (int i = 0; i < num_streams; i++)
	{
		mcparms_t *mcparms = (mcparms_t *) malloc(sizeof(mcparms_t));	
		new(mcparms) mcparms_t;
		
		if (ip_address.empty())
		{
			std::ostringstream out;
			out << base_url << start_nibble + i ;
		
			mcparms->address = out.str();
		}
		else
			mcparms->address = ip_address;
		
		mcparms->port = port;
		mcparms->iif_address = iif_address;

		char timestamp[100];
		GetTimestamp((char *) &timestamp);
		printf("%-16s %-30s Starting listener - %d\n", mcparms->address.c_str(), timestamp, i + 1);

		streamer[i] = new StreamReader();
		streamer[i]->Start(mcparms);
		
		// req.tv_sec = (rand() % 30); 
		req.tv_sec = 1; 
		req.tv_nsec = 0;

		nanosleep(&req, &req);
	}

	for (int i = 0; i < num_streams; i++)
		streamer[i]->Join();
	
	return 0;
}