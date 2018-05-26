#ifndef STREAMER_READER_H
#define STREAMER_READER_H

#include <inttypes.h>

#include "tools.h"
#include "PracticalSocket.h"
#include "Logger.h"

using namespace std;

#define MAXRCVSTRING 4096
#define DATA_WAIT_MS 120 * 1000

typedef struct mcparms_t {
	string iif_address;
	string address;
	int port;
} mcparms_t;

class StreamReader {

public:	
	StreamReader();
	~StreamReader();

	void Start(mcparms_t *parms);
	void Join();

	pthread_t m_tid;

	static void *ReadStream(void *arg);
};

#endif