#ifndef DB_PROTOCOL_H_INCLUDED
#define DB_PROTOCOL_H_INCLUDED

#define RADIOTAP_LENGTH 12
#define AB80211_LENGTH  24
#define HEADERBUF_SIZ   14
#define DATA_LENTH      34      // size of MSP v1
#define DATA_LENTH_V2   37      // size of MSP v2 frame
#define DATA_UNI_LENGTH 2048	// max payload possible
#define ETHER_TYPE      0x88ab

#define DB_VERSION          0x01

#define DB_PORT_CONTROLLER  0x01
#define DB_PORT_TELEMETRY   0x02
#define DB_PORT_VIDEO       0x03
#define DB_PORT_COMM		0x04
#define DB_PORT_STATUS		0x05
#define DB_PORT_DBPROXY		0x06

#define DB_DIREC_DRONE      0x01
#define DB_DIREC_GROUND   	0x02

#define CRC_RC_TO_DRONE     0x06

struct data {
	uint8_t bytes[DATA_LENTH];
};
struct datav2 {
	uint8_t bytes[DATA_LENTH_V2];
};
struct data_uni {
	uint8_t bytes[DATA_UNI_LENGTH];
};
struct radiotap_header {
	uint8_t bytes[RADIOTAP_LENGTH];
};
struct db_80211_header {
	uint8_t frame_control_field[4];
	uint8_t odd;
	uint8_t direction_dstmac;
	uint8_t comm_id[4];
	uint8_t src_mac_bytes[6];
	uint8_t version_bytes;
	uint8_t port_bytes;
	uint8_t direction_bytes;
	uint8_t payload_length_bytes[2];
	uint8_t crc_bytes;
	uint8_t undefined[2];
};

uint8_t radiotap_header_pre[] = {

		0x00, 0x00, // <-- radiotap version
		0x0c, 0x00, // <- radiotap header length
		0x04, 0x80, 0x00, 0x00, // <-- bitmap
		0x24,       // data rate (will be overwritten)
		0x00,
		0x18, 0x00
};

const uint8_t frame_control_pre_data[] =
		{
				0x08, 0x00, 0x00, 0x00
		};
const uint8_t frame_control_pre_beacon[] =
		{
				0x80, 0x00, 0x00, 0x00
		};

#endif // DB_PROTOCOL_H_INCLUDED
