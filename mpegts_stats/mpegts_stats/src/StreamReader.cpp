#include "StreamReader.h"

const int MAXRCVSTRING = 4096;

#define RECV_DATA_WAIT_MS 60 * 1000

bool isReadingSocket = false;
bool stopReadingSocket = false;

StreamReader::StreamReader(char *if_name, cCondWait *signalRestartAll, cCondWait *signalShutdownStreamReader, cCondWait *signalStreamReaderStarted, cCondWait *signalStreamReaderStopped, pthread_mutex_t mutex)
{
	stopReadingSocket = false;
	isReadingSocket = false;
	
	m_dataStartedTimer = new cTimeMs();
	m_dataWaitTimer = new cTimeMs();
	m_latencyTimer = new cTimeMs();
	
	m_signalRestartAll = signalRestartAll;
	m_signalShutdownStreamReader = signalShutdownStreamReader;
	m_signalStreamReaderStarted = signalStreamReaderStarted;
	m_signalStreamReaderStopped = signalStreamReaderStopped;
	
	m_if_name = if_name;
	m_isBroken = false;
	m_sock = NULL;
	
	m_mutex = mutex;
	m_childTid = 0;
	m_active = false;
	m_running = false;
}

StreamReader::~StreamReader()
{
//	Close();
//	printf("~StreamReader()\n");
}

void StreamReader::Open(string multicastAddress, int port)
{
	Close();
	
	m_multicastAddress = multicastAddress;
	m_port = port;
	
	Start();

#ifdef DBG_STREAM_READER
	GetTimestamp((char *) &timestamp);
	sprintf(logBuf, "%12lu %s StreamReader Started", m_childTid, timestamp);
	Logger::instance().log(logBuf, Logger::kLogLevelInfo);
	// printf("%s\n", logBuf);
#endif
}

void *StreamReader::StartThread(StreamReader *streamReader)
{
	streamReader->Action();
}

bool StreamReader::Running(void) { 
	return m_running; 
}

bool StreamReader::Active(void)
{
	if (m_active) {
		
		//
		// Single UNIX Spec v2 says:
		//
		// The pthread_kill() function is used to request
		// that a signal be delivered to the specified thread.
		//
		// As in kill(), if sig is zero, error checking is
		// performed but no signal is actually sent.
		//
//		int err;
//		if ((err = pthread_kill(m_childTid, 0)) != 0) {
//			if (err != ESRCH)
//				LOG_ERROR;
//			m_childTid = 0;
//			m_active = m_running = false;
//		}
//		else
		
		return true;
	}
	
	return false;
}


#define THREAD_STOP_TIMEOUT  3000 // ms to wait for a thread to stop before newly starting it
#define THREAD_STOP_SLEEP      30 // ms to sleep while waiting for a thread to stop

pthread_t StreamReader::Start(void)
{
	m_active = m_running = true;

	if (pthread_create(&m_childTid, NULL, (void *(*)(void *))&StartThread, (void *)this) == 0) {
		pthread_detach(m_childTid);    // auto-reap
	}

	return m_childTid;
}

void StreamReader::Cancel(int WaitSeconds)
{
	m_running = false;
	if (m_active && WaitSeconds > -1) {
		if (WaitSeconds > 0) {
			for (time_t t0 = time(NULL) + WaitSeconds; time(NULL) < t0;) {
				if (!Active())
					return;
				cCondWait::SleepMs(10);
			}
			// esyslog("ERROR: %s thread %d won't end (waited %d seconds) - canceling it...", description ? description : "", childThreadId, WaitSeconds);
		}
		pthread_cancel(m_childTid);
		m_childTid = 0;
		m_active = false;
	}
}

