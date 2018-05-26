#include <iostream>
#include <thread>

#include "mcstartstreams.h"

using namespace std;

Streamer::Streamer()
{
}

void Streamer::Start(struct MCParmsStruct *parms)
{
	pthread_create(&m_tid, 0, ReadStream, parms);
}

void Streamer::Join()
{
	pthread_join(m_tid, 0);
}

void *Streamer::ReadStream(void *arg)
{
	struct MCParmsStruct *parms = (struct MCParmsStruct*) arg;

	string m_multicastAddress = parms->multicastAddress;
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

	while (1)
	{
		m_runTimer->Set(60 * 1000);
		
		m_sock->joinGroup(m_multicastAddress);
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

//		while (!m_runTimer->TimedOut())
		while (1)
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
							// Close();
						}
						else {
					
							m_dataWaitTimer->Set(DATA_WAIT_MS);

							if (!m_isDataStarted)
							{
								char buf[256];
								sprintf(buf, "%-30s Data started %" PRIu64 "ms", parms->channelName.c_str(), m_dataStartedTimer->Elapsed());
								// Logger::instance().log(string(buf), Logger::kLogLevelInfo);
								printf("%s\n", buf);
						
								m_dataStartedTime = m_dataStartedTimer->Elapsed();
								m_isDataStarted = true;
							}
						}
					}
				}
		
				if (m_dataWaitTimer->TimedOut())
				{
					char timestamp[100];
					GetTimestamp((char *) &timestamp);
			
					char buf[256];
					sprintf(buf, "%-30s Timed out waiting for data", parms->channelName.c_str());
					// Logger::instance().log(string(buf), Logger::kLogLevelError);
					printf("%s\n", buf);

					m_dataWaitTimer->Set(DATA_WAIT_MS);
					// m_signalRestartAll->Signal();
					// cCondWait::SleepMs(10);

					break;
				}
			}
			catch (exception ex)
			{
				printf("Error during StreamReader::readSocket - %s\n", ex.what());
			}
		}
		
		m_sock->leaveGroup(m_multicastAddress);
	}
}

int main(int argc, char *argv[])
{
	struct MCParmsStruct parms[] = {
		{ 12, "WNEM 5 - 1 CBS", "239.254.0.1", 7399 }, 
		{ 13, "WNEM 5 - 2 My 5", "239.254.0.2", 7399 }, 
		{ 14, "WNEM 5 - 3 Cozi TV", "239.254.0.3", 7399 },
		{ 15, "WJRT 12 - 1 ABC", "239.254.0.4", 7399 },
		{ 16, "WJRT 12 - 2 MeTV", "239.254.0.5", 7399 },
		{ 17, "WDCQ 19 - 1 PBS", "239.254.0.6", 7399 },
		{ 18, "WEYI 25 - 1 NBC", "239.254.0.7", 7399 },
		{ 19, "WEYI 46 - 1 CW", "239.254.0.8", 7399 },
		{ 20, "WEYI 25 - 3 Charge!", "239.254.0.9", 7399 },
		{ 21, "WAQP 49 - 1 TCT", "239.254.0.10", 7399 },
		{ 22, "WSMH 66 - 1 FOX", "239.254.0.11", 7399 },
		{ 24, "WSMH 66 - 3 Comet TV", "239.254.0.12", 7399 }
	};

	Streamer *streamer[12];
	
	struct timespec req;

	srand(time(NULL));
	
	for (int i = 0; i < 12; i++)
	{
		streamer[i] = new Streamer();
		streamer[i]->Start(&parms[i]);
		
		req.tv_sec = (rand() % 60); 
		req.tv_nsec = 0;

		nanosleep(&req, &req);
	}

	for (int i = 0; i < 12; i++)
		streamer[i]->Join();

	
//	Streamer *streamer1 = new Streamer();
//	Streamer *streamer2 = new Streamer();
//	Streamer *streamer3 = new Streamer();
//	Streamer *streamer4 = new Streamer();
//	Streamer *streamer5 = new Streamer();
//	Streamer *streamer6 = new Streamer();
//	Streamer *streamer7 = new Streamer();
//	Streamer *streamer8 = new Streamer();
//	Streamer *streamer9 = new Streamer();
//	Streamer *streamer10 = new Streamer();
//	Streamer *streamer11 = new Streamer();
//	Streamer *streamer12 = new Streamer();
//	
//	streamer1->Start(12, "WNEM 5 - 1 CBS", "239.254.0.1", 7399);
//	streamer2->Start(13, "WNEM 5 - 2 My 5", "239.254.0.2", 7399);
//	streamer3->Start(14, "WNEM 5 - 3 Cozi TV", "239.254.0.3", 7399);
//	streamer4->Start(15, "WJRT 12 - 1 ABC", "239.254.0.4", 7399);
//	streamer5->Start(16, "WJRT 12 - 2 MeTV", "239.254.0.5", 7399);
//	streamer6->Start(17, "WDCQ 19 - 1 PBS", "239.254.0.6", 7399);
//	streamer7->Start(18, "WEYI 25 - 1 NBC", "239.254.0.7", 7399);
//	streamer8->Start(19, "WEYI 46 - 1 CW", "239.254.0.8", 7399);
//	streamer9->Start(20, "WEYI 25 - 3 Charge!", "239.254.0.9", 7399);
//	streamer10->Start(21, "WAQP 49 - 1 TCT", "239.254.0.10", 7399);
//	streamer11->Start(22, "WSMH 66 - 1 FOX", "239.254.0.11", 7399);
//	streamer12->Start(24, "WSMH 66 - 3 Comet TV", "239.254.0.12", 7399);
//
//	streamer1->Join();
//	streamer2->Join();
//	streamer3->Join();
//	streamer4->Join();
//	streamer5->Join();
//	streamer6->Join();
//	streamer7->Join();
//	streamer8->Join();
//	streamer9->Join();
//	streamer10->Join();
//	streamer11->Join();
//	streamer12->Join();
		
	return 0;
}