#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "tools.h"

cTimeMs *m_deltaTimer = NULL;
cTimeMs *m_dataTimer = NULL;

char timestamp[255];
bool got_start = false;

int main()
{
    char *url, *line = NULL;
	size_t size;
	ssize_t bytes_read;
	
	m_dataTimer = new cTimeMs();
	m_deltaTimer = new cTimeMs();
	
	m_deltaTimer->Set();

	while ((bytes_read = getline(&line, &size, stdin)) != -1) {
	    
		GetTimestamp((char *) &timestamp);

		if (strstr(line, "switch_channel 1") != 0)
			fprintf(stderr, "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
		
		fprintf(stderr, "%s %6" PRIu64 " ms %6" PRIu64 " ms %s", timestamp, m_deltaTimer->Elapsed(), m_dataTimer->Elapsed(), line);
		
		if (strstr(line, "cmd udp") != 0)
		{
			url = strstr(line, "udp");
			url--;
			*url = 0;
			url++;
		    
			char* newline = strchr(url, '\n');
			if (newline)
				*newline = '\0';

			if (!got_start)
			{
				got_start = true;
				m_dataTimer->Set();
			}
		    
			fprintf(stderr, "%s %6" PRIu64 " ms %-16s %s %6" PRIu64 " ms\n", timestamp, m_deltaTimer->Elapsed(), "<<RDL>> start 1", url, m_dataTimer->Elapsed());
		}	    
		
		else if (strstr(line, "push udp://") != 0)
		{
			url = strstr(line, "udp");
			url--;
			*url = 0;
			url++;
		    
			char* newline = strchr(url, '\n');
			if (newline)
				*newline = '\0';

			if (!got_start)
			{
				got_start = true;
				m_dataTimer->Set();
			}
		    
			fprintf(stderr, "%s %6" PRIu64 " ms %-16s %s %6" PRIu64 " ms\n", timestamp, m_deltaTimer->Elapsed(), "<<RDL>> start 2", url, m_dataTimer->Elapsed());
		}
		
		else if (strstr(line, "player create before") != 0)
		{
			got_start = false;
			fprintf(stderr, "%s %6" PRIu64 " ms %-16s %6" PRIu64 " ms\n", timestamp, m_deltaTimer->Elapsed(), "<<RDL>> join 1", m_dataTimer->Elapsed());
		}

//		else if (strstr(line, "player play before") != 0)
//		{
//			got_start = false;
//			fprintf(stderr, "%s %6" PRIu64 " ms %-16s %6" PRIu64 " ms\n", timestamp, m_deltaTimer->Elapsed(), "<<RDL>> join 2", m_dataTimer->Elapsed());
//		}
		
		else if (strstr(line, "ffurl_open before") != 0)
		{
			got_start = false;
			fprintf(stderr, "%s %6" PRIu64 " ms %-16s %6" PRIu64 " ms\n", timestamp, m_deltaTimer->Elapsed(), "<<RDL>> data 1", m_dataTimer->Elapsed());
		}
		
		else if(strstr(line, "ffurl_open after") != 0)
		{
			got_start = false;
			fprintf(stderr, "%s %6" PRIu64 " ms %-16s %6" PRIu64 " ms\n", timestamp, m_deltaTimer->Elapsed(), "<<RDL>> data 2", m_dataTimer->Elapsed());		
		}
		
		else if(strstr(line, "New PMT PID") != 0)
		{
			got_start = false;
			fprintf(stderr, "%s %6" PRIu64 " ms %-16s %6" PRIu64 " ms\n", timestamp, m_deltaTimer->Elapsed(), "<<RDL>> data synced", m_dataTimer->Elapsed());
		}
		
		else if(strstr(line, "AUDIO_EVENT_FRAME_DECODED") != 0)
		{
			got_start = false;
			fprintf(stderr, "%s %6" PRIu64 " ms %-16s %6" PRIu64 " ms\n", timestamp, m_deltaTimer->Elapsed(), "<<RDL>> audio primed", m_dataTimer->Elapsed());
		}
		
		else if(strstr(line, "AUDIO_EVENT_STREAM_IN_SYNC") != 0)
		{
			got_start = false;
			fprintf(stderr, "%s %6" PRIu64 " ms %-16s %6" PRIu64 " ms\n", timestamp, m_deltaTimer->Elapsed(), "<<RDL>> audio synced", m_dataTimer->Elapsed());
		}
		
		else if(strstr(line, "VIDEO_EVENT_FRAME_DECODED") != 0)
		{
			got_start = false;
			fprintf(stderr, "%s %6" PRIu64 " ms %-16s %6" PRIu64 " ms\n", timestamp, m_deltaTimer->Elapsed(), "<<RDL>> video primed", m_dataTimer->Elapsed());
		}
		
		else if(strstr(line, "VIDEO_EVENT_STREAM_IN_SYNC") != 0)
		{
			got_start = false;
			fprintf(stderr, "%s %6" PRIu64 " ms %-16s %6" PRIu64 " ms\n", timestamp, m_deltaTimer->Elapsed(), "<<RDL>> video synced", m_dataTimer->Elapsed());
		}
		
		else if(strstr(line, "VIDEO_EVENT_FIRST_FRAME_ON_DISPLAY") != 0)
		{
			got_start = false;
			fprintf(stderr, "%s %6" PRIu64 " ms %-16s %6" PRIu64 " ms\n", timestamp, m_deltaTimer->Elapsed(), "<<RDL>> first video frame", m_dataTimer->Elapsed());
		}
		
		else if(strstr(line, "VIDEO_EVENT_FRAME_SUPPLIED") != 0)
		{
			got_start = false;
			fprintf(stderr, "%s %6" PRIu64 " ms  %-16s %6" PRIu64 " ms\n", timestamp, m_deltaTimer->Elapsed(), "<<RDL>> playing video", m_dataTimer->Elapsed());
		}
		
		if (m_deltaTimer->Elapsed() > 500)
			fprintf(stderr, "%s -------------------------------------------------------------------------------------------------------------\n", timestamp);

		fflush(stderr);

		m_deltaTimer->Set();
    }

	return 0;
}