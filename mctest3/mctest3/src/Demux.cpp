#include "Demux.h"

int g_loglevel = DEMUX_DBG_INFO;

#define PROCESS_TS_MPEG
#define TIMEOUT_EXEC_DELAY 60 // minutes
#define FALSE_ALARM_TIMEOUT 10 // minutes

Demux::Demux(cCondWait *signalShutdown, parms_t *parms)
{
	try
	{
		TSDemux::DBGLevel(g_loglevel);

		srand(time(NULL));   // seed the randomizer
		
		m_streamReader = NULL;
		m_signalShutdown = signalShutdown;
		
		m_statsTimer = new cTimeMs();
		m_dataWaitTimer = new cTimeMs();
		m_channelChangeTimer = new cTimeMs();
		m_lastTimeoutExecTimer = new cTimeMs();
		m_falseAlarmTimer = new cTimeMs();
		
		audioStats = new RunningStats();
		videoStats = new RunningStats();
		otherStats = new RunningStats();

		m_av_buf = NULL;
		m_channelUtils = NULL;
		m_streamReader = NULL;
		m_AVContext = NULL;
		m_timeoutsDetected = 0;
		
		m_parms = parms;

		if (!m_parms->ip_address.empty())
			m_url = "udp://" + m_parms->ip_address + ":" + string(itoa(m_parms->port));
		
		if (pthread_mutex_init(&mtxStreamReader, NULL) < 0)
			fprintf(stderr, "Error: cannot create StreamReader Mutex\n");
		
		// m_csvFileHeader = string("GmtDateTime,Mac,ChannelNum,ChannelName,URL,StartMs,TestSecs,AudioPkts,VideoPkts,OtherPkts,AudioErrs,VideoErrs,OtherErrs,ProgramErrs,AudioPctErr,VideoPctErr,OtherPctErr,AudioLoss,VideoLoss,OtherLoss,CBSize");
		m_csvFileHeader = string("GmtDateTime,Mac,ChannelNum,ChannelName,URL,StartMs,TestSecs,AudioPctErr,VideoPctErr,OtherPctErr");
		strcpy(m_csvFileTimestamp, CsvWriter::instance(m_csvFileHeader).getFileTimestamp());
		
		// only allow writing to database if this is the first instance and the portal address and port are populated
		if ((m_parms->instance_num == 0) && (strlen(m_parms->portal_ip) > 0) && (m_parms->portal_port != 0))
			m_statsWriter = new StatsWriter(m_parms->portal_ip, m_parms->portal_port);
		else
			m_statsWriter = NULL;
		
		// m_mutex = mutex;
		m_childTid = 0;
		m_active = false;
		m_running = false;

		Start();

		// Join();
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Exception %s during %s on line %d\n", e.what(), __func__, __LINE__);
	}
	catch (...) {
		fprintf(stderr, "Unknown Exception during %s on line %d\n", __func__, __LINE__);
	}
}

Demux::~Demux()
{
}

void *Demux::StartThread(Demux *demux)
{
	demux->Action();
	
	return NULL;
}

bool Demux::Running(void) { 
	return m_running; 
}

bool Demux::Active(void)
{
	if (m_active) {
		
		//
		// Single UNIX Spec v2 says:
		//
		// The pthread_kill() function is used to request
		// that a signal be delivered to the specified thread.
		//
		// As in kill(), if sig is zero, error checking is
		// performed but no signal is actually sent.
		//
		int err;
		if ((err = pthread_kill(m_childTid, 0)) != 0) {
			if (err != ESRCH)
				LOG_ERROR;
			m_childTid = 0;
			m_active = m_running = false;
		}

		else
			return true;
	}
	
	return false;
}


#define THREAD_STOP_TIMEOUT  3000 // ms to wait for a thread to stop before newly starting it
#define THREAD_STOP_SLEEP      30 // ms to sleep while waiting for a thread to stop

pthread_t Demux::Start(void)
{
	// pthread_t childTid;
	
	m_active = m_running = true;

	if (pthread_create(&m_childTid, NULL, (void *(*)(void *))&StartThread, (void *)this) == 0) {
		pthread_detach(m_childTid);      // auto-reap
	}

	return m_childTid;
}

void Demux::Cancel(int WaitSeconds)
{
	m_running = false;
	if (m_active && WaitSeconds > -1) {
		if (WaitSeconds > 0) {
			for (time_t t0 = time(NULL) + WaitSeconds; time(NULL) < t0;) {
				if (!Active())
					return;
				cCondWait::SleepMs(10);
			}
			// esyslog("ERROR: %s thread %d won't end (waited %d seconds) - canceling it...", description ? description : "", childThreadId, WaitSeconds);
		}
		pthread_cancel(m_childTid);
		m_childTid = 0;
		m_active = false;
	}
}

void Demux::Join()
{
	pthread_join(m_childTid, NULL);
}

