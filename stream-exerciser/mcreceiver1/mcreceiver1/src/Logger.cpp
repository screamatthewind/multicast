// Logger.cpp
// Implementation of a multithread safe singleton logger class
#include <stdexcept>
#include "Logger.h"

using namespace std;

const string Logger::kLogLevelDebug = "DEBUG:";
const string Logger::kLogLevelInfo = "INFO: ";
const string Logger::kLogLevelError = "ERROR:";

Logger* Logger::pInstance = nullptr;

mutex Logger::sMutex;

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
	time_t now;
	char filename[32];

	filename[0] = '\0';
	now = time(NULL);

	if (now != -1)
		strftime(filename, 32, "../mcreceiver1_%m%d%Y.log", localtime(&now));

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
//	time_t now;
//	char timestamp[32];
//
//	timestamp[0] = '\0';
//	now = time(NULL);
//
//	strftime(timestamp, 32, "%H:%M:%S", gmtime(&now));
//	mOutputStream << inLogLevel << " " << timestamp << " " << inMessage << endl;

	mOutputStream << inMessage << endl;
}