#ifndef MULTICASTER_H
#define MULTICASTER_H

#include <sys/types.h>   /* for type definitions */
#include <sys/socket.h>  /* for socket API function calls */
#include <netinet/in.h>  /* for address structs */
#include <arpa/inet.h>   /* for sockaddr_in */
#include <stdio.h>       /* for printf() */
#include <stdlib.h>      /* for atoi() */
#include <string.h>      /* for strlen() */
#include <unistd.h>      /* for close() */

#define MIN_PORT 1024    /* minimum port allowed */
#define MAX_PORT 65535   /* maximum port allowed */

class Multicaster 
{
public:
	Multicaster();
	~Multicaster();
	
	int Open(const char *mc_address_str, const char *mc_if_address_str, int mc_port, int mc_ttl, bool isWrite);
	int Send(unsigned char *buf, int len);
	int Receive(unsigned char *buf, int len);

	int                mc_sock; 
	struct sockaddr_in mc_address;
};

#endif