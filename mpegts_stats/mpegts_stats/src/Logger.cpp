// Logger.cpp
// Implementation of a multithread safe singleton logger class
#include <stdexcept>
#include <unistd.h>
#include "Logger.h"
#include "string.h"

using namespace std;

const string Logger::kLogLevelDebug = "DEBUG:";
const string Logger::kLogLevelInfo = "INFO: ";
const string Logger::kLogLevelError = "ERROR:";

#define LINESZ 1024

Logger* Logger::pInstance = nullptr;
mutex Logger::sMutex;

char hostname[LINESZ];

Logger& Logger::instance()
{
	static Cleanup cleanup;

	lock_guard<mutex> guard(sMutex);
	if (pInstance == nullptr)
		pInstance = new Logger();
	return *pInstance;
}

Logger::Cleanup::~Cleanup()
{
	lock_guard<mutex> guard(Logger::sMutex);
	delete Logger::pInstance;
	Logger::pInstance = nullptr;
}

Logger::~Logger()
{
	mOutputStream.close();
}

Logger::Logger()
{
	OpenStream();
}

void Logger::OpenStream()
{
	time_t now;

	filename[0] = '\0';
	now = time(NULL);

	gethostname(hostname, LINESZ);
	
	strftime(filename_timestamp, LINESZ, "%m%d%Y", localtime(&now));
	sprintf(filename, "/var/log/%s-mpegts_stats-%s.log", hostname, filename_timestamp);

	string logFileName(filename);
	
	mOutputStream.open(logFileName, ios_base::app);
	if (!mOutputStream.good()) {
		throw runtime_error("Unable to initialize the Logger!");
	} 
}

void Logger::log(const string& inMessage, const string& inLogLevel)
{
	lock_guard<mutex> guard(sMutex);
	logHelper(inMessage, inLogLevel);
}

void Logger::log(const vector<string>& inMessages, const string& inLogLevel)
{
	lock_guard<mutex> guard(sMutex);
	for (size_t i = 0; i < inMessages.size(); i++) {
		logHelper(inMessages[i], inLogLevel);
	}
}

void Logger::logHelper(const std::string& inMessage, const std::string& inLogLevel)
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
	
	strftime(cur_timestamp, 32, "%H:%M:%S", gmtime(&now));
	mOutputStream << inLogLevel << " " << cur_timestamp << " " << inMessage << endl;
	mOutputStream.flush();
}