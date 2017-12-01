//
// Created by Wolfgang Christl on 30.11.17.
// This file is part of DroneBridge
// https://github.com/seeul8er/DroneBridge
//

const uint8_t radiotap_header_pre[] = {

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