#ifndef UTILS_H
#define UTILS_H

// #include "defs.h"

#include <sys/socket.h>  
#include <arpa/inet.h>
#include <netinet/in.h> 
#include <netinet/ip.h>
#include <netinet/igmp.h>
#include <net/if.h>
#include <net/route.h>
#include <sys/ioctl.h>
#include <linux/mroute.h>
#include <errno.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <ctype.h>

#define MAX_INET_BUF_LEN 19

typedef void(*ihfunc_t)(int, fd_set *);

#define NHANDLERS       3
typedef struct ihandler {
	int fd; /* File descriptor               */
	ihfunc_t func; /* Function to call with &fd_set */
} ihandler;

extern int nhandlers;
extern ihandler ihandlers[NHANDLERS];

int register_input_handler(int fd, ihfunc_t func);
char *packet_kind(int proto, int type, int code);
char *inet_fmt(uint32_t addr, char *s, size_t len);
int inet_valid_subnet(uint32_t nsubnet, uint32_t nmask);
int inet_valid_host(uint32_t naddr);
uint32_t inet_parse(char *s, int n);
size_t strlcpy(char *dst, const char *src, size_t size);

#endif