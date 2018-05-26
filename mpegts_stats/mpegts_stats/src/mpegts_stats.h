#ifndef MPEGTS_LANGUAGES_H
#define MPEGTS_LANGUAGES_H

#include "Demux.h"
#include "Logger.h"
#include "queue.h"
#include "defs.h"

typedef struct ps_interface_t {
	LIST_ENTRY(ps_interface_t) link;
	
	uint32_t ip_address;
	char     if_name[32];   
} jpi_mrt_channel_t;

#endif
