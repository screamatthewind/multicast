#include "packet_sniffer.h"

#define PACKAGE_VERSION "1.0.8"

using namespace std;

char *prognm = NULL;
char if_name[32];

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
	printf("Usage: %s [-nv]\n", prognm);
	printf("  -n  --nic=NETWORK_IF        network interface name - default ANY\n");
	printf("  -v, --version               Show version\n");
	printf("  -?, --help                  This message\n");
	printf("\n");
	
	exit(0);
}

void clean_up()
{
	if (buffer != NULL)
	{
		free(buffer);
		buffer = NULL;
	}

	if (sock_raw > 0)
		close(sock_raw);
	
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

void init(int argc, char *argv[])
{
	struct option long_options[] = {
		{ "nic", 1, 0, 'n' },
		{ "version", 0, 0, 'v' },
		{ "help", 0, 0, '?' },
		{ NULL, 0, 0, 0 }
	};

	char versionstring[100];
	int   ch;
	
	prognm = progname(argv[0]);
	snprintf(versionstring, sizeof(versionstring), "%s version %s", prognm, PACKAGE_VERSION);

	if_name[0] = 0;

	if (geteuid() != 0)
	{
		printf("Need root privileges to start\n");
		usage();
	}

	while ((ch = getopt_long(argc, argv, "i:n:p:mgdv?", long_options, NULL)) != EOF) {

		switch (ch) {

		case 'n':
			strcpy(if_name, optarg);
			break;
			
		case 'v':
			printf("%s\n", versionstring);
			exit(0);
			
		case '?':
			usage();

		default:
			usage();
		}
	}

	if (strlen(if_name) == 0)
	{
		printf("Interface option is required\n");
		list_interfaces();
		usage();
	}

	if ((strlen(if_name) && (!is_valid_interface(if_name))))
	{
		printf("%s is not a valid interface\n", if_name);
		list_interfaces();
		usage();
	}
	
	configureSignalHandlers();
}

int main(int argc, char *argv[])
{
	init(argc, argv);
	monitor_udp((char *) &if_name);
}