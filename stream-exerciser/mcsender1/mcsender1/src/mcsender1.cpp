/*
 *   C++ sockets on Unix and Windows
 *   Copyright (C) 2002
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "PracticalSocket.h"  // For UDPSocket and SocketException

#include <string>
#include <iostream>           // For cout and cerr
#include <sstream>
#include <cstdlib>            // For atoi()
#include <pthread.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "thread.h"

#ifdef WIN32
#include <windows.h>          // For ::Sleep()
void sleep(unsigned int seconds) {::Sleep(seconds * 1000); }
#else
#include <unistd.h>           // For sleep()
#endif

using namespace std;

#define DATA_LENGTH 188

typedef struct mcparms_t {
	string address;
	int port;
	unsigned char *data;
	int ttl;
	pthread_t threadId;
	cCondWait *cond;
	UDPSocket *sock;
	sockaddr_in destAddr;
} mcparms_t;

void multicast_send(void *parms)
{
	mcparms_t *mcparms = (mcparms_t *) parms; 
	
	mcparms->sock = new UDPSocket();
	mcparms->cond = new cCondWait();
	
	fillAddr(mcparms->address, mcparms->port, mcparms->destAddr);
	
	try {

		mcparms->sock->setMulticastTTL(mcparms->ttl);

		while (1){
			mcparms->sock->sendTo(mcparms->data, DATA_LENGTH, mcparms->destAddr);
			mcparms->cond->SleepMs(100);
		}
	}
	catch (SocketException &e) {
		cerr << e.what() << endl;
		exit(1);
	}

}

int num_streams;
string base_url = string("239.0.1.");
// string base_url = string("239.254.0.14");

int main(int argc, char *argv[]) {

	if ((argc != 2) && (argc != 3))
	{
		printf("usage: %s num_streams\n", argv[0]);
		printf("   or %s num_streams base_url\n", argv[0]);
		printf("example: %s 10 239.0.2.\n", argv[0]);
		exit(0);
	}
	
	num_streams = atoi(argv[1]);
	
	if (argc == 3)
		base_url = string(argv[2]);
	
	cout << "Base URL: " << base_url << " Streams : " << num_streams << endl;
	
	mcparms_t *mcparms[num_streams];
		
	for (int i = 0; i < num_streams; i++)
	{
		mcparms[i] = (mcparms_t *) malloc(sizeof(mcparms_t));	
		new(mcparms[i]) mcparms_t;
	
		std::ostringstream out;
		out << base_url << i + 1;
		
//		mcparms[i]->address = base_url;
		mcparms[i]->address = out.str();
		mcparms[i]->port = 5001;
		mcparms[i]->data = (unsigned char *) malloc(DATA_LENGTH);
		mcparms[i]->ttl = 32;
	
		printf("Starting stream: %s\n", mcparms[i]->address.c_str());
		
		pthread_create(&mcparms[i]->threadId, NULL, (void *(*)(void *))&multicast_send, mcparms[i]);
		
		cCondWait::SleepMs(1000);
	}

	for (int i = 0; i < num_streams; i++)
		pthread_join(mcparms[i]->threadId, NULL);

	return 0;
}