#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <getopt.h>
#include <unistd.h>

#include "mr_cache.h"
#include "utils.h"

#define PACKAGE_VERSION "1.0.8"

#define LINESZ 1024
#define MAX_ENTRIES 1024

//#define ETH_VIF 1
//#define ETH_NAME "eno1"
// #define ETH_NAME "eth1"

typedef struct ip_mr_cache {
	char group[32];
	char origin[32];
	short iif;
	unsigned long pkts;
	unsigned long bytes;
	unsigned long wrong;
	int oifs1;
	int oifs2;
} ip_mr_cache;

ip_mr_cache entries1[MAX_ENTRIES];
ip_mr_cache entries2[MAX_ENTRIES];

typedef struct ipmr_vifs_t {
	int  ifNum;
	char ifName[32];
	unsigned long bytesIn;
	unsigned long pktsIn;
	unsigned long bytesOut;
	unsigned long pktsOut;
	int flags;
	int local;
	int remote;
} ipmr_vifs_t;

ipmr_vifs_t vifs[MAX_ENTRIES];
int num_vifs;

int size1, size2;

char iif1[32];
char iif2[32];

FILE *logFile = NULL;
char *prognm  = NULL;

int  iif_num = -1;
char *iif_name;

char hostname[1024];

static char *progname(char *arg0)
{
	char *nm;

	nm = strrchr(arg0, '/');
	if (nm)
		nm++;
	else
		nm = arg0;

	return nm;
}

void usage()
{
	printf("Usage: %s [-iv?]\n", prognm);
	printf("  -i, --iif=IIF               Input interface name\n");
	printf("  -v, --version               Show version\n");
	printf("  -?, --help                  This message\n");
	printf("\n");
	
	exit(0);
}

void sigAbortHandler(int signum) {
	if (logFile != NULL)
	{
		GetTimestamp((char *) &timestamp);
		fprintf(logFile, "%s Exited\n", timestamp);

		fflush(logFile);
		fclose(logFile);
		logFile = NULL;
	}

	fprintf(stderr, "Exited\n");

	exit(0);  
}

void sigIntHandler(int signum) {
	if (logFile != NULL)
	{
		GetTimestamp((char *) &timestamp);
		fprintf(logFile, "%s Exited\n", timestamp);

		fflush(logFile);
		fclose(logFile);
		logFile = NULL;
	}

	fprintf(stderr, "Exited\n");

	exit(0);  
}

void sigSegvHandler(int signum) {
	if (logFile != NULL)
	{
		GetTimestamp((char *) &timestamp);
		fprintf(logFile, "%s Exited\n", timestamp);

		fflush(logFile);
		fclose(logFile);
		logFile = NULL;
	}

	fprintf(stderr, "Exited\n");

	exit(0);  
}

void sigTermHandler(int signum) {

	if (logFile != NULL)
	{
		GetTimestamp((char *) &timestamp);
		fprintf(logFile, "%s Exited\n", timestamp);

		fflush(logFile);
		fclose(logFile);
		logFile = NULL;
	}

	fprintf(stderr, "Exited\n");

	exit(0);  
}

void sigStopHandler(int signum) {
	if (logFile != NULL)
	{
		GetTimestamp((char *) &timestamp);
		fprintf(logFile, "%s Exited\n", timestamp);

		fflush(logFile);
		fclose(logFile);
		logFile = NULL;
	}

	fprintf(stderr, "Exited\n");

	exit(0);  
}

void configureSignalHandlers()
{
	signal(SIGABRT, sigAbortHandler);  
	signal(SIGINT, sigIntHandler);  
	signal(SIGSEGV, sigSegvHandler);  
	signal(SIGTERM, sigTermHandler);  
	signal(SIGTSTP, sigStopHandler);
}

int entries_sort_cmp(const void *v1, const void *v2) {
	const struct ip_mr_cache *c1 = v1;
	const struct ip_mr_cache *c2 = v2;
	
	char buf1[255];
	char buf2[255];

	strcpy(buf1, c1->group);
	strcat(buf1, c1->origin);
	
	strcpy(buf2, c2->group);
	strcat(buf2, c2->origin);

	return strcmp(buf1, buf2);
}

