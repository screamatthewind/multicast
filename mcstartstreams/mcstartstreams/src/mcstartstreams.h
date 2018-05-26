#ifndef STREAMER_READER_H
#define STREAMER_READER_H

#include <inttypes.h>

#include "tools.h"
#include "PracticalSocket.h"

using namespace std;

#define MAXRCVSTRING 4096
#define DATA_WAIT_MS 120 * 1000

struct MCParmsStruct {
	int channelNum;
	string channelName;
	string multicastAddress;
	int port;
} MCParmsStruct;

class Streamer {

public:	
	Streamer();
	~Streamer();

	void Start(struct MCParmsStruct *parms);
	void Join();

	pthread_t m_tid;

	static void *ReadStream(void *arg);

};

#endif