void Demux::Init()
{
	try
	{
		Close();
		
		m_isBroken = false;
		
		m_av_buf_size = AV_BUFFER_SIZE;
		m_av_buf = (unsigned char*)malloc(sizeof(*m_av_buf) * (m_av_buf_size + 1));
		
		if (m_av_buf)
		{
			m_av_pos = 0;
			m_channel = 0;

			m_AVContext = new TSDemux::AVContext(this, 0, m_channel);
		}
		else
		{
			fprintf(stderr, LOGTAG "alloc AV buffer failed\n");
		}
	
		m_channelUtils = new ChannelUtils(m_parms->channel_list_filename);
		
//		if (!m_channelNum.empty())
//			m_channelUtils->getChannelByNumber(m_channelNum);
		
		if (!m_url.empty())
			m_channelUtils->setUrl(m_url, false);

		else
			m_channelUtils->getRandomChannel();
		
		m_streamReader = new StreamReader(m_parms->if_name, m_signalRestartAll, m_signalShutdownStreamReader, m_signalStreamReaderStarted, m_signalStreamReaderStopped, mtxStreamReader);
		m_streamReader->Open(m_channelUtils->m_address, m_channelUtils->m_port);
	
		if (!m_signalStreamReaderStarted->Wait(1000))
		{
			fprintf(stderr, "StreamReader did not start\n");
		}

#ifdef DBG_DEMUX
		GetTimestamp((char *) &timestamp);
		sprintf(logBuf, "%12lu %s %s Demux:Init()", m_streamReader->m_childTid, timestamp, m_channelUtils->m_address.c_str());
		Logger::instance().log(logBuf, Logger::kLogLevelInfo);
		// fprintf(stderr, "%s\n", logBuf);
#endif
		
		m_audioStream = NULL;
		m_videoStream = NULL;

		m_audioDiscontinuities = 0;
		m_videoDiscontinuities = 0;
		m_otherDiscontinuities = 0;

		m_audioPacketCtr = 0;
		m_videoPacketCtr = 0;
		m_otherPacketCtr = 0;
	
		m_audioFrameLoss = 0;
		m_videoFrameLoss = 0;
		m_otherFrameLoss = 0;
	
		m_otherErrors = 0;
	
		m_audioDiscontinuitiesCsv = 0;
		m_videoDiscontinuitiesCsv = 0;
		m_otherDiscontinuitiesCsv = 0;

		m_audioPacketCtrCsv = 0;
		m_videoPacketCtrCsv = 0;
		m_otherPacketCtrCsv = 0;
	
		m_audioFrameLossCsv = 0;
		m_videoFrameLossCsv = 0;
		m_otherFrameLossCsv = 0;
	
		m_otherErrorsCsv = 0;
		
		m_prevAudioPts = m_prevVideoPts = m_prevOtherPts = 0;
	
		m_isRunning = true;
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Exception %s during %s on line %d\n", e.what(), __func__, __LINE__);
	}
	catch (...) {
		fprintf(stderr, "Unknown Exception during %s on line %d\n", __func__, __LINE__);
	}
}

void Demux::Close()
{
	try
	{
#ifdef DBG_DEMUX
		if (m_streamReader)
		{
			GetTimestamp((char *) &timestamp);
			sprintf(logBuf, "%12lu %s %s Demux:Close()", m_streamReader->m_childTid, timestamp, m_channelUtils->m_address.c_str());
			Logger::instance().log(logBuf, Logger::kLogLevelInfo);
			//		fprintf(stderr, "%s\n", logBuf);
		}
#endif
		
		if (m_streamReader)
		{
			m_signalShutdownStreamReader->Signal();
			if (!m_signalStreamReaderStopped->Wait(1000))
			{
				fprintf(stderr, "StreamReader did not shutdown\n");
			}

			m_streamReader = NULL;
		}
		
		if (m_av_buf)
		{
			// fprintf(stderr, LOGTAG "free AV buffer: allocated size was %zu\n", m_av_buf_size);
			free(m_av_buf);
			m_av_buf = NULL;
		}

		if (m_AVContext)
		{
			delete m_AVContext;
			m_AVContext = NULL;
		}

		if (m_channelUtils)
		{
			delete m_channelUtils;
			m_channelUtils = NULL;		
		}
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Exception %s during %s on line %d\n", e.what(), __func__, __LINE__);
	}
	catch (...) {
		fprintf(stderr, "Unknown Exception during %s on line %d\n", __func__, __LINE__);
	}
}