int entries_search_cmp(const void *v1, const void *v2) {
	char *c1 = (char *) v1;
	const struct ip_mr_cache *c2 = v2;

	char buf[255];

	strcpy(buf, c2->group);
	strcat(buf, c2->origin);
	
	return strcmp(c1, buf);
}

int getData() {

	char sGroup[32];
	char sOrigin[32];
	
	int group;
	int origin;
	short  iif;
	unsigned long pkts;
	unsigned long bytes;
	unsigned long wrong;
	int oifs1;
	int oifs2;

	char *line = NULL;
	ssize_t bytes_read;
	size_t len = LINESZ;
	
	FILE *f = fopen("/proc/net/ip_mr_cache", "r");

	// skip the header
	bytes_read = getline(&line, &len, f);

	int i = 0;
	
	while ((bytes_read = getline(&line, &len, f)) != -1) 
	{
		char* newline = strchr(line, '\n');        // find the newline character
		if(newline)
			*newline = '\0';
		
		sscanf(line, "%08X %08X %3hd %8lu %8lu %8lu %2d:%3d", &group, &origin, &iif, &pkts, &bytes, &wrong, &oifs1, &oifs2);

		sprintf(sGroup,
			"%d.%d.%d.%d",
			group & 0x000000FF,
			(group & 0x0000FF00) >> 8,
			(group & 0x00FF0000) >> 16,
			(group & 0xFF000000) >> 24);

		sprintf(sOrigin,
			"%d.%d.%d.%d",
			origin & 0x000000FF,
			(origin & 0x0000FF00) >> 8,
			(origin & 0x00FF0000) >> 16,
			(origin & 0xFF000000) >> 24);
		
		strcpy(entries1[i].group, sGroup);
		strcpy(entries1[i].origin, sOrigin);
		
		entries1[i].iif = iif;
		entries1[i].pkts = pkts;
		entries1[i].bytes = bytes;
		entries1[i].wrong = wrong;
		entries1[i].oifs1 = oifs1;
		entries1[i].oifs2 = oifs2;
	
		i++;
		
		if (i >= MAX_ENTRIES)
		{
			GetTimestamp((char *) &timestamp);
			fprintf(stderr, "%s getData: too many entries: %d\n", timestamp, i);
			fclose(f);
			
			exit(0);
		}
	}

	fclose(f);

	qsort(entries1, i, sizeof(struct ip_mr_cache), entries_sort_cmp);
	
	return i;
}

