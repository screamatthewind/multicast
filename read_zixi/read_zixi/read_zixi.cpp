// https://stackoverflow.com/questions/22077802/simple-c-example-of-doing-an-http-post-and-consuming-the-response
	
#include <stdio.h> 
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <netdb.h> 
#include <arpa/inet.h>
#include <sys/time.h>
#include <unistd.h>

void error(const char *msg) { perror(msg); exit(0); }

int main(int argc, char *argv[])
{
	struct timeval start, end;
    long mtime, seconds, useconds;  
	gettimeofday(&start, NULL);
	
	int portno = 4800;

	struct sockaddr_in serv_addr;
	int sockfd, bytes;
	char buffer[4096];

	sprintf(buffer, "GET /?url=zixi://10.153.255.15:2077/AMC-HD HTTP/1.1\r\nUser-Agent: Lavf/56.36.100\r\nAccept: */*\r\nRange: bytes=0-\r\nConnection: close\r\nHost: 127.0.0.1:4800\r\nIcy-MetaData: 1\r\n\r\n");
	printf("Request: %s\n", buffer);

	/* create the socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) 
		perror("ERROR opening socket");

	char src_ip[INET_ADDRSTRLEN] = "127.0.0.1";
	in_addr_t dst_ip;
	
	if (inet_pton(AF_INET, src_ip, (void *) &dst_ip) == NULL){
		perror("inet_ntop");
		exit(EXIT_FAILURE);	
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = dst_ip;
	
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		perror("ERROR connecting");

	printf("write\n");
	
	bytes = write(sockfd, buffer, strlen(buffer));
	if (bytes < 0)
		perror("ERROR writing message to socket");
		
	/* receive the response */
	memset(buffer, 0, sizeof(buffer));

	printf("read\n");
	
	int ctr = 0;
	
	do {
		bytes = read(sockfd, buffer, sizeof(buffer));
		if (bytes < 0)
			perror("ERROR reading response from socket");

		buffer[bytes] = 0;
		
		gettimeofday(&end, NULL);
		seconds  = end.tv_sec  - start.tv_sec;
		useconds = end.tv_usec - start.tv_usec;
		mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;		

		printf("%8ld ms bytes %d\n", mtime, bytes);
		printf("Response:\n%s\n", buffer);
		
		memcpy((void *) &start, (void *) &end, sizeof(struct timeval));
		
		if ((ctr++ % 1000) == 0)
			usleep(60000000);
		
		// if (bytes == 0)
		//	break;
		
	} while (1);

	/* close the socket */
	close(sockfd);

	/* process response */

	return 0;
}