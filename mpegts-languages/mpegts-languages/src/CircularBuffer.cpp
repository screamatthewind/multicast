#include "CircularBuffer.h"
//#include <gperftools/profiler.h>

CircularBuffer::CircularBuffer(CircularBufferContext *cbContext, size_t capacity, cCondWait *signalRestartAll)
{
	m_signalRestartAll = signalRestartAll;
	m_cbContext = cbContext;
	
	cbContext->beg_index = 0;
	cbContext->end_index = 0;
	cbContext->capacity = capacity;
	cbContext->position = 0;
	cbContext->size = 0;
	cbContext->max_size = 0;
	
	m_ignorePosition = false;	
	m_prevPosition = 0;
	
	m_isBroken = false;
	
#ifdef DBG_CIRCULAR_BUFFER
	GetTimestamp((char *) &timestamp);
	sprintf(logBuf, "%12lu %s New CircularBuffer", m_cbContext->thread_id, timestamp);
	Logger::instance().log(logBuf, Logger::kLogLevelInfo);
	// printf("%s\n", logBuf);
#endif
}

CircularBuffer::~CircularBuffer()
{
	close();
//	printf("~CircularBuffer()\n");
}

void CircularBuffer::close()
{
//	printf("CircularBuffer::close()\n");

//	m_cbContext->beg_index = 0;
//	m_cbContext->end_index = 0;
//	m_cbContext->size = 0;
//	m_cbContext->capacity = 0;
//	m_cbContext->position = 0;
	
	// delete[] m_cbContext->data;
}

long CircularBuffer::write(const unsigned char *data, size_t bytes)
{
	if (m_isBroken) {
		cCondWait::SleepMs(5);
		return -1;
	}	
	
	if (bytes == 0) return 0;

	size_t capacity = m_cbContext->capacity;
	size_t bytes_to_write; // = std::min(bytes, (capacity-1) - m_cbContext->size);

	if (bytes > ((capacity - 1) - m_cbContext->size))
		bytes_to_write = ((capacity - 1) - m_cbContext->size);
	else
		bytes_to_write = bytes;
	
	if (bytes != bytes_to_write)
	{
		m_isBroken = true;

		GetTimestamp((char *) &timestamp);
		sprintf(logBuf, "%12lu %s Error: CircularBuffer is full: resetting", m_cbContext->thread_id, timestamp);
		Logger::instance().log(logBuf, Logger::kLogLevelError);
		// printf("%s\n", logBuf);

		m_signalRestartAll->Signal();
		cCondWait::SleepMs(10);
		
		return -1;
	}
	
	if(m_cbContext->size + bytes_to_write > capacity)
	{
		m_isBroken = true;

		m_cbContext->max_size = m_cbContext->size;
		
		printf("Error: CircularBuffer Overflow: resettings\n");
		m_signalRestartAll->Signal();
		cCondWait::SleepMs(10);
	}
	
	if (bytes_to_write != 0)
	{
		// Write in a single step
		if(bytes_to_write <= capacity - m_cbContext->end_index)
		{
			memcpy(m_cbContext->data + m_cbContext->end_index, data, bytes_to_write);
			m_cbContext->end_index += bytes_to_write;
			if (m_cbContext->end_index == capacity) m_cbContext->end_index = 0;
		}
		// Write in two steps
		else
		{
			size_t size_1 = capacity - m_cbContext->end_index;
			memcpy(m_cbContext->data + m_cbContext->end_index, data, size_1);
		
			size_t size_2 = bytes_to_write - size_1;
			memcpy(m_cbContext->data, data + size_1, size_2);
		
			m_cbContext->end_index = size_2;
		}
	}

	calculateSize();
	
#ifdef DBG_CIRCULAR_BUFFER
	GetTimestamp((char *) &timestamp);
	sprintf(logBuf, "%12lu %s CircularBuffer write: %zu  size: %zd  beg_ptr: %zd  end_ptr: %zd  diff: %zd", m_cbContext->thread_id, timestamp, bytes, m_cbContext->size, m_cbContext->beg_index, m_cbContext->end_index, m_cbContext->end_index - m_cbContext->beg_index);
	Logger::instance().log(logBuf, Logger::kLogLevelInfo);
//	printf("%s\n", logBuf);
#endif
	
	return bytes_to_write;
}