void getDeltas() {

	int i;
	long millis;
	ip_mr_cache *cur_entry, *prev_entry;
	
	char searchStr[255];
	char buf[32];
	
	for (i = 0; i < size1; i++)
	{
		strcpy(searchStr, entries1[i].group);
		strcat(searchStr, entries1[i].origin);
		
		prev_entry = bsearch(searchStr, entries2, size2, sizeof(struct ip_mr_cache), entries_search_cmp);

		uint32_t group = inet_parse((char *) &entries1[i].group, 4);
		uint32_t origin = inet_parse((char *) &entries1[i].origin, 4);
			
		millis = -1;
		
		if (prev_entry == 0)
		{
			if (entries1[i].iif == iif_num)
				strcpy(iif1, iif_name);
			else
				strcpy(iif1, "unresolved");
			
			mrc_node *entry = mrc_search(group, origin);
			
			if (entry != NULL)
			{
				GetTimestamp((char *) &timestamp);
				fprintf(stderr, "%s existing entry found during add\n", timestamp);
				mrc_delete(group, origin);
			}
			
			mrc_add(group, origin, entries1[i].iif, GetMillis(), MRC_NEW);
			
			GetTimestamp((char *) &timestamp);
			fprintf(stderr, "%s new entry: %-18s %-18s %-20s %7d ms\n", timestamp, entries1[i].group, entries1[i].origin, iif1, entries1[i].iif == iif_num ? 1 : -1);
			fprintf(logFile, "%s new entry: %-18s %-18s %-20s %7d ms\n", timestamp, entries1[i].group, entries1[i].origin, iif1, entries1[i].iif == iif_num ? 1 : -1);
		}
		else {
			if (entries1[i].iif == iif_num)
				strcpy(iif1, iif_name);
			else
				strcpy(iif1, "unresolved");

			if (prev_entry->iif == iif_num)
				strcpy(iif2, iif_name);
			else
				strcpy(iif2, "unresolved");

			if (prev_entry->iif != entries1[i].iif) {
				mrc_node *mrc_entry = mrc_search(group, origin);
			
				if (mrc_entry != NULL)
				{
					millis = GetMillis() - mrc_entry->millis;
					mrc_delete(group, origin);
				}

				mrc_add(group, origin, entries1[i].iif, GetMillis(), prev_entry->iif == iif_num ? MRC_RESOLVED : MRC_UNRESOLVED);

				GetTimestamp((char *) &timestamp);
				sprintf(buf, "%s to %s", iif2, iif1);
				fprintf(stderr, "%s changed:   %-18s %-18s %-20s %7ld ms\n", timestamp, entries1[i].group, entries1[i].origin, buf, millis);
				fprintf(logFile, "%s changed:   %-18s %-18s %-20s %7ld ms\n", timestamp, entries1[i].group, entries1[i].origin, buf, millis);
			}
		}
	}
	
	for (i = 0; i < size2; i++)
	{
		strcpy(searchStr, entries2[i].group);
		strcat(searchStr, entries2[i].origin);
		
		uint32_t group = inet_parse((char *) &entries2[i].group, 4);
		uint32_t origin = inet_parse((char *) &entries2[i].origin, 4);
		
		cur_entry = bsearch(searchStr, entries1, size1, sizeof(struct ip_mr_cache), entries_search_cmp);
		
		if (cur_entry == 0)
		{
			if (entries2[i].iif == iif_num)
				strcpy(iif2, iif_name);
			else
				strcpy(iif2, "unresolved");

			mrc_node *mrc_entry = mrc_search(group, origin);
			
			if (mrc_entry != NULL)
			{
				millis = GetMillis() - mrc_entry->millis;
				mrc_delete(group, origin);
			}
			
			GetTimestamp((char *) &timestamp);
			fprintf(stderr, "%s removed:   %-18s %-18s %-20s %7ld ms\n", timestamp, entries2[i].group, entries2[i].origin, iif2, millis);
			fprintf(logFile, "%s removed:   %-18s %-18s %-20s %7ld ms\n", timestamp, entries2[i].group, entries2[i].origin, iif2, millis);
		}
	}
}

void initialReport() {

	int i;

	size1 = getData();
	
	for (i = 0; i < size1; i++) {
		
		if (entries1[i].iif == iif_num)
		{
			strcpy(iif1, iif_name);
			GetTimestamp((char *) &timestamp);
			fprintf(stderr, "%s existing:  %-18s %-18s %-20s %7d ms\n", timestamp, entries1[i].group, entries1[i].origin, iif1, -1);
			fprintf(logFile, "%s existing:  %-18s %-18s %-20s %7d ms\n", timestamp, entries1[i].group, entries1[i].origin, iif1, -1);
		}
	}
}

