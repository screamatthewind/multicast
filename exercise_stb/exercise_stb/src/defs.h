#ifndef DEFS_H
#define DEFS_H

#define NOT_PI

#include <stdio.h>
#include <string.h>

#include "tools.h"

using namespace std;

#ifndef IFNAMSIZ
#define IFNAMSIZ 32
#endif

#define entry_type_udp 1
#define entry_type_membership_report 2
#define entry_type_membership_query 3
#define entry_type_leave_group 4
#define entry_type_play_request 5
#define entry_type_stop_request 6
#define entry_type_log_request 7
#define entry_type_get_logo_request 8
#define entry_type_set_last_id_request 9
#define entry_type_set_events_request 10
#define entry_type_get_events_request 11
#define entry_type_get_current_request 12
#define entry_type_unknown_request 13
#define entry_type_response 14
#define entry_type_ack 15
#define entry_type_syn 16
#define entry_type_other 17

typedef struct stb_stats_t {
	int entry_type;
	uint64_t elapsed_ms;
	uint64_t byte_ctr;
} stb_stats_t;

#define MAX_STB_DATA_STATS 50

typedef struct stb_data_t {
	int port;
	in_addr_t portal_ip;
	in_addr_t group_ip;
	stb_stats_t stats[MAX_STB_DATA_STATS];
	int num_stats;
	int status;
	cTimeMs *monitorTimer;
	char if_name[IFNAMSIZ];
} stb_data_t;

extern bool sniff_packets(stb_data_t *stb_data);

#endif