long CircularBuffer::read(unsigned char *data, uint64_t pos, size_t bytes)
{
//	static bool profilerStarted = false;
	
#ifdef DBG_CIRCULAR_BUFFER
	GetTimestamp((char *) &timestamp);
	sprintf(logBuf, "%12lu %s CircularBuffer read: %zu  size: %zd  beg_ptr: %zd  end_ptr: %zd  pos: %" PRIu64, m_cbContext->thread_id, timestamp, bytes, m_cbContext->size, m_cbContext->beg_index, m_cbContext->end_index, pos);
	Logger::instance().log(logBuf, Logger::kLogLevelInfo);
//	printf("%s\n", logBuf);
#endif
	
	if(m_isBroken) {
		cCondWait::SleepMs(5);
		return -1;
	}	

	if (bytes == 0) return 0;

	if ((m_ignorePosition == true) && (pos < m_prevPosition))
		printf("CircularBuffer::read - Position went backwards\n");
	
	size_t capacity = m_cbContext->capacity;
	size_t bytes_to_read = bytes;  // std::min(bytes, m_cbContext->size);

	// give reader time to sync up
	if(!m_ignorePosition)
	{
		if (pos < 1000)
		{
			m_cbContext->beg_index = pos;
			m_cbContext->position = pos;
		}
		else
			m_ignorePosition = true;
	}
	
//	if (m_cbContext->position != pos)
//		printf("CircularBuffer out of sync: %d  %d\n", m_cbContext->position, pos);
			
	if (isEmpty())
	{
		// printf("0");

		data = NULL;
		return 0;
	}
	
	if (bytes_to_read > m_cbContext->size)
	{
		// printf("CircularBuffer not enough data during read: %d  %d\n", bytes_to_read, m_cbContext->size);

		data = NULL;
		return 0;
	}
	
	size_t save_beg_index = m_cbContext->beg_index;
	size_t save_end_index = m_cbContext->end_index;
	
	// Read in a single step
	if(bytes_to_read <= capacity - m_cbContext->beg_index)
	{
		memcpy(data, m_cbContext->data + m_cbContext->beg_index, bytes_to_read);
		m_cbContext->beg_index += bytes_to_read;
		if (m_cbContext->beg_index >= capacity) 
			m_cbContext->beg_index = 0;
	}
	// Read in two steps
	else
	{
		size_t size_1 = capacity - m_cbContext->beg_index;
		memcpy(data, m_cbContext->data + m_cbContext->beg_index, size_1);
		
		size_t size_2 = bytes_to_read - size_1;
		memcpy(data + size_1, m_cbContext->data, size_2);
		m_cbContext->beg_index = size_2;
	}

//	if (m_cbContext->size > 200000)
//	{
//		if (!profilerStarted) {
//			// ProfilerStart("mpegts-languages_perf.log");
//			profilerStarted = true;
//			printf("PROFILER STARTED\n");
//		}
//		
//	}	
	
	size_t pos_delta = pos - m_cbContext->position;
	
	if ((m_ignorePosition) && (bytes_to_read != pos_delta))
	{
		m_cbContext->beg_index = save_beg_index;
		m_cbContext->end_index = save_end_index;
		
		if (pos_delta <= capacity - m_cbContext->beg_index)
		{
			m_cbContext->beg_index += pos_delta;
			if (m_cbContext->beg_index >= capacity) 
				m_cbContext->beg_index = 0;
		}
		else
		{
			size_t size_1 = capacity - m_cbContext->beg_index;
			size_t size_2 = pos_delta - size_1;

			m_cbContext->beg_index = size_2;
		}
	}
	
	calculateSize();

	if (m_ignorePosition)
		m_cbContext->position += pos_delta;

	m_prevPosition = pos;
	
	return bytes_to_read;
}

bool CircularBuffer::isEmpty()
{
	if (m_cbContext->beg_index == m_cbContext->end_index)
		return true;
	
	return false;
}

void CircularBuffer::calculateSize()
{
	if (m_cbContext->beg_index == m_cbContext->end_index)
		m_cbContext->size = 0;
	else
		m_cbContext->size = (m_cbContext->end_index > m_cbContext->beg_index) ? m_cbContext->end_index - m_cbContext->beg_index : m_cbContext->capacity - m_cbContext->beg_index + m_cbContext->end_index;

	if(m_cbContext->size > m_cbContext->max_size)
		m_cbContext->max_size = m_cbContext->size;
}

size_t CircularBuffer::capacity()
{ 
	return m_cbContext->capacity; 
}

size_t CircularBuffer::GetMaxSize()
{
	size_t result = m_cbContext->max_size;
	m_cbContext->max_size = m_cbContext->size;

	return result;
}