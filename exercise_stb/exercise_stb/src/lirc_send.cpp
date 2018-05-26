#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <lirc_client.h>

#include <string>

#include "defs.h"
#include "ChannelUtils.h"
#include "thread.h"
#include "lirc_send.h"

int fd;

void lirc_send_key(string key_code)
{
	vector<string> components = split(key_code, ","); 
	
	const char *key_sym = components[0].c_str();
	int scan_code = (int) strtol(components[1].c_str(), 0, 16);
	
	fd = lirc_get_local_socket(NULL, 0);
	if (fd < 0) {
		printf("Error during lirc_get_local_socket\n");
	}
	
	if (lirc_simulate(fd, REMOTE_NAME, key_sym, scan_code, 0) == -1)
	{
		printf("Error during lirc_simulate\n");
	}

	if (lirc_simulate(fd, REMOTE_NAME, key_sym, scan_code, 1) == -1)
	{
		printf("Error during lirc_simulate\n");
	}

	close(fd);
}	

void lirc_change_channel(int channel_num)
{
	char s_channel_num[10];
	
	snprintf(s_channel_num, sizeof(s_channel_num), "%d", channel_num);
	
	for (int i = 0; i < strlen(s_channel_num); i++)
	{
		char c = s_channel_num[i];
		
		switch (c) {
		case '1':
			lirc_send_key(string(KEY_1));
			break;
			
		case '2':
			lirc_send_key(string(KEY_2));
			break;
			
		case '3':
			lirc_send_key(string(KEY_3));
			break;
			
		case '4':
			lirc_send_key(string(KEY_4));
			break;
			
		case '5':
			lirc_send_key(string(KEY_5));
			break;
			
		case '6':
			lirc_send_key(string(KEY_6));
			break;
			
		case '7':
			lirc_send_key(string(KEY_7));
			break;
			
		case '8':
			lirc_send_key(string(KEY_8));
			break;

		case '9':
			lirc_send_key(string(KEY_9));
			break;

		case '0':
			lirc_send_key(string(KEY_0));
			break;
		}
		
		cCondWait::SleepMs(100);
	}
	
	lirc_send_key(string(KEY_OK));
}

void lirc_channel_up()
{
	fd = lirc_get_local_socket(NULL, 0);
	if (fd < 0) {
		printf("Error during lirc_get_local_socket\n");
	}
	
	if (lirc_simulate(fd, REMOTE_NAME, CHANNEL_UP, 0x13BC, 0) == -1)
	{
		printf("Error during lirc_simulate\n");
	}

	if (lirc_simulate(fd, REMOTE_NAME, CHANNEL_UP, 0x13BC, 1) == -1)
	{
		printf("Error during lirc_simulate\n");
	}

	close(fd);
}

void lirc_channel_down()
{
	fd = lirc_get_local_socket(NULL, 0);
	if (fd < 0) {
		printf("Error during lirc_get_local_socket\n");
	}
	
	if (lirc_simulate(fd, REMOTE_NAME, CHANNEL_DOWN, 0x1391, 0) == -1)
	{
		printf("Error during lirc_simulate\n");
	}

	if (lirc_simulate(fd, REMOTE_NAME, CHANNEL_DOWN, 0x1391, 1) == -1)
	{
		printf("Error during lirc_simulate\n");
	}
	
	close(fd);
}
