#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h> 
#include <ctype.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h> 
#include <string.h> 

using namespace std;

#define LINESZ 1024
#define NUM_LENGTHS 29

int main(int argc, char *argv[])
{
	FILE *fin = fopen(argv[1], "r");
	FILE *fout = fopen("parsed.log", "w");

	char *line = NULL;
	ssize_t bytes_read;
	size_t len = LINESZ;

	int lengths[NUM_LENGTHS] = { 27, 27, 60, 17, 12, 7, 10, 7, 10, 7, 10, 7, 10, 7, 10, 7, 10, 7, 10, 7, 10, 7, 10, 7, 10, 7, 10, 7, 10};
	
	while ((bytes_read = getline(&line, &len, fin)) != -1) 
	{
		char* newline = strchr(line, '\n');         // find the newline character
		if(newline)
			*newline = '\0';

		if (strstr(line, "Started") != NULL)
			continue;
		
		if (strstr(line, "Exited") != NULL)
			continue;

		if (strstr(line, "Status") != NULL)
			continue;

		if (strstr(line, "17 - WDCQ 19-1 PBS (udp://239.254.0.6:7399)") != NULL)
			printf("timed out");

		int length = 0;
		int length_read = 0;
		
		char *p = line;
		
		for (int i = 0; i < NUM_LENGTHS; i++)
		{
			length = lengths[i];
			length_read += length;
			
			if (length_read > bytes_read)
				break;
			
			p[length-1] = 0;
			
			char *p2 = p;
			while ((*p2 != 0) && (*p2 == ' '))
				p2++;
			
			printf("%s\n", p2);
			fprintf(fout, "%s", p2);
			
			fprintf(fout, ",");
			
			p += length;
		}
		
		fprintf(fout, "\n");
	}
	
	fclose(fin);
	fflush(fout);
	fclose(fout);
	
	return 0;
}