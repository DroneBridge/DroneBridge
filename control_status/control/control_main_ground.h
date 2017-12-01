#ifndef MAIN_H_INCLUDED
#define MAIN_H_INCLUDED

int openSocket(char ifName[16], unsigned char dest_mac[6], char trans_mode, int bitrate_option, int frame_type,
               int rc_protocol);
void closeSocket();
int sendPacket(unsigned short contData[]);
int conf_ethernet(unsigned char dest_mac[6]);
int conf_monitor_dep(unsigned char dest_mac[6], int bitrate_option, int frame_type);

int i6S(int Joy_IF, char calibrate_comm[]);
int16_t normalize_i6S(int16_t value, int16_t adjustingValue);

int16_t normalize_xbox(int value, int adjustingValue, int axis);

#endif // MAIN_H_INCLUDED
