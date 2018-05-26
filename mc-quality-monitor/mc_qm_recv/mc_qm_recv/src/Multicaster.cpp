#include "Multicaster.h"

Multicaster::Multicaster()
{
}

Multicaster::~Multicaster()
{
}

int Multicaster::Open(const char *mc_address_str, int mc_port, int mc_ttl, bool isWrite)
{
	if ((mc_port < MIN_PORT) || (mc_port > MAX_PORT)) {
		fprintf(stderr, "Invalid port number argument %d.\n", mc_port);
		fprintf(stderr, "Valid range is between %d and %d.\n", MIN_PORT, MAX_PORT);
		exit(1);
	}
  
	if (isWrite)
	{
		if ((mc_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0) {
			perror("Multicaster::Open-1 socket() failed");
			try {
				close(mc_sock);
			}
			catch (...)
			{
			}
			return -1;
		}

		if ((setsockopt(mc_sock, IPPROTO_IP, IP_MULTICAST_TTL, (void*) &mc_ttl, sizeof(mc_ttl))) < 0) {
			perror("Multicaster::Open-2 setsockopt() failed");
			try {
				close(mc_sock);
			}
			catch (...)
			{
			}
			return -1;
		} 
		
		memset(&mc_address, 0, sizeof(mc_address));
		
		mc_address.sin_family      = AF_INET;
		mc_address.sin_addr.s_addr = inet_addr(mc_address_str);
		mc_address.sin_port        = htons(mc_port);
	}
	else
	{
		if ((mc_sock = socket(PF_INET, SOCK_DGRAM | SOCK_NONBLOCK, IPPROTO_UDP)) < 0) {
			perror("Multicaster::Open-3 socket() failed");
			try {
				close(mc_sock);
			}
			catch (...)
			{
			}
			return -1;
		}

		int flag_on = 1;
		if ((setsockopt(mc_sock, SOL_SOCKET, SO_REUSEADDR, &flag_on, sizeof(flag_on))) < 0) {
			perror("Multicaster::Open-4 setsockopt() failed");
			try {
				close(mc_sock);
			}
			catch (...)
			{
			}
			return -1;
		}

		memset(&mc_address, 0, sizeof(mc_address));
		mc_address.sin_family      = AF_INET;
		mc_address.sin_addr.s_addr = htonl(INADDR_ANY);
		mc_address.sin_port        = htons(mc_port);

		if ((bind(mc_sock, (struct sockaddr *) &mc_address, sizeof(mc_address))) < 0) {
			perror("Multicaster::Open-5 bind() failed");
			try {
				close(mc_sock);
			}
			catch (...)
			{
			}
			return -1;
		}

		struct ip_mreq mc_req; 
		mc_req.imr_multiaddr.s_addr = inet_addr(mc_address_str);
		mc_req.imr_interface.s_addr = htonl(INADDR_ANY);

		if ((setsockopt(mc_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*) &mc_req, sizeof(mc_req))) < 0) {
			perror("Multicaster::Open-6 setsockopt() failed");
			try {
				close(mc_sock);
			}
			catch (...)
			{
			}
			return -1;
		}	
	}
	
	return mc_sock;
}
	
int Multicaster::Send(unsigned char *buf, int len)
{
	int number = 0;

	int send_len = sendto(mc_sock, buf, len, 0, (struct sockaddr *) &mc_address, sizeof(mc_address));
	if (send_len < 0)
	{
		perror("Multicaster::Send-7 sendto() sent incorrect number of bytes");
		return -1;
	}
	
	return send_len;
}

int Multicaster::Receive(unsigned char *buf, int len, int data_timeout_ms)
{	
	fd_set readset;
	struct timeval tv;
	int recv_len = 0;
	
	FD_ZERO(&readset);
	FD_SET(mc_sock, &readset);

	tv.tv_sec = 0;
	tv.tv_usec = data_timeout_ms * 1000;

	int result = select(mc_sock + 1, &readset, NULL, NULL, &tv);
	if (result == -1)
	{
		perror("Multicaster::Receive-8 select() failed");
		return -1;
	}
	
	if (result)
	{
		if (FD_ISSET(mc_sock, &readset)) {
			recv_len = recv(mc_sock, (void *) buf, len, 0);
			if (recv_len < 0) {
				perror("Multicaster::Receive-9 recvfrom() failed");
				return -1;
			}
		}
	}
	
	return recv_len;
}