void watchMrCache(void *empty) {

	char filename[LINESZ];
	time_t now = time(NULL);
	
	gethostname(hostname, LINESZ);

	strftime(timestamp, LINESZ, "%m%d%Y", localtime(&now));
	sprintf(filename, "/var/log/%s-monitor_mrt-%s.log", hostname, timestamp);

	logFile = fopen(filename, "wa");
	
	GetTimestamp((char *) &timestamp);
	fprintf(logFile, "%s Started\n", timestamp);
	
	initialReport();

	size2 = 0;
	
	while (1)
	{
		size1 = getData();
	
		//		int j;
		//		for (j = 0; j < size1; j++)
		//			printf("%-20s %-20s %4ld %8ld %10ld %2d %s %s\n", entries1[j].group, entries1[j].origin, entries1[j].iif, entries1[j].pkts, entries1[j].bytes, entries1[j].wrong, entries1[j].oifs1, entries1[j].oifs2);

		if(size2 != 0)
			getDeltas();
	
		memcpy(&entries2, &entries1, sizeof(struct ip_mr_cache) * MAX_ENTRIES);
		size2 = size1;
		
		usleep(100000); // 100 milliseconds
	}
}

int getVifs()
{
	int iif_num = -1;
	
	int  ifNum;
	char ifName[32];
	unsigned long bytesIn;
	unsigned long pktsIn;
	unsigned long bytesOut;
	unsigned long pktsOut;
	int flags;
	int local;
	int remote;
	
	char *line = NULL;
	ssize_t bytes_read;
	size_t len = LINESZ;
	
	FILE *f = fopen("/proc/net/ip_mr_vif", "r");

	// skip the header
	bytes_read = getline(&line, &len, f);

	int i = 0;
	
	while ((bytes_read = getline(&line, &len, f)) != -1) 
	{
		char* newline = strchr(line, '\n');         // find the newline character
		if(newline)
			*newline = '\0';
		
		sscanf(line, "%2d %10s %8ld %7ld  %8ld %7ld %05X %08X %08X", &ifNum, (char *) &ifName, &bytesIn, &pktsIn, &bytesOut, &pktsOut, &flags, &local, &remote);

		vifs[i].ifNum = ifNum;
		strcpy(vifs[i].ifName, ifName);
		vifs[i].bytesIn = bytesIn;
		vifs[i].pktsIn = pktsIn;
		vifs[i].bytesIn = bytesIn;
		vifs[i].pktsOut = pktsOut;
		vifs[i].flags = flags;
		vifs[i].local = local;
		vifs[i].remote = remote;
		
		i++;
	}

	fclose(f);

	return i;
}

int getVif(char *vif_name)
{
	int i;
	int vif_num = -1;
	
	for (i = 0; i < num_vifs; i++)
	{
		if (strcmp(vifs[i].ifName, vif_name) == 0)
		{
			vif_num = vifs[i].ifNum;
			break;
		}		
	}
	
	return vif_num;
}

void listVifs()
{
	int i;

	printf("Valid interfaces:\n");
	
	for (i = 0; i < num_vifs; i++)
		printf("  %s\n", vifs[i].ifName);
	
	printf("\n");
}

int main(int argc, char *argv[])
{
	pthread_t threadId;
	char versionstring[100];
	int   ch;

	struct option long_options[] = {
		{ "iif", 1, 0, 'i' },
		{ "version", 0, 0, 'v' },
		{ "help", 0, 0, '?' },
		{ NULL, 0, 0, 0 }
	};
	
	prognm = progname(argv[0]);
	snprintf(versionstring, sizeof(versionstring), "%s version %s", prognm, PACKAGE_VERSION);

	num_vifs = getVifs();
	
	if (argc == 1)
	{
		printf("IIF is required\n\n");
		usage();
	}
	
	while ((ch = getopt_long(argc, argv, "i:v?", long_options, NULL)) != EOF) {

		switch (ch) {
			
		case 'i':
			iif_name = optarg;
			iif_num = getVif(iif_name);
			
			break;
			
		case 'v':
			printf("%s\n", versionstring);
			return 0;
			
		case '?':
			usage();

		default:
			usage();
		}
	}
	
	if (iif_num < 0)
	{
		printf("Cannot find iif: %s\n\n", optarg);
		listVifs();
		usage();
	}
			
	configureSignalHandlers();
	
	pthread_create(&threadId, NULL, (void *(*)(void *))&watchMrCache, NULL);
	pthread_join(threadId, NULL);
	
	fprintf(stderr, "Exiting\n");
}