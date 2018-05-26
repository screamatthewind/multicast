#ifndef __CIRCULARBUFFER_INCLUDED__
#define __CIRCULARBUFFER_INCLUDED__

#include <algorithm> // for std::min
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string>
#include <string.h>
#include <inttypes.h>

#include "Logger.h"
#include "tools.h"
#include "thread.h"

struct CircularBufferContext {
	size_t beg_index;
	size_t end_index;
	size_t size;
	size_t capacity;
	size_t position;
	size_t max_size;
	pthread_t thread_id;
	unsigned char *data;
};
		
class CircularBuffer
{
public:
	CircularBuffer(CircularBufferContext *cbContext, size_t capacity, cCondWait *signalRestartAll);
	~CircularBuffer();

	// size_t size() const { return size_; }
	size_t capacity();
	long write(const unsigned char *data, size_t bytes);
	long read(unsigned char *data, uint64_t pos, size_t bytes);
	void close();
	size_t GetMaxSize();
		
private:
	CircularBufferContext *m_cbContext;
		
	bool m_isBroken;
	bool m_ignorePosition;
	uint64_t m_prevPosition;

	char logBuf[255];
	char timestamp[100];

	cCondWait *m_signalRestartAll;
		
	void calculateSize();
	bool isEmpty();
};
#endif