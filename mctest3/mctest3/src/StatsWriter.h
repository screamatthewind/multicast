#ifndef STATSWRITER_H
#define STATSWRITER_H

#include "csv_reader.h"
#include "findfirst.h"

class StatsWriter 
{
public:
	StatsWriter(char *portal_ip, int portal_port);
	~StatsWriter();

	bool SaveToDatabase(char *cur_timestamp);
	
private:
	bool PostStats(char *message);
	bool SendDecoderStats(char *filename);
	void DeleteFile(char *filename);
	
	char *m_portal_ip;
	int m_portal_port;
};

#endif