void Demux::Reset()
{
	GetTimestamp((char *) &timestamp);
	sprintf(logBuf, "%12lu %s %s Leave: %s", m_streamReader->m_childTid, timestamp, m_channelUtils->m_address.c_str(), m_channelUtils->getChannelName().c_str());
	Logger::instance().log(logBuf, Logger::kLogLevelInfo);
	fprintf(stderr, "%s\n", logBuf);

#ifdef DBG_DEMUX
	GetTimestamp((char *) &timestamp);
	sprintf(logBuf, "%12lu %s %s Demux:Reset()", m_streamReader->m_childTid, timestamp, m_channelUtils->m_address.c_str());
	Logger::instance().log(logBuf, Logger::kLogLevelInfo);
	// fprintf(stderr, "%s\n", logBuf);
#endif
	
	m_isRunning = false;

	OutputCsv();
	// OutputStats();

	if(m_parms->run_once)
	{
		m_running = m_isRunning = false;
		Close();
	}
	
	// if ((m_channelNum.empty()) && (m_url.empty()))
	if (m_url.empty())
		NextChannel();
}

void Demux::ChangeChannel(string multicastAddress, int port)
{
	try
	{
		m_channelUtils->m_address = multicastAddress;
		m_channelUtils->m_port = port;
		m_channelUtils->m_url = string("udp://") + multicastAddress + string(":") + to_string(port);
		m_channelUtils->saveSettings();
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Exception %s during %s on line %d\n", e.what(), __func__, __LINE__);
	}
	catch (...) {
		fprintf(stderr, "Unknown Exception during %s on line %d\n", __func__, __LINE__);
	}
}

void Demux::NextChannel()
{
	try
	{
		m_channelUtils->getRandomChannel();
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Exception %s during %s on line %d\n", e.what(), __func__, __LINE__);
	}
	catch (...) {
		fprintf(stderr, "Unknown Exception during %s on line %d\n", __func__, __LINE__);
	}
}

void Demux::ExecTimeoutProgram()
{
	// only execute timeout program periodically
	if (!m_lastTimeoutExecTimer->TimedOut())
		return;
	
	m_lastTimeoutExecTimer->Set(TIMEOUT_EXEC_DELAY * 60 * 1000); // minutes
	m_timeoutsDetected = 0;
	
	GetTimestamp((char *) &timestamp);
	sprintf(logBuf, "%12lu %s %s Executing timeout program: %s", m_streamReader->m_childTid, timestamp, m_channelUtils->m_address.c_str(), m_channelUtils->getChannelName().c_str());
	Logger::instance().log(logBuf, Logger::kLogLevelInfo);
	fprintf(stderr, "%s\n", logBuf);
	
	int pid= fork();             

	if (pid == 0) {
		execv(m_parms->timeout_exec_filename.c_str(), NULL);
		fprintf(stderr, "Child process could not do execv: %s\n", m_parms->timeout_exec_filename.c_str());
	}
}

const unsigned char* Demux::ReadAV(uint64_t pos, size_t n, long *result_size)
{
	try
	{
		if (m_isBroken)
		{
			*result_size = -1;
			return NULL;
		}
		
		int ctr = 0;
	
		m_dataWaitTimer->Set(READ_DATA_WAIT_MS);
		
		while (m_isRunning)
		{
			*result_size = m_streamReader->read(m_av_buf, pos, n);
			
			if (*result_size < 0) 
			{
#ifdef DBG_DEMUX
				GetTimestamp((char *) &timestamp);
				sprintf(logBuf, "%12lu %s %s Demux::ReadAV - Broken", m_streamReader->m_childTid, timestamp, m_channelUtils->m_address.c_str());
				Logger::instance().log(logBuf, Logger::kLogLevelInfo);
				fprintf(stderr, "%s\n", logBuf);
#endif

				m_isBroken = true;
				m_signalRestartAll->Signal();
				break;
			}

			else if (*result_size == 0)
			{
				ctr++;

#ifdef DBG_DEMUX
				GetTimestamp((char *) &timestamp);
				sprintf(logBuf, "%12lu %s %s ReadAV() ctr: %d  size: %zu  pos: %" PRIu64, m_streamReader->m_childTid, timestamp, m_channelUtils->m_address.c_str(), ctr, n, pos);
				Logger::instance().log(logBuf, Logger::kLogLevelInfo);
				// fprintf(stderr, "%s\n", logBuf);
#endif 
				
				cCondWait::SleepMs(1); 
			}
			
			else
			{
				ctr = 0;
				break;
			}

			if (m_dataWaitTimer->TimedOut())
			{
				GetTimestamp((char *) &timestamp);
				sprintf(logBuf, "%12lu %s %s Demux::ReadAV - Timed out waiting for data", m_streamReader->m_childTid, timestamp, m_channelUtils->m_address.c_str());
				Logger::instance().log(logBuf, Logger::kLogLevelInfo);
				fprintf(stderr, "%s\n", logBuf);
			
				m_dataWaitTimer->Set(READ_DATA_WAIT_MS);

				if (!m_parms->timeout_exec_filename.empty())
				{
					if (m_timeoutsDetected == 0)
						m_falseAlarmTimer->Set(FALSE_ALARM_TIMEOUT * 60 * 1000); // minutes
					
					m_timeoutsDetected++;
				}
				
				m_isBroken = true;
				m_signalRestartAll->Signal();

				*result_size = -1;   // signal an error to calling function
				
				break;
			}

			// only exec timeout program if more then 1 timeout is detected during the false alarm timeout period
			if ((m_timeoutsDetected > 0) && (m_falseAlarmTimer->TimedOut()))
			{
				if (m_timeoutsDetected > 1)
					ExecTimeoutProgram();
				
				m_timeoutsDetected = 0;
			}
		}

		return m_av_buf; 
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Exception %s during %s on line %d\n", e.what(), __func__, __LINE__);
		return NULL;
	}
	catch (...) {
		fprintf(stderr, "Unknown Exception during %s on line %d\n", __func__, __LINE__);
		return NULL;
	}
}

