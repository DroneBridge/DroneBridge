#ifndef AB_PROTOCOL_H_INCLUDED
#define AB_PROTOCOL_H_INCLUDED

#define RADIOTAP_LENGTH 12
#define AB80211_LENGTH  24
#define HEADERBUF_SIZ	14
#define DATA_LENTH      22
#define ETHER_TYPE      0x88ab

#define AB_VERSION          0x01
#define AB_PORT_CONTROLLER  0x01
#define AB_PORT_TELEMETRY   0x02
#define AB_PORT_VIDEO       0x03
#define AB_DIREC_DRONE      0x01
#define AB_DIREC_GROUNDST   0x02
#define CRC_RC_TO_DRONE     0x06

struct data
{
    uint8_t bytes[DATA_LENTH];
};
struct radiotap_header
{
    uint8_t bytes[RADIOTAP_LENGTH];
};
struct ab_80211_header
{
    uint8_t frame_control_field[4];
    uint8_t dest_mac_bytes[6];
    uint8_t src_mac_bytes[6];
    uint8_t version_bytes;
    uint8_t port_bytes;
    uint8_t direction_bytes;
    uint8_t payload_length_bytes[2];
    uint8_t crc_bytes;
    uint8_t undefined[2];
};
struct crcdata
{
    uint8_t bytes[18];
};

uint8_t MSPbuf[] =
{
    0x24, 0x4d, 0x3c, 0x10, 0xc8, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xe8, 0x03, 0xe8,
    0x03, 0xe8, 0x03, 0xe8, 0x03, 0x00
};
const uint8_t radiotap_header_pre[] = {

	0x00, 0x00, // <-- radiotap version
	0x0c, 0x00, // <- radiotap header lengt
	0x04, 0x80, 0x00, 0x00, // <-- bitmap
	0x22,
	0x00,
	0x18, 0x00
};

uint8_t frame_control_pre[] =
{
    0x08, 0x00, 0x00, 0x00
};

#endif // AB_PROTOCOL_H_INCLUDED
