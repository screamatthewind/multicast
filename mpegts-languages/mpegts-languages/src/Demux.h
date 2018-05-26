/*
 *      Copyright (C) 2013 Jean-Luc Barriere
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#ifndef TEST_DEMUX_H
#define TEST_DEMUX_H

// #include <fcntl.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <cstdio>
#include <string>
#include <inttypes.h>
#include <signal.h>

#include "StreamReader.h"
#include "ChannelUtils.h"
#include "RunningStats.h"
#include "tsDemuxer.h"
#include "CsvWriter.h"
#include "Logger.h"
#include "thread.h"
#include "tools.h"
#include "debug.h"
#include "defs.h"

#define AV_BUFFER_SIZE          1310720
#define POSMAP_PTS_INTERVAL     270000LL
#define CHANNEL_CHANGE_SECS     5 * 60

#define SHOW_STATS_MS 60 * 1000
#define READ_DATA_WAIT_MS 60 * 1000

#define LOGTAG  "[DEMUX] "

class Demux : public TSDemux::TSDemuxer
{
public:
	Demux(cCondWait *signalShutdown, parms_t *parms);
	~Demux();

	void Init();
	void Close();
	void Reset();
	
	void OutputStats(void);
	void OutputCsv();
	void ExecTimeoutProgram();
	
	const unsigned char* ReadAV(uint64_t pos, size_t n, long *result_size);

	parms_t *m_parms;
	string   m_url;
	
	
	TSDemux::AVContext* m_AVContext;
	StreamReader *m_streamReader;

	cCondWait *m_signalShutdown;
	cCondWait *m_signalRestartAll = new cCondWait();
	cCondWait *m_signalShutdownStreamReader = new cCondWait();
	cCondWait *m_signalStreamReaderStarted = new cCondWait();
	cCondWait *m_signalStreamReaderStopped = new cCondWait();
	
	uint16_t m_channel;

	bool get_stream_data(TSDemux::STREAM_PKT* pkt);
	void register_pmt();
	void show_stream_info(uint16_t pid);

	// AV raw buffer
	public : 
	
	size_t m_av_buf_size;            ///< size of av buffer
	uint64_t m_av_pos;               ///< absolute position in av
	unsigned char* m_av_buf;         ///< buffer

	TSDemux::ElementaryStream* m_audioStream;
	TSDemux::ElementaryStream* m_videoStream;
	
	ChannelUtils *m_channelUtils;
	void ChangeChannel(string multicastAddress, int port);
	void NextChannel();

	RunningStats *audioStats;
	RunningStats *videoStats;
	RunningStats *otherStats;
	
	cTimeMs *m_statsTimer;
	cTimeMs *m_dataWaitTimer;
	cTimeMs *m_channelChangeTimer;
	cTimeMs *m_lastTimeoutExecTimer;
	cTimeMs *m_falseAlarmTimer;
	
	int m_timeoutsDetected;
		
	bool m_isDataStarted;
	
	long m_audioDiscontinuities;
	long m_videoDiscontinuities;
	long m_otherDiscontinuities;

	long m_audioPacketCtr;
	long m_videoPacketCtr;
	long m_otherPacketCtr;
	
	long m_audioFrameLoss;
	long m_videoFrameLoss;
	long m_otherFrameLoss;

	long m_otherErrors;
	
	long m_audioDiscontinuitiesCsv;
	long m_videoDiscontinuitiesCsv;
	long m_otherDiscontinuitiesCsv;

	long m_audioPacketCtrCsv;
	long m_videoPacketCtrCsv;
	long m_otherPacketCtrCsv;
	
	long m_audioFrameLossCsv;
	long m_videoFrameLossCsv;
	long m_otherFrameLossCsv;

	long m_otherErrorsCsv;

	bool m_isRunning;
	bool m_isBroken;
	
	unsigned long m_channelChangeDelay;
		
	uint64_t m_prevAudioPts, m_prevVideoPts, m_prevOtherPts;

	pthread_mutex_t mtxStreamReader;

	static void *StartThread(Demux *demux);
	pthread_t Start(void);
	bool Running(void); 

	virtual void Action(void);
	void Cancel(int WaitSeconds);
	bool Active(void);
	void Join(void);
	
	pthread_mutex_t m_mutex;
	bool m_running, m_active;
	pthread_t m_childTid;

	char logBuf[255];
	char timestamp[100];
}
;

#endif /* TEST_DEMUX_H */

