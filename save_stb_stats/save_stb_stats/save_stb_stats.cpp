#include <stdio.h> /* printf, sprintf */
#include <stdlib.h> /* exit */
#include <string.h>
#include <unistd.h> /* read, write, close */
#include <string.h> /* memcpy, memset */
#include <sys/socket.h> /* socket, connect */
#include <netinet/in.h> /* struct sockaddr_in, struct sockaddr */
#include <netdb.h> /* struct hostent, gethostbyname */
#include <arpa/inet.h>

#define CSV_IO_NO_THREAD

#include "csv.h"

char tmpMessage[65535];
char jsonMessage[65535];
char message[65535];

void replace_char(char *str, char old_ch, char new_ch)
{
	char *pch = str;
	int pos;
	
	do
	{
		pch = strchr(pch, old_ch);
		if (pch != NULL)
		{
			pos = pch - str;
			str[pos] = new_ch;
			pch++;
		}
		
	} while (pch != NULL);	
}

bool PostStats(char *portal_ip, int portal_port)
{
	struct sockaddr_in serv_addr;
	int sockfd, bytes, sent, received, total;
	char response[4096];

	fprintf(stderr, ".");
	fflush(stderr);
	
//	printf("Request: %s\n", message);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
	{
		perror("\nERROR opening socket");
		return false;
	}

	in_addr_t dst_ip;
	if (inet_pton(AF_INET, portal_ip, (void *) &dst_ip) == NULL) {
		perror("\ninet_ntop");
		return false;
	}
	
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portal_port);
	serv_addr.sin_addr.s_addr = dst_ip;
	
	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		perror("\nERROR connecting");
		return false;
	}

	total = strlen(message);
	sent = 0;

	do {
		bytes = write(sockfd, message + sent, total - sent);
		if (bytes < 0)
		{
			perror("\nERROR writing message to socket");
			return false;
		}
		
		if (bytes == 0)
			break;
		
		sent += bytes;
	} while (sent < total);

	bytes = read(sockfd, response, 4096);
	if (bytes < 0)
	{
		perror("\nERROR reading response from socket");
		return false;
	}

	close(sockfd);

	response[bytes] = 0;
	//	printf("Response:\n%s\n", response);
	
	if(strstr(response, "200 OK") == NULL)
		return false;
	
	return true;
}

bool SendDecoderStats(char *filename, char *portal_ip, int portal_port, char *mac_address)
{
	std::string GmtDateTime, ChannelName, URL;
	int ChannelNum;
	long StartMs, TestSecs, AudioPkts, VideoPkts, OtherPkts, AudioErrs, VideoErrs, OtherErrs, ProgramErrs;
	std::string pctAudioErr, pctVideoErr, pctOtherErr;
	long AudioLoss, VideoLoss, OtherLoss, CbSize;
	double fPctAudioErr, fPctVideoErr, fPctOtherErr;
	std::string::size_type sz; 
	
	char *jsonHeader = (char *) "{'DecoderStats':[";
	char *jsonFooter = (char *) "]}";
	char *jsonTemplate = (char *) "{'mac':'%s', 'gmtDateTime':'%s', 'channelNum':'%d', 'channelName':'%s', 'channelUrl':'%s', 'startMs':'%d', 'testSecs':'%d', 'audioPkts':'%d', 'videoPkts':'%d', 'otherPkts':'%d', 'audioErrs':'%d', 'videoErrs':'%d', 'otherErrs':'%d', 'programErrs':'%d', 'pctAudioErr':'%.2f', 'pctVideoErr':'%.2f', 'pctOtherErr':'%.2f', 'audioLoss':'%d', 'videoLoss':'%d', 'otherLoss':'%d', 'cbSize':%d}";

	io::CSVReader<20> in(filename);
	in.read_header(io::ignore_no_column, "GmtDateTime", "ChannelNum", "ChannelName", "URL", "StartMs", "TestSecs", "AudioPkts", "VideoPkts", "OtherPkts", "AudioErrs", "VideoErrs", "OtherErrs", "ProgramErrs", "AudioPctErr", "VideoPctErr", "OtherPctErr", "AudioLoss", "VideoLoss", "OtherLoss", "CBSize");

	while (in.read_row(GmtDateTime, ChannelNum, ChannelName, URL, StartMs, TestSecs, AudioPkts, VideoPkts, OtherPkts, AudioErrs, VideoErrs, OtherErrs, ProgramErrs, pctAudioErr, pctVideoErr, pctOtherErr, AudioLoss, VideoLoss, OtherLoss, CbSize)) 
	{
		fPctAudioErr = std::stod(pctAudioErr, &sz);
		fPctVideoErr = std::stod(pctVideoErr, &sz);
		fPctOtherErr = std::stod(pctOtherErr, &sz);
	
		//	sprintf(tmpMessage, jsonTemplate, "2018-02-23 08:01:38", 226, "Destination America", "udp://239.0.1.116:7399", 62, 240, 26016, 163899, 2586, 0, 0, 0, 0, 0.0, 0.0, 0.0, 0, 0, 0, 3948);
		sprintf(tmpMessage, jsonTemplate, mac_address, GmtDateTime.c_str(), ChannelNum, ChannelName.c_str(), URL.c_str(), StartMs, TestSecs, AudioPkts, VideoPkts, OtherPkts, AudioErrs, VideoErrs, OtherErrs, ProgramErrs, fPctAudioErr, fPctVideoErr, fPctOtherErr, AudioLoss, VideoLoss, OtherLoss, CbSize);
		sprintf(jsonMessage, "%s%s%s", jsonHeader, tmpMessage, jsonFooter);
		replace_char(jsonMessage, '\'', '\"');
		
		sprintf(message, "POST /decoderStats/ HTTP/1.1\r\nContent-Length: %d\r\nHost: %s:%d\r\nContent-Type: application/json\r\n\r\n%s", strlen(jsonMessage), portal_ip, portal_port, jsonMessage); 

		if (!PostStats(portal_ip, portal_port))
			return false;
	}
	
	return true;
}

void DeleteFile(char *filename)
{
	char tmpFilename[255];
	char *p = strstr(filename, ".csv");
	if (p != NULL)
	{
		int pos = p - filename;
		filename[pos] = 0;
	}
	
	sprintf(tmpFilename, "%s.csv", filename);
	remove(tmpFilename);

	sprintf(tmpFilename, "%s.log", filename);
	remove(tmpFilename);
}

int main(int argc, char *argv[])
{
	// char src_ip[INET_ADDRSTRLEN] = "173.225.90.74";
	char filename[255];
	char mac_address[32];
	char portal_ip[INET_ADDRSTRLEN];
	int portal_port;

	strcpy(filename, "../mctest3-02262018.csv");
	
	strcpy(mac_address, "b8:27:eb:2f:ec:56");

	strcpy(portal_ip, "192.168.44.143");
	portal_port = 8080;
	
	try {
		if (SendDecoderStats((char *) filename, portal_ip, portal_port, mac_address))
			DeleteFile((char *) filename);
	} catch (const std::exception& ex) {
		fprintf(stderr, "\nERROR during SendDecoderStats %s\n", ex.what());
	} catch (const std::string& ex) {
		fprintf(stderr, "\nERROR during SendDecoderStats %s\n", ex.c_str());
	} catch (...) {
		fprintf(stderr, "\nERROR during SendDecoderStats\n");
	}	

	fprintf(stderr, "\nDone\n");
	
	return 0;
}