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

vector<string> split(string s, string delimiter);
bool replace(std::string& str, const std::string& from, const std::string& to);

class ChannelUtils
{
public:
	ChannelUtils();
	~ChannelUtils();

	string getChannelInfo();
	string getChannelNumber();
	string getChannelName();
	string getChannelByNumber(string channelNum);
	string getChannelByUrl(string url);
	string getNextChannel();
	string getPrevChannel();
	string getRandomChannel();
	string getFirstChannel();
	
	void setUrl(string url, bool saveSettings);

	void saveSettings();
	void getSettings();
	int loadChannelList();
	void updateAddressPort();
	
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