void Demux::register_pmt()
{
	try
	{
		const std::vector<TSDemux::ElementaryStream*> es_streams = m_AVContext->GetStreams();
		if (!es_streams.empty())
		{
			//		for (unsigned int i = 0; i < es_streams.size(); i++)
			//		{
			//			if (es_streams[i]->stream_type == TSDemux::STREAM_TYPE_VIDEO_H264)
			//				m_videoStream = es_streams[i];
			//		}
		
			for(std::vector<TSDemux::ElementaryStream*>::const_iterator it = es_streams.begin() ; it != es_streams.end() ; ++it)
				m_AVContext->StartStreaming((*it)->pid);
		}
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Exception %s during %s on line %d\n", e.what(), __func__, __LINE__);
	}
	catch (...) {
		fprintf(stderr, "Unknown Exception during %s on line %d\n", __func__, __LINE__);
	}
}

void Demux::show_stream_info(uint16_t pid)
{
	try
	{
		TSDemux::ElementaryStream* es = m_AVContext->GetStream(pid);
		if (!es)
			return;

		//	uint16_t channel = m_AVContext->GetChannel(pid);
		//	fprintf(stderr, LOGTAG "dump stream infos for channel %u PID %.4x\n", channel, es->pid);
		//	fprintf(stderr, "  Codec name     : %s\n", es->GetStreamCodecName());
		//	fprintf(stderr, "  Language       : %s\n", es->stream_info.language);
		//	fprintf(stderr, "  Identifier     : %.8x\n", stream_identifier(es->stream_info.composition_id, es->stream_info.ancillary_id));
		//	fprintf(stderr, "  FPS scale      : %d\n", es->stream_info.fps_scale);
		//	fprintf(stderr, "  FPS rate       : %d\n", es->stream_info.fps_rate);
		//	fprintf(stderr, "  Interlaced     : %s\n", (es->stream_info.interlaced ? "true" : "false"));
		//	fprintf(stderr, "  Height         : %d\n", es->stream_info.height);
		//	fprintf(stderr, "  Width          : %d\n", es->stream_info.width);
		//	fprintf(stderr, "  Aspect         : %3.3f\n", es->stream_info.aspect);
		//	fprintf(stderr, "  Channels       : %d\n", es->stream_info.channels);
		//	fprintf(stderr, "  Sample rate    : %d\n", es->stream_info.sample_rate);
		//	fprintf(stderr, "  Block align    : %d\n", es->stream_info.block_align);
		//	fprintf(stderr, "  Bit rate       : %d\n", es->stream_info.bit_rate);
		//	fprintf(stderr, "  Bit per sample : %d\n", es->stream_info.bits_per_sample);
		//	fprintf(stderr, "\n");

			if((es->stream_type == TSDemux::STREAM_TYPE_AUDIO_AAC) || (es->stream_type == TSDemux::STREAM_TYPE_AUDIO_AAC_ADTS) || (es->stream_type == TSDemux::STREAM_TYPE_AUDIO_AAC_LATM))
		{
			if (strncmp(es->stream_info.language, "eng", 3) == 0)
				m_audioStream = es;
		}

		if (es->stream_type == TSDemux::STREAM_TYPE_VIDEO_H264)
			m_videoStream = es;
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Exception %s during %s on line %d\n", e.what(), __func__, __LINE__);
	}
	catch (...) {
		fprintf(stderr, "Unknown Exception during %s on line %d\n", __func__, __LINE__);
	}
}

bool Demux::get_stream_data(TSDemux::STREAM_PKT* pkt)
{
	try
	{
		TSDemux::ElementaryStream* es = m_AVContext->GetPIDStream();
		if (!es)
			return false;

		if (!es->GetStreamPacket(pkt))
			return false;

		if (pkt->duration > 180000)
			pkt->duration = 0;
	
		return true;
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Exception %s during %s on line %d\n", e.what(), __func__, __LINE__);
	}
	catch (...) {
		fprintf(stderr, "Unknown Exception during %s on line %d\n", __func__, __LINE__);
	}
}