void StreamReader::Action(void)
{
	m_recvBuf = (unsigned char *) malloc(MAXRCVSTRING);     
	
	m_cbContext.data = (unsigned char *) malloc(CIRCULAR_BUFFER_SIZE + 1);
	m_cbContext.data[CIRCULAR_BUFFER_SIZE + 1] = 0xAA;   // guard byte
	m_cbContext.thread_id = m_childTid;
	
	m_circularBuffer = new CircularBuffer(&m_cbContext, CIRCULAR_BUFFER_SIZE, m_signalRestartAll);

	m_sock = new UDPSocket(m_port);
	m_sock_fd = m_sock->getHandle();

	if (strcmp(m_if_name, "any") != 0)
	{
		struct ifreq ifr;
		memset(&ifr, 0, sizeof(ifr));
		strcpy((char *) ifr.ifr_name, m_if_name);
		int err = ioctl(m_sock_fd, SIOCGIFINDEX, &ifr);
		if (err < 0) {
			perror("SIOCGIFINDEX");
		}
	
		struct sockaddr_ll sll;
		memset(&sll, 0, sizeof(sll));
		sll.sll_family = AF_PACKET;
		sll.sll_ifindex = ifr.ifr_ifindex;
		sll.sll_protocol = htons(ETH_P_ALL);
		err = bind(m_sock_fd, (struct sockaddr *) &sll, sizeof(sll));
		if (err < 0) {
			perror("bind");
		}

		struct packet_mreq      mr;
		memset(&mr, 0, sizeof(mr));
		mr.mr_ifindex = ifr.ifr_ifindex;
		mr.mr_type = PACKET_MR_PROMISC;
		err = setsockopt(m_sock_fd, SOL_PACKET, PACKET_ADD_MEMBERSHIP, &mr, sizeof(mr));
		if (err < 0) {
			perror("PACKET_MR_PROMISC");
		}
	}
	
	m_sock->joinGroup(m_multicastAddress);
	
	m_dataWaitTimer->Set(RECV_DATA_WAIT_MS);
	
	m_dataStartedTimer->Set();
	m_dataStartedTime = 0;
	m_isDataStarted = false;

	m_signalStreamReaderStarted->Signal();

	prefetch();

	while (Running())
	{
		if (m_signalShutdownStreamReader->WaitUs(10))
		{
			// printf("got m_signalShutdownStreamReader\n");
			Cancel(-1);
			Close();
			break;
		}

		readSocket();
		// cCondWait::SleepMs(1);
		
		if (m_cbContext.data[CIRCULAR_BUFFER_SIZE + 1] != 0xAA)
			printf("Circular buffer is corrupt\n");
	}

	// printf("StreamReader::fillBuffer - exiting thread\n");
}

void StreamReader::Close() 
{
	try
	{
		if (m_sock == NULL)
		{
			return;
		}
				
		// Lock();
		pthread_mutex_lock(&m_mutex);

		m_sock->leaveGroup(m_multicastAddress);
		m_sock->disconnect();

		delete m_sock;
		m_sock = NULL;

		if (m_recvBuf != NULL)
		{
			free(m_recvBuf);
			m_recvBuf = NULL;			
		}

		if (m_cbContext.data)
		{
			free(m_cbContext.data);
			m_cbContext.data = NULL;
		}
			
		if (m_circularBuffer) {
			delete m_circularBuffer;
			m_circularBuffer = NULL;
		}
		
		// Unlock();
		pthread_mutex_unlock(&m_mutex);
			
		m_signalStreamReaderStopped->Signal();
	}
	catch (exception ex)
	{
		m_sock = NULL;
		printf("Exception during StreamReader::Close - %s\n", ex.what());
		pthread_mutex_unlock(&m_mutex);
		// Unlock();
	}
}

void StreamReader::Kill()
{
	Cancel(0);
}

// prime the pump.  Give time to sync up with new streams
bool StreamReader::prefetch()
{
	int ctr = 0;
	bool ret = true;
	
	do
	{
		if (m_signalShutdownStreamReader->WaitUs(1))
		{
			ret = false;
			break;
		}
		
		readSocket();
	} while (ctr++ < 10);
	
	return ret;
}

