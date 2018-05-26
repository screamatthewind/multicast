
#include <string>
#include <iostream>           // For cout and cerr
#include <sstream>
#include <cstdlib>            // For atoi()
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <inttypes.h>
#include <math.h>

// #include "Logger.h"
#include "CsvWriter.h"
#include "Multicaster.h"
#include "tools.h"
#include "thread.h"
#include "lirc_send.h"

#define PACKAGE_VERSION "0.9.8"
#define BUF_SIZE 188
#define NUM_BUFS 5
#define LIRC_TIMER 200 // 200 ms
#define MIN_TIME_US 1517997165188603ull // sanity check for date time

using namespace std;

string         m_address = "239.128.128.128";
int            m_port = 5001;
Multicaster   *m_mcreader;
int            m_sock;
cTimeMs       *m_lircTimer = new cTimeMs();

unsigned char  *m_buffer;

int            packet_loss_ctr = 0;
int            data_timeout_ms = 3000;  //   3 secs
int            can_show_jitter_ctr = 0;
bool           debug = false;

unsigned long long latencies[NUM_BUFS];

float mean_us, stddev_us, jitter_us;

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

data_packet_t m_data_packets[NUM_BUFS], m_sorted_data_packets[NUM_BUFS], m_prev_data_packet;
int           m_cur_buf_ptr = 0;
uint32_t      m_packet_ctr = 0;

string m_csvFileHeader;
char m_csvFileTimestamp[255];

char *prognm = NULL;
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
	printf("Usage: %s [-ipljdv]\n", prognm);
	printf("  -i, --ip=IP_ADDRESS       default: %s\n", m_address.c_str());
	printf("  -p, --port=PORT           default: %d\n", m_port);
	printf("  -d, --debug               Show all data - default=false\n");
	printf("  -v, --version             Show version\n");
	printf("  -?, --help                This message\n");
	printf("\n");
	
	exit(0);
}

void CalcStats()
{
	float sum = 0.0;
	
	mean_us = 0.0;
	stddev_us = 0.0;
	jitter_us = 0.0;
	
	for (int i = 0; i < NUM_BUFS; i++)
	{
		if (i > 0)
			jitter_us += fabs((float) latencies[i] - latencies[i - 1]);

		sum += (float) latencies[i];
	}
	
	mean_us = sum / (float) NUM_BUFS;
	jitter_us = jitter_us / (float) NUM_BUFS - 1;

	for (int i = 0; i < NUM_BUFS; ++i)
		stddev_us += pow((float) (float) latencies[i] - mean_us, 2);
	
	stddev_us = sqrt(stddev_us / (float) NUM_BUFS);
}

void WriteStbUserData(char *status)
{
	if (!m_lircTimer->TimedOut())
		return;
	
	char data[255];
	
	sprintf(data, "{\"title\":\"Data Quality Monitor\",\"status\":\"%s\"}", status);
	
	FILE *f = fopen("/mnt/Userfs/data/dataQuality.json", "w");
	fprintf(f, "%s", data);
	fclose(f);
	
	lirc_send_key(YELLOW);

	m_lircTimer->Set(LIRC_TIMER);
	
	packet_loss_ctr = 0;
}

int comparator(const void *p, const void *q) 
{
    unsigned long long l = ((data_packet_t *)p)->seq;
    unsigned long long r = ((data_packet_t *)q)->seq; 

	if (l == r)
		return 0;
	
	else if (l < r)
		return -1;
	
	else
		return 1;
}

void SortBuffers()
{
	memcpy((void *) &m_sorted_data_packets, (void *) &m_data_packets, sizeof(data_packet_t) * NUM_BUFS);
	qsort((void*) &m_sorted_data_packets, NUM_BUFS, sizeof(data_packet_t), comparator);
}

void LogStatus(char *status) {
	char timestamp[128];
	char tmpBuf[255];
	
	if (!debug)
	{
		GetTimestamp((char *) &timestamp);
		sprintf(tmpBuf, "%s, %s", timestamp, status);
		CsvWriter::instance(m_csvFileHeader).write(tmpBuf);
	}

	fprintf(stderr, "%s\n", status);
}