void Demux::OutputStats()
{
	try
	{
		float audioPctLoss;
		float videoPctLoss;
		float otherPctLoss;
	
		if (m_audioDiscontinuities == 0)
			audioPctLoss = 0;
		else
			audioPctLoss = 100 - ((((float)(m_audioPacketCtr - m_audioDiscontinuities)) / ((float) m_audioPacketCtr)) * 100);
	
		if (m_videoDiscontinuities == 0)
			videoPctLoss = 0;
		else
			videoPctLoss = 100 - ((((float)(m_videoPacketCtr - m_videoDiscontinuities)) / ((float) m_videoPacketCtr)) * 100);
	
		if (m_otherDiscontinuities == 0)
			otherPctLoss = 0;
		else
			otherPctLoss = 100 - ((((float)(m_otherPacketCtr - m_otherDiscontinuities)) / ((float) m_otherPacketCtr)) * 100);
	
		char buf[512];
		sprintf(buf,
			"Pkts: a %5ld v %5ld o %5ld  Errors: a %4ld v %4ld o %4ld x %4ld Err%%: a %3.1f%% v %3.1f%% o %3.1f%%  Frame Loss: a %4ld v %4ld o %4ld", 
			m_audioPacketCtr,
			m_videoPacketCtr,
			m_otherPacketCtr, 
			m_audioDiscontinuities,
			m_videoDiscontinuities,
			m_otherDiscontinuities, 
			m_otherErrors,
			audioPctLoss,
			videoPctLoss,
			otherPctLoss,
			m_audioFrameLoss,
			m_videoFrameLoss,
			m_otherFrameLoss);
	
		string strBuf(buf);
		Logger::instance().log(strBuf, Logger::kLogLevelInfo);

		cout << strBuf << endl;
	
		m_audioDiscontinuities = 0;
		m_videoDiscontinuities = 0;
		m_otherDiscontinuities = 0;
	
		m_audioPacketCtr = 0;
		m_videoPacketCtr = 0;
		m_otherPacketCtr = 0;

		m_audioFrameLoss = 0;
		m_videoFrameLoss = 0;
		m_otherFrameLoss = 0;
	
		m_otherErrors = 0;
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Exception %s during %s on line %d\n", e.what(), __func__, __LINE__);
	}
	catch (...) {
		fprintf(stderr, "Unknown Exception during %s on line %d\n", __func__, __LINE__);
	}
}

void Demux::OutputCsv()
{
	try
	{
		char buf[512];
		string strBuf;

		uint64_t dataStartedTime = m_streamReader->m_dataStartedTime;

		if ((m_audioPacketCtrCsv == 0) && (m_videoPacketCtrCsv == 0) && (m_otherPacketCtrCsv == 0))
			dataStartedTime = 999999;
		
		float audioPctLossCsv;
		float videoPctLossCsv;
		float otherPctLossCsv;
	
		if (m_audioDiscontinuitiesCsv == 0)
			audioPctLossCsv = 0;
		else
			audioPctLossCsv = 100 - ((((float)(m_audioPacketCtrCsv - m_audioDiscontinuitiesCsv)) / ((float) m_audioPacketCtrCsv)) * 100);
	
		if (m_videoDiscontinuitiesCsv == 0)
			videoPctLossCsv = 0;
		else
			videoPctLossCsv = 100 - ((((float)(m_videoPacketCtrCsv - m_videoDiscontinuitiesCsv)) / ((float) m_videoPacketCtrCsv)) * 100);
	
		if (m_otherDiscontinuitiesCsv == 0)
			otherPctLossCsv = 0;
		else
			otherPctLossCsv = 100 - ((((float)(m_otherPacketCtrCsv - m_otherDiscontinuitiesCsv)) / ((float) m_otherPacketCtrCsv)) * 100);
	
//		sprintf(buf,
//			"%s,%s,%s,%s,%" PRIu64 ",%ld,%ld,%ld,%ld,%ld,%ld,%ld,%ld,%.1f%%,%.1f%%,%.1f%%,%ld,%ld,%ld,%zu",
//			m_parms->mac_address,
//			m_channelUtils->getChannelNumber().c_str(),
//			m_channelUtils->getChannelName().c_str(),
//			m_channelUtils->m_url.c_str(),
//			dataStartedTime,
//			m_channelChangeDelay,
//			m_audioPacketCtrCsv,
//			m_videoPacketCtrCsv,
//			m_otherPacketCtrCsv, 
//			m_audioDiscontinuitiesCsv,
//			m_videoDiscontinuitiesCsv,
//			m_otherDiscontinuitiesCsv, 
//			m_otherErrorsCsv,
//			audioPctLossCsv,
//			videoPctLossCsv,
//			otherPctLossCsv,
//			m_audioFrameLossCsv,
//			m_videoFrameLossCsv,
//			m_otherFrameLossCsv,
//			m_streamReader->m_cbContext.max_size);
	
		sprintf(buf,
			"%s,%s,%s,%s,%" PRIu64 ",%ld,%.1f%%,%.1f%%,%.1f%%",
			m_parms->mac_address,
			m_channelUtils->getChannelNumber().c_str(),
			m_channelUtils->getChannelName().c_str(),
			m_channelUtils->m_url.c_str(),
			dataStartedTime,
			m_channelChangeDelay,
			audioPctLossCsv,
			videoPctLossCsv,
			otherPctLossCsv);
		
		m_streamReader->m_cbContext.max_size = m_streamReader->m_cbContext.size;

		strBuf = string(buf);
		CsvWriter::instance(m_csvFileHeader).write(strBuf);	
		
		m_audioDiscontinuitiesCsv = 0;
		m_videoDiscontinuitiesCsv = 0;
		m_otherDiscontinuitiesCsv = 0;
	
		m_audioPacketCtrCsv = 0;
		m_videoPacketCtrCsv = 0;
		m_otherPacketCtrCsv = 0;

		m_audioFrameLossCsv = 0;
		m_videoFrameLossCsv = 0;
		m_otherFrameLossCsv = 0;
	
		m_otherErrorsCsv = 0;

//		if (m_statsWriter != NULL)
//		{
//			strcpy(buf, CsvWriter::instance(m_csvFileHeader).getFileTimestamp());
//
//			if (strcmp(m_csvFileTimestamp, buf) != 0)
//			{
//				m_statsWriter->SaveToDatabase(buf);
//				strcpy(m_csvFileTimestamp, buf);
//			}
//		}
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Exception %s during %s on line %d\n", e.what(), __func__, __LINE__);
	}
	catch (...) {
		fprintf(stderr, "Unknown Exception during %s on line %d\n", __func__, __LINE__);
	}
}

