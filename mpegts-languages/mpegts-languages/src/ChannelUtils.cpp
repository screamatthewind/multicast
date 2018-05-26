#include "ChannelUtils.h"

ChannelUtils::ChannelUtils(string channelListFilename)
{
	m_lockFile = new cLockFile(".");

	numChannels = 0;
	
	if (!channelListFilename.empty())
	{
		m_channelListFilename = channelListFilename;
		
		size_t pos = m_channelListFilename.find_last_of("/");
		string path;
		
		if (pos <= m_channelListFilename.length())
			path = m_channelListFilename.substr(0, pos + 1);
		
		m_iniFilename = path + "mpegts-languages.ini";
	}
	else
	{
		m_channelListFilename = "/etc/channel_list.csv";
		m_iniFilename = "/etc/mpegts-languages.ini";
	}

	loadChannelList();
	getSettings();
}

ChannelUtils::~ChannelUtils()
{
}

vector<string> split(string s, string delimiter) {
	vector<string> list;
	size_t pos = 0;
	string token;
	while ((pos = s.find(delimiter)) != string::npos) {
		token = s.substr(0, pos);
		list.push_back(token);
		s.erase(0, pos + delimiter.length());
	}
	list.push_back(s);

	return list;
}

bool replace(std::string& str, const std::string& from, const std::string& to) {
	size_t start_pos = str.find(from);
	if (start_pos == std::string::npos)
		return false;
	str.replace(start_pos, from.length(), to);
	return true;
}

string ChannelUtils::getChannelInfo() {
    
	string url = m_url;
	
	vector<ChannelStruct>::iterator it;
	string result = "Unknown (" + m_url + ")"; 
	
	for (it = channels.begin(); it < channels.end(); it++) {
		if (it->url.compare(url) == 0) {
			result = string(itoa(it->channelNum)) + " - " + it->channelName + " (" + it->url + ")";
			break;
		}
	}

	return result;
}

string ChannelUtils::getChannelName() {
    
	string url = m_url;

	vector<ChannelStruct>::iterator it;
	string result = "";
	
	for (it = channels.begin(); it < channels.end(); it++) {
		if (it->url.compare(url) == 0) {
			result = it->channelName;
			break;
		}
	}

	return result;
}

string ChannelUtils::getChannelNumber() {

	string url = m_url;
    
	vector<ChannelStruct>::iterator it;
	string result = "";
	
	for (it = channels.begin(); it < channels.end(); it++) {
		if (it->url.compare(url) == 0) {
			result = string(itoa(it->channelNum));
			break;
		}
	}

	return result;
}

void ChannelUtils::updateAddressPort()
{
	string url = m_url;

	if (m_url.size() == 0)
		return;
	
	replace(url, "udp://", "");
	vector<string> components = split(url, ":"); 

	m_address = components[0];
	m_port    = atoi(components[1].c_str());	
}

void ChannelUtils::setUrl(string url, bool bSaveSettings) {
    
	m_url  = url;
	
	updateAddressPort();
	
	if (bSaveSettings)
		saveSettings();
}

string ChannelUtils::getChannelByUrl(string url) {
    
	vector<ChannelStruct>::iterator it;
	
	for (it = channels.begin(); it < channels.end(); it++) {
		if (it->url == url)
		{
			m_url = it->url;
			break;				
		}
	}

	updateAddressPort();
	saveSettings();

	return m_url;
}

string ChannelUtils::getChannelByNumber(string channelNum) {
    
	vector<ChannelStruct>::iterator it;
	
	for (it = channels.begin(); it < channels.end(); it++) {
		if (it->channelNum == atoi(channelNum.c_str())) 
		{
			m_url = it->url;
			break;				
		}
	}

	updateAddressPort();
	saveSettings();

	return m_url;
}

