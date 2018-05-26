#ifndef LIRC_SEND_H
#define LIRC_SEND_H

#include <string>

using namespace std;

#ifdef PI

#define REMOTE_NAME "mag256"
#define CHANNEL_UP "KEY_CHANNEL_UP"
#define CHANNEL_DOWN "KEY_CHANNEL_DOWN"

#else
#define REMOTE_NAME "TT"
#define CHANNEL_UP "ch+"
#define CHANNEL_DOWN "ch-"
#define KEY_1 "1,0x1381"
#define KEY_2 "2,0x1382"
#define KEY_3 "3,0x1383"
#define KEY_4 "4,0x1384"
#define KEY_5 "5,0x1385"
#define KEY_6 "6,0x1386"
#define KEY_7 "7,0x1387"
#define KEY_8 "8,0x1388"
#define KEY_9 "9,0x1389"
#define KEY_0 "0,0x1380"
#define KEY_OK "ok,0x13AC"
#define KEY_TV "tv,0x13B5"
#endifvoid lirc_send_key(string key_code);void lirc_channel_up();
void lirc_channel_down();
void lirc_change_channel(int channel_num);
#endif