void Demux::Action()
{
	try
	{
		// fprintf(stderr, "Thread Id: %d\n", ThreadId());
	
		while(Running())
		{
			int ret = 0;	

			Init();

			if (m_parms->channel_change_delay > 0)
				m_channelChangeDelay =  m_parms->channel_change_delay;
			else
				m_channelChangeDelay =  (rand() % CHANNEL_CHANGE_SECS) + 60;

			m_channelChangeTimer->Set(m_channelChangeDelay * 1000);

			GetTimestamp((char *) &timestamp);
			sprintf(logBuf, "%12lu %s %s Join: %s  Test Secs: %ld", m_streamReader->m_childTid, timestamp, m_channelUtils->m_address.c_str(), m_channelUtils->getChannelName().c_str(), m_channelChangeDelay);
			Logger::instance().log(logBuf, Logger::kLogLevelInfo);
			fprintf(stderr, "%s\n", logBuf);
	
			m_statsTimer->Set(SHOW_STATS_MS);

			while (m_isRunning)
			{
				if (m_signalShutdown->WaitUs(1))
				{
					GetTimestamp((char *) &timestamp);
					sprintf(logBuf, "%12lu %s %s Demux shutting down", m_streamReader->m_childTid, timestamp, m_channelUtils->m_address.c_str());
					Logger::instance().log(logBuf, Logger::kLogLevelInfo);
					fprintf(stderr, "%s\n", logBuf);
					fflush(stderr);
					
					m_running = m_isRunning = false;
					
					OutputCsv();
					OutputStats();

					Close();
					
					break;
				}
				
				if (m_signalRestartAll->WaitUs(1)) 
				{
					GetTimestamp((char *) &timestamp);
					sprintf(logBuf, "%12lu %s %s Demux::Action - Got Restart All Signal", m_streamReader->m_childTid, timestamp, m_channelUtils->m_address.c_str());
					Logger::instance().log(logBuf, Logger::kLogLevelInfo);
					fprintf(stderr, "%s\n", logBuf);

					Reset();
					break;
				}

				if (m_isBroken)
				{
					GetTimestamp((char *) &timestamp);
					sprintf(logBuf, "%12lu %s %s Demux::Action - Broken", m_streamReader->m_childTid, timestamp, m_channelUtils->m_address.c_str());
					Logger::instance().log(logBuf, Logger::kLogLevelInfo);
					fprintf(stderr, "%s\n", logBuf);
			
					Reset();
					break;
				}
				
				if (m_channelChangeTimer->TimedOut())
				{
					Reset();
					break;
				}

#ifdef PROCESS_TS_MPEG
				
				if (m_statsTimer->TimedOut())
				{
					// OutputStats();

					m_statsTimer->Set(SHOW_STATS_MS);
				}

				if (!m_isRunning)
					continue;
				
				ret = m_AVContext->TSResync(m_streamReader->m_childTid);
				if (ret != TSDemux::AVCONTEXT_CONTINUE)
				{
					if (ret != TSDemux::AVCONTEXT_NO_DATA)
						fprintf(stderr, "Error during TSResync(): %d\n", ret);

					continue;
				}

				if (!m_isRunning)
					continue;

				uint16_t tsPid;
				ret = m_AVContext->ProcessTSPacket(&tsPid);
				if (ret < 0)
					fprintf(stderr, "Error during ProcessTSPacket(): %d\n", ret);
		
				if ((m_audioStream) && (m_audioStream->pid == tsPid))
				{
					m_audioPacketCtr++;
					m_audioPacketCtrCsv++;
				}
		
				else if ((m_videoStream) && (m_videoStream->pid == tsPid))
				{
					m_videoPacketCtr++;
					m_videoPacketCtrCsv++;
				}
		
				else
				{
					m_otherPacketCtr++;
					m_otherPacketCtrCsv++;
				}

				if (ret == TSDemux::AVCONTEXT_DISCONTINUITY)
				{
#ifdef DBG_DEMUX
					GetTimestamp((char *) &timestamp);
					sprintf(logBuf, "%12lu %s %s Demux::AVCONTEXT_DISCONTINUITY", m_streamReader->m_childTid, timestamp, m_channelUtils->m_address.c_str());
					Logger::instance().log(logBuf, Logger::kLogLevelInfo);
					// fprintf(stderr, "%s\n", logBuf);
#endif
					
					if((m_audioStream) && (m_audioStream->pid == tsPid))
					{
						m_audioDiscontinuities++;
						m_audioDiscontinuitiesCsv++;
					}
				
					else if((m_videoStream) && (m_videoStream->pid == tsPid))
					{
						m_videoDiscontinuities++;
						m_videoDiscontinuitiesCsv++;
					}
			
					else
					{
						m_otherDiscontinuities++;
						m_otherDiscontinuitiesCsv++;
					}
				}

				if (!m_isRunning)
					continue;

				int getStreamDataCtr = 0;
			
				if (m_AVContext->HasPIDStreamData())
				{
					TSDemux::STREAM_PKT pkt;
					while (get_stream_data(&pkt))
					{
						if (getStreamDataCtr++ > 10)
						{
							//						Logger::instance().log("Get Stream Data Stuck", Logger::kLogLevelError);
							//						cout << endl << "Get Stream Data Stuckk" << endl;
						
							m_otherErrors++;
							m_otherErrorsCsv++;
							break;
						}
					
						if (pkt.streamChange)
							show_stream_info(pkt.pid);

						if ((m_audioStream) && (m_audioStream->pid == pkt.pid))
						{

							if (m_prevAudioPts != 0)
							{
								uint64_t ptsDelta; 
		
								if (pkt.pts < m_prevAudioPts)
								{
#ifdef DBG_DEMUX
									GetTimestamp((char *) &timestamp);
									sprintf(logBuf, "%12lu %s %s Audio pts out of order: %" PRIu64 ", %" PRIu64, m_streamReader->m_childTid, timestamp, m_channelUtils->m_address.c_str(), pkt.pts, m_prevAudioPts);
									Logger::instance().log(logBuf, Logger::kLogLevelInfo);
									// fprintf(stderr, "%s\n", logBuf);
#endif
									ptsDelta = m_prevAudioPts - pkt.pts;
								}
								else 
									ptsDelta = pkt.pts - m_prevAudioPts;

								audioStats->Push(ptsDelta);
								if (audioStats->NumDataValues() > 100)
								{
									int offset = (int)(audioStats->Mean() * 0.1f);
									if (audioStats->StandardDeviation() > offset)
										offset = audioStats->StandardDeviation();
			
									if (ptsDelta > audioStats->Mean() + offset)
									{
										m_audioFrameLoss = ptsDelta / audioStats->Mean();
										m_audioFrameLossCsv = ptsDelta / audioStats->Mean();
										// TSDemux::DBG(DEMUX_DBG_WARN, "High audio pts delta: %" PRIu64 " Mean: %.0f StdDev: %.0f Skew: %.2f Kurtosis: %.2f\n", ptsDelta, audioStats->Mean(), audioStats->StandardDeviation(), audioStats->Skewness(), audioStats->Kurtosis());
									}
								}
							}
					
							m_prevAudioPts = pkt.pts;
						}

						else if ((m_videoStream) && (m_videoStream->pid == pkt.pid))
						{
							if (m_prevVideoPts != 0)
							{
								uint64_t ptsDelta; 
		
								if (pkt.pts < m_prevVideoPts)
								{
#ifdef DBG_DEMUX
									GetTimestamp((char *) &timestamp);
									sprintf(logBuf, "%12lu %s %s Video pts out of order: %" PRIu64 ", %" PRIu64, m_streamReader->m_childTid, timestamp, m_channelUtils->m_address.c_str(), pkt.pts, m_prevVideoPts);
									// Logger::instance().log(logBuf, Logger::kLogLevelInfo);
									// fprintf(stderr, "%s\n", logBuf);
#endif			
									ptsDelta = m_prevVideoPts - pkt.pts;
								}
								else 
									ptsDelta = pkt.pts - m_prevVideoPts;

								videoStats->Push(ptsDelta);
								if (videoStats->NumDataValues() > 100)
								{
									int offset = (int)(videoStats->Mean() * 0.1f);
									if (videoStats->StandardDeviation() > offset)
										offset = videoStats->StandardDeviation();
			
									if (ptsDelta > videoStats->Mean() + offset)
									{
										m_videoFrameLoss = ptsDelta / videoStats->Mean();
										m_videoFrameLossCsv = ptsDelta / videoStats->Mean();
										// TSDemux::DBG(DEMUX_DBG_WARN, "High video pts delta: %" PRIu64 " Mean: %.0f StdDev: %.0f Skew: %.2f Kurtosis: %.2f\n", ptsDelta, videoStats->Mean(), videoStats->StandardDeviation(), videoStats->Skewness(), videoStats->Kurtosis());
									}
								}
							}
					
							m_prevVideoPts = pkt.pts;
						}
						else 
						{
							if (m_prevOtherPts != 0)
							{
								uint64_t ptsDelta; 
		
								if (pkt.pts < m_prevOtherPts)
								{
#ifdef DBG_DEMUX
									GetTimestamp((char *) &timestamp);
									sprintf(logBuf, "%12lu %s %s Other pts out of order: %" PRIu64 ", %" PRIu64, m_streamReader->m_childTid, timestamp, m_channelUtils->m_address.c_str(), pkt.pts, m_prevOtherPts);
									Logger::instance().log(logBuf, Logger::kLogLevelInfo);
									// fprintf(stderr, "%s\n", logBuf);
#endif			
									ptsDelta = m_prevOtherPts - pkt.pts;
								}
								else 
									ptsDelta = pkt.pts - m_prevOtherPts;

								otherStats->Push(ptsDelta);
								if (otherStats->NumDataValues() > 100)
								{
									int offset = (int)(otherStats->Mean() * 0.1f);
									if (otherStats->StandardDeviation() > offset)
										offset = otherStats->StandardDeviation();
			
									if (ptsDelta > otherStats->Mean() + offset)
									{
										m_otherFrameLoss = ptsDelta / otherStats->Mean();
										m_otherFrameLossCsv = ptsDelta / otherStats->Mean();
										// TSDemux::DBG(DEMUX_DBG_WARN, "High other pts delta: %" PRIu64 " Mean: %.0f StdDev: %.0f Skew: %.2f Kurtosis: %.2f\n", ptsDelta, otherStats->Mean(), otherStats->StandardDeviation(), otherStats->Skewness(), otherStats->Kurtosis());
									}
								}
							}
					
							m_prevOtherPts = pkt.pts;
						}
					}
				}
		
				if (!m_isRunning)
					continue;

				if (m_AVContext->HasPIDPayload())
				{
					ret = m_AVContext->ProcessTSPayload();
					if (ret == TSDemux::AVCONTEXT_PROGRAM_CHANGE)
					{
						register_pmt();
						std::vector<TSDemux::ElementaryStream*> streams = m_AVContext->GetStreams();
						for (std::vector<TSDemux::ElementaryStream*>::const_iterator it = streams.begin(); it != streams.end(); ++it)
						{
							if ((*it)->has_stream_info)
								show_stream_info((*it)->pid);
						}
					}
				}

				if (ret < 0)
				{
					fprintf(stderr, LOGTAG "%s: error %d\n", __FUNCTION__, ret);
					m_otherErrors++;
					m_otherErrorsCsv++;
				
					GetTimestamp((char *) &timestamp);
					sprintf(logBuf, "%12lu %s %s Demux::Action - Fatal Error", m_streamReader->m_childTid, timestamp, m_channelUtils->m_address.c_str());
					Logger::instance().log(logBuf, Logger::kLogLevelInfo);
					fprintf(stderr, "%s\n", logBuf);

					Reset();
					break;
				}

				if (ret == TSDemux::AVCONTEXT_TS_ERROR)
					m_AVContext->Shift();
				else
					m_AVContext->GoNext();
#endif				
			}
		}

		fprintf(stderr, "Main thread exited\n");
	
		//	return ret;
	}
	catch (const std::exception& e) {
		fprintf(stderr, "Exception %s during %s on line %d\n", e.what(), __func__, __LINE__);
	}
	catch (...) {
		fprintf(stderr, "Unknown Exception during %s on line %d\n", __func__, __LINE__);
	}
}