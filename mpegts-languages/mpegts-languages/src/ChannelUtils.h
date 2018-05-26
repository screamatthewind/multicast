#ifndef __CHANNEL_UTILS_INCLUDED__
#define __CHANNEL_UTILS_INCLUDED__

#include <stdio.h>
#include <string.h>

#include <string>
#include <utility>
#include <iostream>
#include <istream>
#include <vector>
#include <iterator>
#include <fstream>

#include "tools.h"

using namespace std;

class ChannelUtils
{
public:
	ChannelUtils(string channelListFilename);
	~ChannelUtils();

	string getChannelInfo();
	string getChannelNumber();
	string getChannelName();
	string getChannelByNumber(string channelNum);
	string getChannelByUrl(string url);
	string getNextChannel();
	string getPrevChannel();
	string getRandomChannel();
	
	void setUrl(string url, bool saveSettings);

	void saveSettings();
	void getSettings();
	int loadChannelList();
	void updateAddressPort();

	string m_channelListFilename;
	string m_iniFilename;
	
	typedef struct ChannelStruct {
		int         channelNum;
		int         xmltvNum;
		string      channelName;
		string      url;
	} ChannelStruct;

	vector<ChannelStruct> channels;
	int                   numChannels;
	std::string           userChannel;
	string                m_url;
	int                   m_volume;
	string                m_address;
	int                   m_port;
	
	cLockFile*            m_lockFile;
};

#endif