string ChannelUtils::getNextChannel() {
    
	vector<ChannelStruct>::iterator it;
	
	for (it = channels.begin(); it < channels.end(); it++) {
		if (it->url.compare(m_url) == 0) {
			
			if (++it < channels.end()) 
			{
				m_url = it->url;
				break;				
			}
			else
				m_url = channels[0].url;

			break;
		}
	}

	updateAddressPort();
	saveSettings();

	return m_url;
}

string ChannelUtils::getPrevChannel() {
    
	vector<ChannelStruct>::reverse_iterator it;
	
	for (it = channels.rbegin(); it < channels.rend(); it++) {
		if (it->url.compare(m_url) == 0) {
			
			if (++it < channels.rend()) 
			{
				m_url = it->url;
				break;				
			}
			else
				m_url = channels[numChannels - 1].url;
		}
	}

	updateAddressPort();
	saveSettings();

	return m_url;
}

string ChannelUtils::getRandomChannel()
{
	int channelNum;
		
	if (numChannels == 0)
		return "";
	
	channelNum = rand() % numChannels;
	m_url = channels[channelNum].url;

	updateAddressPort();
	saveSettings();	

	return m_url;
}

void ChannelUtils::saveSettings() {
	ofstream outfile;

	if (!m_lockFile->Lock(1))
	{
		m_lockFile->RemoveLock();
		if (!m_lockFile->Lock(1))
		{
			printf("Error: File locked - unable to open channel ini file\n");
			return;
		}
	}
	
	outfile.open(m_iniFilename);

	if (outfile.is_open())
	{
		outfile << m_url << endl;
		outfile << m_volume << endl;

		outfile.close();
	}
	
	m_lockFile->Unlock();
}

void ChannelUtils::getSettings() {
	ifstream infile;

	if (!m_lockFile->Lock(1))
	{
		m_lockFile->RemoveLock();
		if (!m_lockFile->Lock(1))
		{
			printf("Error: File locked - unable to open channel ini file\n");
			return;
		}
	}
	
	infile.open(m_iniFilename);
	if (infile.is_open())
	{
		infile >> m_url;
		infile >> m_volume;
		
		updateAddressPort();

		infile.close();
		m_lockFile->Unlock();

		return;
	}

	if (channels.size() > 0)
		m_url = channels[0].url;
	
	updateAddressPort();
}

int ChannelUtils::loadChannelList() {

	FILE *fp;
	int  channelNum;
	int  xmltvNum;
	char channelName[64];
	char url[128];

	if (!m_lockFile->Lock(10))
	{
		m_lockFile->RemoveLock();
		if (!m_lockFile->Lock(10))
		{
			printf("Error: File locked - unable to open channel list file\n");
			return 0;
		}
	}
	
	if ((fp = fopen(m_channelListFilename.c_str(), "r")) == NULL) {
		cout << "Fatal Error: can't open channel list file " << m_channelListFilename << endl;
		exit(0);
	}

	char str[130];
	char *pos;

	// skip header
	if(fgets(str, 128, fp) == NULL) {
		fclose(fp);
		m_lockFile->Unlock();
		return 0;
	}

	numChannels = 0;
	
	while (true) {

		if (fgets(str, 128, fp) != NULL)
		{
			if ((pos = strchr(str, '\n')) != NULL)
				*pos = '\0';

			if ((pos = strchr(str, '\r')) != NULL)
				*pos = '\0';

			sscanf(str, "%d,%d,%[^,],%[^,]", &channelNum, &xmltvNum, channelName, url);

			// channels.push_back({channelNum, xmltvNum, channelName, url});
			// channels.emplace_back(channelNum, xmltvNum, channelName, url);
		   
			channels.push_back(ChannelStruct());
		   
			channels[numChannels].channelNum  = channelNum;
			channels[numChannels].xmltvNum    = xmltvNum;
			channels[numChannels].channelName = string(channelName);
			channels[numChannels].url         = string(url);
		}

		else
			break;

		numChannels++;
	}

	fclose(fp);
	m_lockFile->Unlock();

	return numChannels;
}
