#include <iostream>
#include <thread>

#include "StreamReader.h"

#define JOIN_SECS 10 * 60
#define LEAVE_SECS 3 * 60

using namespace std;

StreamReader::StreamReader()
{
}

void StreamReader::Start(mcparms_t *parms)
{
	pthread_create(&m_tid, 0, ReadStream, parms);
}

void StreamReader::Join()
{
	pthread_join(m_tid, 0);
}

void *StreamReader::ReadStream(void *arg)
{
	struct mcparms_t *parms = (struct mcparms_t*) arg;

	string m_iifAddress = parms->iif_address;
	string m_multicastAddress = parms->address;
	int m_port = parms->port;	
	
	int m_sock_fd;
	UDPSocket *m_sock;

	cTimeMs *m_runTimer = new cTimeMs();
	cTimeMs *m_dataWaitTimer = new cTimeMs();
	cTimeMs *m_dataStartedTimer = new cTimeMs();
	uint64_t m_dataStartedTime;
	
	bool m_isDataStarted;

	unsigned char *m_recvBuf;
	m_recvBuf = (unsigned char *) malloc(MAXRCVSTRING);     
	m_sock = new UDPSocket(m_port);

	char buf[255];
	char timestamp[100];

	while (1)
	{
		int waitTimer = ((rand() % JOIN_SECS) * 1000) + 500; 
		
		char timestamp[100];
		GetTimestamp((char *) &timestamp);
		sprintf(buf, "%-16s %-30s Joining for %d Secs", m_multicastAddress.c_str(), timestamp, waitTimer / 1000);
		Logger::instance().log(buf, Logger::kLogLevelInfo);
		printf("%s\n", buf);
		
		m_runTimer->Set(waitTimer);
		
		m_sock->joinGroup(m_multicastAddress, m_iifAddress);
		m_sock_fd = m_sock->getHandle();
	
		m_dataWaitTimer->Set(DATA_WAIT_MS);
	
		m_dataStartedTimer->Set();
		m_dataStartedTime = 0;
		m_isDataStarted = false;

		fd_set readset;
		struct timeval tv;
		int result;
		int bytesRcvd = 0;
		long bytes_written;

		while (!m_runTimer->TimedOut())
		{
			try
			{
				FD_ZERO(&readset);
				FD_SET(m_sock_fd, &readset);
	
				tv.tv_sec = 1;
				tv.tv_usec = 0;
		
				result = select(m_sock_fd + 1, &readset, NULL, NULL, &tv);
				if (result == -1)
					printf("Error during StreamReader::readSocket = %d\n", result);

				else if (result)
				{
					if (FD_ISSET(m_sock_fd, &readset)) {
						result = bytesRcvd = m_sock->recv(m_recvBuf, MAXRCVSTRING);
						if (result == 0) {
							/* other side closed the socket */
							GetTimestamp((char *) &timestamp);
							sprintf(buf, "%-16s %-30s Socked closed", m_multicastAddress.c_str(), timestamp);
							Logger::instance().log(buf, Logger::kLogLevelInfo);
							printf("%s\n", buf);
						}
						else {
					
							m_dataWaitTimer->Set(DATA_WAIT_MS);

							if (!m_isDataStarted)
							{
								GetTimestamp((char *) &timestamp);
								sprintf(buf, "%-16s %-30s Data started %" PRIu64 "ms", m_multicastAddress.c_str(), timestamp, m_dataStartedTimer->Elapsed());
								Logger::instance().log(buf, Logger::kLogLevelInfo);
								printf("%s\n", buf);

								m_dataStartedTime = m_dataStartedTimer->Elapsed();
								m_isDataStarted = true;
							}
						}
					}
				}
		
				if (m_dataWaitTimer->TimedOut())
				{
					GetTimestamp((char *) &timestamp);
					sprintf(buf, "%-16s %-30s Timed out", m_multicastAddress.c_str(), timestamp);
					Logger::instance().log(buf, Logger::kLogLevelInfo);
					printf("%s\n", buf);

					break;
				}
			}
			catch (exception ex)
			{
				printf("Error during StreamReader::readSocket - %s\n", ex.what());
			}
		}
		
		waitTimer = (rand() % LEAVE_SECS) + 1; 

		GetTimestamp((char *) &timestamp);
		sprintf(buf, "%-16s %-30s Leaving for %d Secs", m_multicastAddress.c_str(), timestamp, waitTimer);
		Logger::instance().log(buf, Logger::kLogLevelInfo);
		printf("%s\n", buf);

		m_sock->leaveGroup(m_multicastAddress);
		
		struct timespec req;
		
		req.tv_sec = waitTimer; 
		req.tv_nsec = 0;

		nanosleep(&req, &req);
	}
}
