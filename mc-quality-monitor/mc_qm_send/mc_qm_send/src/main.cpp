#include <string>
#include <iostream>           // For cout and cerr
#include <sstream>
#include <cstdlib>            // For atoi()
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

#include "tools.h"
#include "thread.h"
#include "Multicaster.h"

#define PACKAGE_VERSION "0.1.0"

#define BUF_SIZE 188

using namespace std;

string         m_nic = "any";
string         m_if_address = "";
// string         m_if_address = "10.153.0.10";
string         m_address = "239.128.128.128";
int            m_port = 5001;
int            m_ttl  = 32;
sockaddr_in    m_dest;

int            max_jitter_us    =  30000; //  30 ms
int            max_latency_us   = 300000; // 300 ms
int            data_interval_ms =      3; //   3 ms

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
	printf("Usage: %s [-iptnljdv]\n", prognm);
	printf("  -i, --ip=MC_ADDRESS       default: %s\n", m_address.c_str());
	printf("  -p, --port=MC_PORT        default: %d\n", m_port);
	printf("  -t, --ttl=MC_TTL          default: %d\n", m_ttl);
	printf("  -n, --nic=MC_NETWORK_IF   default: %s\n", m_nic.c_str());
	printf("  -l, --latency=MAX_LATENCY default: %d ms\n", max_latency_us / 1000);
	printf("  -j, --jitter=MAX_JITTER   default: %d ms\n", max_jitter_us / 1000);
	printf("  -d, --delay=DATA_INTERVAL default: %d ms\n", data_interval_ms);
	printf("  -v, --version             Show version\n");
	printf("  -?, --help                This message\n");
	printf("\n");
	
	exit(0);
}
int main(int argc, char *argv[]) {

		struct option long_options[] = {
		{ "ip", 1, 0, 'i' },
		{ "port", 1, 0, 'p' },
		{ "ttl", 1, 0, 't' },
		{ "nic", 1, 0, 'n' },
		{ "latency", 1, 0, 'l' },
		{ "jitter", 1, 0, 'j' },
		{ "delay", 1, 0, 'd' },
		{ "version", 0, 0, 'v' },
		{ "help", 0, 0, '?' },
		{ NULL, 0, 0, 0 }
	};

	char versionstring[100];
	int   ch;
	
	prognm = progname(argv[0]);

	if (geteuid() != 0)
	{
		printf("Need root privileges to start\n");
		usage();
	}
	
	snprintf(versionstring, sizeof(versionstring), "%s version %s", prognm, PACKAGE_VERSION);

	while ((ch = getopt_long(argc, argv, "i:p:t:n:l:j:d:v?", long_options, NULL)) != EOF) {

		switch (ch) {

		case 'i':
			m_address = string(optarg);
			break;

		case 'p':
			m_port = atoi(optarg);
			break;

		case 't':
			m_ttl = atoi(optarg);
			break;

		case 'n':
			m_nic = string(optarg);
			break;
			
		case 'l':
			max_latency_us = atoi(optarg) * 1000;
			break;
			
		case 'j':
			max_jitter_us = atoi(optarg) * 1000;
			break;

		case 'd':
			data_interval_ms = atoi(optarg);
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
	
	if (!is_valid_interface((char *) m_nic.c_str()))
	{
		printf("%s is not a valid interface\n", m_nic.c_str());
		list_interfaces();
		usage();
	}

	if (m_nic.compare("any") != 0)
		m_if_address = string(get_ip_address((char *) m_nic.c_str()));
	
	srand(time(NULL));
	
	Multicaster *m_sender = new Multicaster();
	m_sender->Open(m_address.c_str(), m_if_address.c_str(), m_port, m_ttl, true);

	timeval cur_time;
	
	typedef struct data_packet_t
	{
		int data_len;
		int checksum;
		unsigned long long seq;
		unsigned long long cur_time_us;
		int max_jitter_us;
		int max_latency_us;
		int data_interval_ms;
		int dummy;
	} data_packet_t;
	
	data_packet_t data_packet;
	
	data_packet.seq = 0;
	data_packet.data_interval_ms = data_interval_ms;
	data_packet.data_len = BUF_SIZE - sizeof(data_packet_t);
	data_packet.max_latency_us = max_latency_us;
	data_packet.max_jitter_us = max_jitter_us;

	unsigned char *buffer = (unsigned char *) malloc(BUF_SIZE);	
	unsigned char *data_ptr = buffer + sizeof(data_packet_t);

	while (1)
	{
		data_packet.seq++;

		gettimeofday(&cur_time, NULL);
		data_packet.cur_time_us = (1000000ull * cur_time.tv_sec) + cur_time.tv_usec;
		data_packet.checksum = 0;

//		memset((void *) buffer, 0xFF, BUF_SIZE);
//		for (int i = 0; i < data_packet.data_len; i++)
//			data_ptr[i] = i;
		
		rand_string(data_ptr, data_packet.data_len);
		memcpy((void *) buffer, (void *) &data_packet, sizeof(data_packet_t));

		int checksum = inet_cksum((unsigned char *) buffer, BUF_SIZE);
		data_packet_t *p = (data_packet_t *) buffer;
		p->checksum = checksum;

		m_sender->Send(buffer, BUF_SIZE);
		
		cCondWait::SleepMs(data_interval_ms);
	}
}

