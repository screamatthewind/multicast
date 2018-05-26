#include "utils.h"

/*
 * Convert an IP address in uint32_t (network) format into a printable string.
 */
char *inet_fmt(uint32_t addr, char *s, size_t len)
{
	uint8_t *a;

	a = (uint8_t *)&addr;
	snprintf(s, len, "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);

	return s;
}

/*
 * Convert the printable string representation of an IP address into the
 * uint32_t (network) format.  Return 0xffffffff on error.  (To detect the
 * legal address with that value, you must explicitly compare the string
 * with "255.255.255.255".)
 * The return value is in network order.
 */
uint32_t inet_parse(char *s, int n)
{
	uint32_t a = 0;
	unsigned int a0 = 0, a1 = 0, a2 = 0, a3 = 0;
	int i;
	char c;

	i = sscanf(s, "%u.%u.%u.%u%c", &a0, &a1, &a2, &a3, &c);
	if (i < n || i > 4 || a0 > 255 || a1 > 255 || a2 > 255 || a3 > 255)
		return 0xffffffff;

	((uint8_t *)&a)[0] = a0;
	((uint8_t *)&a)[1] = a1;
	((uint8_t *)&a)[2] = a2;
	((uint8_t *)&a)[3] = a3;

	return a;
}

void GetTimestamp(char *currentTime)
{
	struct timeval curTime;
	gettimeofday(&curTime, NULL);

	int milli = curTime.tv_usec / 1000;
	int usec = curTime.tv_usec;

	char buffer[80];
	strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", localtime(&curTime.tv_sec));
	sprintf(currentTime, "%s.%-6d", buffer, usec);
}

int GetMillis()
{
	struct timeval curTime;
	gettimeofday(&curTime, NULL);

	long millis = (((curTime.tv_sec) * 1000) + (curTime.tv_usec / 1000.0)) + 0.5;
	
	return millis;
}