int main(int argc, char *argv[]) {

		struct option long_options[] = {
		{ "ip", 1, 0, 'i' },
		{ "port", 1, 0, 'p' },
		{ "debug", 0, 0, 'd' },
		{ "version", 0, 0, 'v' },
		{ "help", 0, 0, '?' },
		{ NULL, 0, 0, 0 }
	};

	char versionstring[100];
	int   ch;
	
	prognm = progname(argv[0]);

	snprintf(versionstring, sizeof(versionstring), "%s version %s", prognm, PACKAGE_VERSION);

	while ((ch = getopt_long(argc, argv, "i:p:l:j:dv?", long_options, NULL)) != EOF) {

		switch (ch) {

		case 'i':
			m_address = string(optarg);
			break;

		case 'p':
			m_port = atoi(optarg);
			break;
			
		case 'd':
			debug = true;
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
	
	// give time for O/S to start
	cCondWait::SleepMs(2000);
		
	char tmpBuf[255];
	char status[32];
	
	bool first_time = true;
	
	timeval cur_time;
	unsigned long long latency_us, cur_time_us;
	unsigned long ts_usec, data_interval_us;

	long seq_delta;
	bool no_data = false;

	int recv_len;
	bool calc_stats = false;
	int checksum, pkt_checksum;

	m_buffer = (unsigned char*) malloc(BUF_SIZE);

	m_mcreader = new Multicaster();
	while (m_mcreader->Open(m_address.c_str(), m_port, 0, false) <= 0)
		cCondWait::SleepMs(2000);

	m_csvFileHeader = string("Timestamp, Seq Delta, Latency us, Jitter us, Status");
	CsvWriter::instance(m_csvFileHeader);

	// strcpy(m_csvFileTimestamp, CsvWriter::instance(m_csvFileHeader).getFileTimestamp());

	while (1)
	{
		recv_len = m_mcreader->Receive(m_buffer, BUF_SIZE, data_timeout_ms);
		
		if (recv_len == BUF_SIZE)
		{
			memcpy((void *) &m_data_packets[m_cur_buf_ptr], (void *) m_buffer, sizeof(data_packet_t));

			pkt_checksum = m_data_packets[m_cur_buf_ptr].checksum;
			data_packet_t *p = (data_packet_t *) m_buffer;
			p->checksum = 0;
			
			checksum = inet_cksum(m_buffer, BUF_SIZE);
			
			if (checksum - pkt_checksum != 0)
			{
				sprintf(tmpBuf, "0, 0, 0, Bad Checksum");
				
				WriteStbUserData(tmpBuf);
				LogStatus(tmpBuf);

				continue;
			}
			
			if (no_data)
			{
				sprintf(tmpBuf, "0, 0, 0, Got Data");
				
				WriteStbUserData(tmpBuf);
				LogStatus(tmpBuf);
				
				no_data = false;
				first_time = true;
			}

			if (first_time)
			{
				data_interval_us = m_data_packets[m_cur_buf_ptr].data_interval_ms * 1000;
				first_time = false;
			}
						
			// give time for wall clock time to be set.  toss out the first 1000 packets
			m_packet_ctr++;
			if (m_packet_ctr < 1000)
				goto skip;
			
			if (m_packet_ctr == 1000)
			{
				sprintf(tmpBuf, "0, 0, 0, Started Monitoring - v%s", PACKAGE_VERSION);
				LogStatus(tmpBuf);
				WriteStbUserData(tmpBuf);
			}
			
			gettimeofday(&cur_time, NULL);
			cur_time_us = (1000000ull * cur_time.tv_sec) + cur_time.tv_usec;

			// sanity check for date/time
			if ((cur_time_us < MIN_TIME_US) || (m_data_packets[m_cur_buf_ptr].cur_time_us < MIN_TIME_US))
				continue;
						
			strcpy(status, "Ok");

			// sanity check -- this should never happen
			if (cur_time_us < m_data_packets[m_cur_buf_ptr].cur_time_us)
			{
				WriteStbUserData((char *) & "0, 0, 0, Timestamp is invalid");
				goto skip;
			}
			
			latency_us = cur_time_us - m_data_packets[m_cur_buf_ptr].cur_time_us;
			if (latency_us > data_interval_us) // this happens on the first packet
				latency_us -= data_interval_us;
			
			// sanity check
			if (latency_us > 10000000ull)
				goto skip;
			
			latencies[m_cur_buf_ptr] = latency_us;

			SortBuffers();
			CalcStats();

			// jitter calculation will always be wrong after a high latency event
			if (can_show_jitter_ctr > 0)
				can_show_jitter_ctr--;
				
//			if (m_data_packets[m_cur_buf_ptr].seq - m_prev_data_packet.seq != 1)
//			{
//				sprintf(status, "Packet out of order");
//				WriteStbUserData(status);
//			}

			if ((can_show_jitter_ctr == 0) && (jitter_us > m_data_packets[m_cur_buf_ptr].max_jitter_us))
			{
				sprintf(status, "High Jitter: %.0f ms", jitter_us / 1000);
				WriteStbUserData(status);
			}
				
			if (latency_us > m_data_packets[m_cur_buf_ptr].max_latency_us)
			{
				sprintf(status, "High Latency: %llu ms", latency_us / 1000);
				WriteStbUserData(status);
				can_show_jitter_ctr = NUM_BUFS;
			}
			
			// handle wrap-around
			if (m_sorted_data_packets[0].seq != 0)
			{
				seq_delta = m_sorted_data_packets[1].seq - m_sorted_data_packets[0].seq;
				if (seq_delta != 1)
				{
					packet_loss_ctr += seq_delta;
					
					sprintf(status, "%ld Packets Lost", seq_delta);
					WriteStbUserData(status);
				}
			}
			else 
				seq_delta = 1;

			if (debug || (strcmp(status, "Ok") != 0))
			{
				// sprintf(tmpBuf, "seq: %ld latency: %5llu ms avg: %8.0f stddev: %8.0f jitter: %8.0f status: %s", 
				sprintf(tmpBuf, "%ld, %llu, %.0f, %s", 
					seq_delta, 
					latency_us / 1000,
					jitter_us / 1000,
					status);
					
				LogStatus(tmpBuf);
			}
			
skip:
			memcpy((void *) &m_prev_data_packet, (void *) &m_data_packets[m_cur_buf_ptr], sizeof(data_packet_t));
			
			m_cur_buf_ptr++;
			if (m_cur_buf_ptr >= NUM_BUFS)
				m_cur_buf_ptr = 0;
		}
		
		else if (recv_len < 0)
		{
			LogStatus((char *) & "0, 0, 0, Error during receive data");
			WriteStbUserData((char *) &"Error during receive data");
		}

		else if (recv_len == 0)
		{
			if (!no_data)
			{
				LogStatus((char *) &"0, 0, 0, No Data");
				WriteStbUserData((char *) &"No Data");
				no_data = true;
			}
		}

		else if (recv_len > 0)
		{
			sprintf(tmpBuf, "0, 0, 0, Bad Packet - wrong size %d", recv_len);
			LogStatus(tmpBuf);
			WriteStbUserData(tmpBuf);
		}
	}
}

