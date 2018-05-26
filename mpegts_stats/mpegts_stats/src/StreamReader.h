#ifndef STREADM_READER_H
#define STREADM_READER_H

#include "PracticalSocket.h"
#include "CircularBuffer.h"
#include "thread.h"
#include "Logger.h"
#include "tools.h"
#include "debug.h"
#include "defs.h"

#define CIRCULAR_BUFFER_SIZE 1000000

class StreamReader
{
public:
	StreamReader(char *if_name, cCondWait *signalRestartAll, cCondWait *signalShutdownStreamReader, cCondWait *signalStreamReaderStarted, cCondWait *signalStreamReaderStopped, pthread_mutex_t mutex);
	~StreamReader();

	long read(unsigned char *data, uint64_t pos, size_t bytes);

	void Open(string multicastAddress, int port);
	void Close();
	void Kill();
	
	size_t GetCBMaxSize();

	CircularBuffer *m_circularBuffer;
	CircularBufferContext m_cbContext;

	cTimeMs *m_latencyTimer;
	cTimeMs *m_dataWaitTimer;
	cTimeMs *m_dataStartedTimer;
	uint64_t m_dataStartedTime;

	cCondWait *m_signalRestartAll;
	cCondWait *m_signalShutdownStreamReader;
	cCondWait *m_signalStreamReaderStarted; 
	cCondWait *m_signalStreamReaderStopped;	
	
	bool m_isDataStarted;
	bool m_isBroken;

	int m_port;
	string m_multicastAddress;
	char *m_if_name;
	
	int ret;

	UDPSocket *m_sock;
	int m_sock_fd;
	
	unsigned char *m_recvBuf;

	bool prefetch();
	void readSocket();
	
	static void *StartThread(StreamReader *streamReader);
	pthread_t Start(void);
	bool Running(void); 

	virtual void Action(void);
	void Cancel(int WaitSeconds);
	bool Active(void);
	
	pthread_mutex_t m_mutex;
	bool m_running, m_active;
	pthread_t m_childTid;
	
	char logBuf[255];
	char timestamp[100];
	
	unsigned long long data_start_us;
	unsigned long long total_bytes_received;
};

#endif


