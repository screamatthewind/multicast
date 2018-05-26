// CsvWriter.cpp
// Implementation of a multithread safe singleton logger class
#include "CsvWriter.h"
#include "unistd.h"
#include "string.h"

using namespace std;

CsvWriter* CsvWriter::pInstance = nullptr;
mutex CsvWriter::sMutex;

CsvWriter& CsvWriter::instance(string& csvHeader)
{
	static Cleanup cleanup;

	lock_guard<mutex> guard(sMutex);
	if (pInstance == nullptr)
		pInstance = new CsvWriter(csvHeader);
	
	return *pInstance;
}

CsvWriter::Cleanup::~Cleanup()
{
	lock_guard<mutex> guard(CsvWriter::sMutex);
	delete CsvWriter::pInstance;
	CsvWriter::pInstance = nullptr;
}

CsvWriter::~CsvWriter()
{
	mOutputStream.close();
}

CsvWriter::CsvWriter(string& csvHeader) : m_csvHeader(csvHeader)
{
	OpenStream();
}

void CsvWriter::OpenStream()
{
	time_t now;

	char hostname[255];
	
	filename[0] = '\0';
	now = time(NULL);

	gethostname(hostname, 255);
	
	strftime(filename_timestamp, 255, "%m%d%Y", localtime(&now));
	sprintf(filename, "/var/log/%s-mctest3-%s.csv", hostname, filename_timestamp);

	string logFilename = string(filename);

	ifstream f(logFilename);
	
	mOutputStream.open(logFilename, ios_base::app);
	if (!mOutputStream.good()) {
		throw runtime_error("Unable to initialize the CsvWriter!");
	} 

	if (!f.good())
		writeHelper(m_csvHeader, false);
}

void CsvWriter::write(const string& inMessage)
{
	lock_guard<mutex> guard(sMutex);
	writeHelper(inMessage, true);
}

void CsvWriter::write(const vector<string>& inMessages)
{
	lock_guard<mutex> guard(sMutex);
	for (size_t i = 0; i < inMessages.size(); i++) {
		writeHelper(inMessages[i], true);
	}
}

void CsvWriter::writeHelper(const std::string& inMessage, bool writeTimestamp)
{
	if (access(filename, F_OK) == -1)
	{
		mOutputStream.close();
		OpenStream();
	}		

	time_t now;
	char cur_timestamp[32];

	cur_timestamp[0] = '\0';
	now = time(NULL);

	// reopen file with new name when date changes
	strftime(cur_timestamp, 255, "%m%d%Y", localtime(&now));
	if (strcmp(filename_timestamp, cur_timestamp) != 0)
	{
		mOutputStream.close();
		OpenStream();
	}
		
	if (writeTimestamp)
	{
		strftime(cur_timestamp, 32, "%F %H:%M:%S", gmtime(&now));
		mOutputStream << cur_timestamp << ", " << inMessage << endl;
	}
	else
		mOutputStream << inMessage << endl;
}