void StreamReader::readSocket()
{
	fd_set readset;
	struct timeval tv;
	int result;
	int bytesRcvd = 0;
	long bytes_written;
	
	try
	{
		if (m_isBroken) {
			cCondWait::SleepMs(5);
			return;
		}	
		
		FD_ZERO(&readset);
		FD_SET(m_sock_fd, &readset);
	
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		
		isReadingSocket = true;
		
		result = select(m_sock_fd + 1, &readset, NULL, NULL, &tv);
		if (result == -1)
			TSDemux::DBG(DEMUX_DBG_WARN, "Error during StreamReader::readSocket = %d\n", result);

		else if (result)
		{
			if (FD_ISSET(m_sock_fd, &readset)) {
				result = bytesRcvd = m_sock->recv(m_recvBuf, MAXRCVSTRING);
				if (result == 0) {
					/* other side closed the socket */
					// Close();
				}
				else {
					
					total_bytes_received += bytesRcvd;
					
					m_dataWaitTimer->Set(RECV_DATA_WAIT_MS);

//					fprintf(stderr, "latency %llu\n", m_latencyTimer->Elapsed());
//					m_latencyTimer->Set();
	
					if (!m_isDataStarted)
					{
						struct timeval cur_time;
						gettimeofday(&cur_time, NULL);
						data_start_us = (1000000ull * cur_time.tv_sec) + cur_time.tv_usec;						
						
						GetTimestamp((char *) &timestamp);
						sprintf(logBuf, "%12lu %s %s Data started %" PRIu64 "ms", m_childTid, timestamp, m_multicastAddress.c_str(), m_dataStartedTimer->Elapsed());
						Logger::instance().log(logBuf, Logger::kLogLevelInfo);
						printf("%s\n", logBuf);
						
						m_dataStartedTime = m_dataStartedTimer->Elapsed();
						m_isDataStarted = true;
					}
					
					// Lock();
					pthread_mutex_lock(&m_mutex);
					bytes_written = m_circularBuffer->write(m_recvBuf, bytesRcvd);
					pthread_mutex_unlock(&m_mutex);
					// Unlock();
					
					
					if (bytes_written < 0) {
						m_isBroken = true;
						cCondWait::SleepMs(2);
					}
				}
			}
		}
		
		if (m_dataWaitTimer->TimedOut())
		{
			GetTimestamp((char *) &timestamp);
			sprintf(logBuf, "%12lu %s %s StreamReader::readSocket - Timed out waiting for data", m_childTid, timestamp, m_multicastAddress.c_str());
			Logger::instance().log(logBuf, Logger::kLogLevelInfo);
			printf("%s\n", logBuf);
			
			m_isBroken = true;
			
			return;

//			m_dataWaitTimer->Set(RECV_DATA_WAIT_MS);
//			m_signalRestartAll->Signal();
//			cCondWait::SleepMs(10);
		}
	}
	catch (exception ex)
	{
		TSDemux::DBG(DEMUX_DBG_WARN, "Error during StreamReader::readSocket - %s\n", ex.what());
	}
}

long StreamReader::read(unsigned char *data, uint64_t pos, size_t bytes)
{
	if (m_isBroken)
		return -1;
	
	// Lock();
	pthread_mutex_lock(&m_mutex);
	long bytesRead = m_circularBuffer->read(data, pos, bytes);
	pthread_mutex_unlock(&m_mutex);
	// Unlock();
	
	if (bytesRead < 0)
		m_isBroken = true;
	
	return bytesRead;
}

size_t StreamReader::GetCBMaxSize()
{
	size_t result;
	
	pthread_mutex_lock(&m_mutex);
	result = m_circularBuffer->GetMaxSize();
	pthread_mutex_unlock(&m_mutex);
	
	return result;
}

