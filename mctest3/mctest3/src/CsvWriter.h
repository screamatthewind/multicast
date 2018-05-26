#ifndef CSV_WRITER_H
#define CSV_WRITER_H

// CsvWriter.h
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <mutex>
#include <string>
#include <stdexcept>


// Definition of a multithread safe singleton logger class
class CsvWriter
{
public:

	char filename[255];
	char filename_timestamp[255];

	// Returns a reference to the singleton CsvWriter object
	static CsvWriter& instance(std::string& csvHeader);

	// Writes a single message
	void write(const std::string& inMessage);

	// Writes a vector of messages
	void write(const std::vector<std::string>& inMessages);

	bool fileExists();
	char *getFileTimestamp();
		
protected:
	// Static variable for the one-and-only instance  
	static CsvWriter* pInstance;

	// Constant for the filename
//	static const char* const kLogFileName;

	// Data member for the output stream
	std::ofstream mOutputStream;

	// Embedded class to make sure the single CsvWriter
	// instance gets deleted on program shutdown.
	friend class Cleanup;
	class Cleanup
	{
	public:
		~Cleanup();
	};

	// Logs message. The thread should own a lock on sMutex
	// before calling this function.
	void writeHelper(const std::string& inMessage, bool writeTimestamp);

private:
	CsvWriter(std::string& csvHeader);
	virtual ~CsvWriter();
	CsvWriter(const CsvWriter&);
	CsvWriter& operator=(const CsvWriter&);
	void OpenStream();
	std::string& m_csvHeader;
	static std::mutex sMutex;
};
#endif