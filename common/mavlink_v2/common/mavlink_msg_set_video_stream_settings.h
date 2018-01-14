#pragma once
// MESSAGE SET_VIDEO_STREAM_SETTINGS PACKING

#define MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS 270

MAVPACKED(
typedef struct __mavlink_set_video_stream_settings_t {
 float framerate; /*< Frames per second (set to -1 for highest framerate possible)*/
 uint32_t bitrate; /*< Bit rate in bits per second (set to -1 for auto)*/
 uint16_t resolution_h; /*< Resolution horizontal in pixels (set to -1 for highest resolution possible)*/
 uint16_t resolution_v; /*< Resolution vertical in pixels (set to -1 for highest resolution possible)*/
 uint16_t rotation; /*< Video image rotation clockwise (0-359 degrees)*/
 uint8_t target_system; /*< system ID of the target*/
 uint8_t target_component; /*< component ID of the target*/
 uint8_t camera_id; /*< Camera ID (1 for first, 2 for second, etc.)*/
 char uri[230]; /*< Video stream URI*/
}) mavlink_set_video_stream_settings_t;

#define MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_LEN 247
#define MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_MIN_LEN 247
#define MAVLINK_MSG_ID_270_LEN 247
#define MAVLINK_MSG_ID_270_MIN_LEN 247

#define MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_CRC 232
#define MAVLINK_MSG_ID_270_CRC 232

#define MAVLINK_MSG_SET_VIDEO_STREAM_SETTINGS_FIELD_URI_LEN 230

#if MAVLINK_COMMAND_24BIT
#define MAVLINK_MESSAGE_INFO_SET_VIDEO_STREAM_SETTINGS { \
    270, \
    "SET_VIDEO_STREAM_SETTINGS", \
    9, \
    {  { "target_system", NULL, MAVLINK_TYPE_UINT8_T, 0, 14, offsetof(mavlink_set_video_stream_settings_t, target_system) }, \
         { "target_component", NULL, MAVLINK_TYPE_UINT8_T, 0, 15, offsetof(mavlink_set_video_stream_settings_t, target_component) }, \
         { "camera_id", NULL, MAVLINK_TYPE_UINT8_T, 0, 16, offsetof(mavlink_set_video_stream_settings_t, camera_id) }, \
         { "framerate", NULL, MAVLINK_TYPE_FLOAT, 0, 0, offsetof(mavlink_set_video_stream_settings_t, framerate) }, \
         { "resolution_h", NULL, MAVLINK_TYPE_UINT16_T, 0, 8, offsetof(mavlink_set_video_stream_settings_t, resolution_h) }, \
         { "resolution_v", NULL, MAVLINK_TYPE_UINT16_T, 0, 10, offsetof(mavlink_set_video_stream_settings_t, resolution_v) }, \
         { "bitrate", NULL, MAVLINK_TYPE_UINT32_T, 0, 4, offsetof(mavlink_set_video_stream_settings_t, bitrate) }, \
         { "rotation", NULL, MAVLINK_TYPE_UINT16_T, 0, 12, offsetof(mavlink_set_video_stream_settings_t, rotation) }, \
         { "uri", NULL, MAVLINK_TYPE_CHAR, 230, 17, offsetof(mavlink_set_video_stream_settings_t, uri) }, \
         } \
}
#else
#define MAVLINK_MESSAGE_INFO_SET_VIDEO_STREAM_SETTINGS { \
    "SET_VIDEO_STREAM_SETTINGS", \
    9, \
    {  { "target_system", NULL, MAVLINK_TYPE_UINT8_T, 0, 14, offsetof(mavlink_set_video_stream_settings_t, target_system) }, \
         { "target_component", NULL, MAVLINK_TYPE_UINT8_T, 0, 15, offsetof(mavlink_set_video_stream_settings_t, target_component) }, \
         { "camera_id", NULL, MAVLINK_TYPE_UINT8_T, 0, 16, offsetof(mavlink_set_video_stream_settings_t, camera_id) }, \
         { "framerate", NULL, MAVLINK_TYPE_FLOAT, 0, 0, offsetof(mavlink_set_video_stream_settings_t, framerate) }, \
         { "resolution_h", NULL, MAVLINK_TYPE_UINT16_T, 0, 8, offsetof(mavlink_set_video_stream_settings_t, resolution_h) }, \
         { "resolution_v", NULL, MAVLINK_TYPE_UINT16_T, 0, 10, offsetof(mavlink_set_video_stream_settings_t, resolution_v) }, \
         { "bitrate", NULL, MAVLINK_TYPE_UINT32_T, 0, 4, offsetof(mavlink_set_video_stream_settings_t, bitrate) }, \
         { "rotation", NULL, MAVLINK_TYPE_UINT16_T, 0, 12, offsetof(mavlink_set_video_stream_settings_t, rotation) }, \
         { "uri", NULL, MAVLINK_TYPE_CHAR, 230, 17, offsetof(mavlink_set_video_stream_settings_t, uri) }, \
         } \
}
#endif

/**
 * @brief Pack a set_video_stream_settings message
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param msg The MAVLink message to compress the data into
 *
 * @param target_system system ID of the target
 * @param target_component component ID of the target
 * @param camera_id Camera ID (1 for first, 2 for second, etc.)
 * @param framerate Frames per second (set to -1 for highest framerate possible)
 * @param resolution_h Resolution horizontal in pixels (set to -1 for highest resolution possible)
 * @param resolution_v Resolution vertical in pixels (set to -1 for highest resolution possible)
 * @param bitrate Bit rate in bits per second (set to -1 for auto)
 * @param rotation Video image rotation clockwise (0-359 degrees)
 * @param uri Video stream URI
 * @return length of the message in bytes (excluding serial stream start sign)
 */
static inline uint16_t mavlink_msg_set_video_stream_settings_pack(uint8_t system_id, uint8_t component_id, mavlink_message_t* msg,
                               uint8_t target_system, uint8_t target_component, uint8_t camera_id, float framerate, uint16_t resolution_h, uint16_t resolution_v, uint32_t bitrate, uint16_t rotation, const char *uri)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char buf[MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_LEN];
    _mav_put_float(buf, 0, framerate);
    _mav_put_uint32_t(buf, 4, bitrate);
    _mav_put_uint16_t(buf, 8, resolution_h);
    _mav_put_uint16_t(buf, 10, resolution_v);
    _mav_put_uint16_t(buf, 12, rotation);
    _mav_put_uint8_t(buf, 14, target_system);
    _mav_put_uint8_t(buf, 15, target_component);
    _mav_put_uint8_t(buf, 16, camera_id);
    _mav_put_char_array(buf, 17, uri, 230);
        memcpy(_MAV_PAYLOAD_NON_CONST(msg), buf, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_LEN);
#else
    mavlink_set_video_stream_settings_t packet;
    packet.framerate = framerate;
    packet.bitrate = bitrate;
    packet.resolution_h = resolution_h;
    packet.resolution_v = resolution_v;
    packet.rotation = rotation;
    packet.target_system = target_system;
    packet.target_component = target_component;
    packet.camera_id = camera_id;
    mav_array_memcpy(packet.uri, uri, sizeof(char)*230);
        memcpy(_MAV_PAYLOAD_NON_CONST(msg), &packet, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_LEN);
#endif

    msg->msgid = MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS;
    return mavlink_finalize_message(msg, system_id, component_id, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_MIN_LEN, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_LEN, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_CRC);
}

/**
 * @brief Pack a set_video_stream_settings message on a channel
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param chan The MAVLink channel this message will be sent over
 * @param msg The MAVLink message to compress the data into
 * @param target_system system ID of the target
 * @param target_component component ID of the target
 * @param camera_id Camera ID (1 for first, 2 for second, etc.)
 * @param framerate Frames per second (set to -1 for highest framerate possible)
 * @param resolution_h Resolution horizontal in pixels (set to -1 for highest resolution possible)
 * @param resolution_v Resolution vertical in pixels (set to -1 for highest resolution possible)
 * @param bitrate Bit rate in bits per second (set to -1 for auto)
 * @param rotation Video image rotation clockwise (0-359 degrees)
 * @param uri Video stream URI
 * @return length of the message in bytes (excluding serial stream start sign)
 */
static inline uint16_t mavlink_msg_set_video_stream_settings_pack_chan(uint8_t system_id, uint8_t component_id, uint8_t chan,
                               mavlink_message_t* msg,
                                   uint8_t target_system,uint8_t target_component,uint8_t camera_id,float framerate,uint16_t resolution_h,uint16_t resolution_v,uint32_t bitrate,uint16_t rotation,const char *uri)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char buf[MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_LEN];
    _mav_put_float(buf, 0, framerate);
    _mav_put_uint32_t(buf, 4, bitrate);
    _mav_put_uint16_t(buf, 8, resolution_h);
    _mav_put_uint16_t(buf, 10, resolution_v);
    _mav_put_uint16_t(buf, 12, rotation);
    _mav_put_uint8_t(buf, 14, target_system);
    _mav_put_uint8_t(buf, 15, target_component);
    _mav_put_uint8_t(buf, 16, camera_id);
    _mav_put_char_array(buf, 17, uri, 230);
        memcpy(_MAV_PAYLOAD_NON_CONST(msg), buf, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_LEN);
#else
    mavlink_set_video_stream_settings_t packet;
    packet.framerate = framerate;
    packet.bitrate = bitrate;
    packet.resolution_h = resolution_h;
    packet.resolution_v = resolution_v;
    packet.rotation = rotation;
    packet.target_system = target_system;
    packet.target_component = target_component;
    packet.camera_id = camera_id;
    mav_array_memcpy(packet.uri, uri, sizeof(char)*230);
        memcpy(_MAV_PAYLOAD_NON_CONST(msg), &packet, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_LEN);
#endif

    msg->msgid = MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS;
    return mavlink_finalize_message_chan(msg, system_id, component_id, chan, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_MIN_LEN, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_LEN, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_CRC);
}

/**
 * @brief Encode a set_video_stream_settings struct
 *
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param msg The MAVLink message to compress the data into
 * @param set_video_stream_settings C-struct to read the message contents from
 */
static inline uint16_t mavlink_msg_set_video_stream_settings_encode(uint8_t system_id, uint8_t component_id, mavlink_message_t* msg, const mavlink_set_video_stream_settings_t* set_video_stream_settings)
{
    return mavlink_msg_set_video_stream_settings_pack(system_id, component_id, msg, set_video_stream_settings->target_system, set_video_stream_settings->target_component, set_video_stream_settings->camera_id, set_video_stream_settings->framerate, set_video_stream_settings->resolution_h, set_video_stream_settings->resolution_v, set_video_stream_settings->bitrate, set_video_stream_settings->rotation, set_video_stream_settings->uri);
}

/**
 * @brief Encode a set_video_stream_settings struct on a channel
 *
 * @param system_id ID of this system
 * @param component_id ID of this component (e.g. 200 for IMU)
 * @param chan The MAVLink channel this message will be sent over
 * @param msg The MAVLink message to compress the data into
 * @param set_video_stream_settings C-struct to read the message contents from
 */
static inline uint16_t mavlink_msg_set_video_stream_settings_encode_chan(uint8_t system_id, uint8_t component_id, uint8_t chan, mavlink_message_t* msg, const mavlink_set_video_stream_settings_t* set_video_stream_settings)
{
    return mavlink_msg_set_video_stream_settings_pack_chan(system_id, component_id, chan, msg, set_video_stream_settings->target_system, set_video_stream_settings->target_component, set_video_stream_settings->camera_id, set_video_stream_settings->framerate, set_video_stream_settings->resolution_h, set_video_stream_settings->resolution_v, set_video_stream_settings->bitrate, set_video_stream_settings->rotation, set_video_stream_settings->uri);
}

/**
 * @brief Send a set_video_stream_settings message
 * @param chan MAVLink channel to send the message
 *
 * @param target_system system ID of the target
 * @param target_component component ID of the target
 * @param camera_id Camera ID (1 for first, 2 for second, etc.)
 * @param framerate Frames per second (set to -1 for highest framerate possible)
 * @param resolution_h Resolution horizontal in pixels (set to -1 for highest resolution possible)
 * @param resolution_v Resolution vertical in pixels (set to -1 for highest resolution possible)
 * @param bitrate Bit rate in bits per second (set to -1 for auto)
 * @param rotation Video image rotation clockwise (0-359 degrees)
 * @param uri Video stream URI
 */
#ifdef MAVLINK_USE_CONVENIENCE_FUNCTIONS

static inline void mavlink_msg_set_video_stream_settings_send(mavlink_channel_t chan, uint8_t target_system, uint8_t target_component, uint8_t camera_id, float framerate, uint16_t resolution_h, uint16_t resolution_v, uint32_t bitrate, uint16_t rotation, const char *uri)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char buf[MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_LEN];
    _mav_put_float(buf, 0, framerate);
    _mav_put_uint32_t(buf, 4, bitrate);
    _mav_put_uint16_t(buf, 8, resolution_h);
    _mav_put_uint16_t(buf, 10, resolution_v);
    _mav_put_uint16_t(buf, 12, rotation);
    _mav_put_uint8_t(buf, 14, target_system);
    _mav_put_uint8_t(buf, 15, target_component);
    _mav_put_uint8_t(buf, 16, camera_id);
    _mav_put_char_array(buf, 17, uri, 230);
    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS, buf, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_MIN_LEN, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_LEN, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_CRC);
#else
    mavlink_set_video_stream_settings_t packet;
    packet.framerate = framerate;
    packet.bitrate = bitrate;
    packet.resolution_h = resolution_h;
    packet.resolution_v = resolution_v;
    packet.rotation = rotation;
    packet.target_system = target_system;
    packet.target_component = target_component;
    packet.camera_id = camera_id;
    mav_array_memcpy(packet.uri, uri, sizeof(char)*230);
    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS, (const char *)&packet, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_MIN_LEN, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_LEN, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_CRC);
#endif
}

/**
 * @brief Send a set_video_stream_settings message
 * @param chan MAVLink channel to send the message
 * @param struct The MAVLink struct to serialize
 */
static inline void mavlink_msg_set_video_stream_settings_send_struct(mavlink_channel_t chan, const mavlink_set_video_stream_settings_t* set_video_stream_settings)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    mavlink_msg_set_video_stream_settings_send(chan, set_video_stream_settings->target_system, set_video_stream_settings->target_component, set_video_stream_settings->camera_id, set_video_stream_settings->framerate, set_video_stream_settings->resolution_h, set_video_stream_settings->resolution_v, set_video_stream_settings->bitrate, set_video_stream_settings->rotation, set_video_stream_settings->uri);
#else
    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS, (const char *)set_video_stream_settings, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_MIN_LEN, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_LEN, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_CRC);
#endif
}

#if MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_LEN <= MAVLINK_MAX_PAYLOAD_LEN
/*
  This varient of _send() can be used to save stack space by re-using
  memory from the receive buffer.  The caller provides a
  mavlink_message_t which is the size of a full mavlink message. This
  is usually the receive buffer for the channel, and allows a reply to an
  incoming message with minimum stack space usage.
 */
static inline void mavlink_msg_set_video_stream_settings_send_buf(mavlink_message_t *msgbuf, mavlink_channel_t chan,  uint8_t target_system, uint8_t target_component, uint8_t camera_id, float framerate, uint16_t resolution_h, uint16_t resolution_v, uint32_t bitrate, uint16_t rotation, const char *uri)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    char *buf = (char *)msgbuf;
    _mav_put_float(buf, 0, framerate);
    _mav_put_uint32_t(buf, 4, bitrate);
    _mav_put_uint16_t(buf, 8, resolution_h);
    _mav_put_uint16_t(buf, 10, resolution_v);
    _mav_put_uint16_t(buf, 12, rotation);
    _mav_put_uint8_t(buf, 14, target_system);
    _mav_put_uint8_t(buf, 15, target_component);
    _mav_put_uint8_t(buf, 16, camera_id);
    _mav_put_char_array(buf, 17, uri, 230);
    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS, buf, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_MIN_LEN, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_LEN, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_CRC);
#else
    mavlink_set_video_stream_settings_t *packet = (mavlink_set_video_stream_settings_t *)msgbuf;
    packet->framerate = framerate;
    packet->bitrate = bitrate;
    packet->resolution_h = resolution_h;
    packet->resolution_v = resolution_v;
    packet->rotation = rotation;
    packet->target_system = target_system;
    packet->target_component = target_component;
    packet->camera_id = camera_id;
    mav_array_memcpy(packet->uri, uri, sizeof(char)*230);
    _mav_finalize_message_chan_send(chan, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS, (const char *)packet, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_MIN_LEN, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_LEN, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_CRC);
#endif
}
#endif

#endif

// MESSAGE SET_VIDEO_STREAM_SETTINGS UNPACKING


/**
 * @brief Get field target_system from set_video_stream_settings message
 *
 * @return system ID of the target
 */
static inline uint8_t mavlink_msg_set_video_stream_settings_get_target_system(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint8_t(msg,  14);
}

/**
 * @brief Get field target_component from set_video_stream_settings message
 *
 * @return component ID of the target
 */
static inline uint8_t mavlink_msg_set_video_stream_settings_get_target_component(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint8_t(msg,  15);
}

/**
 * @brief Get field camera_id from set_video_stream_settings message
 *
 * @return Camera ID (1 for first, 2 for second, etc.)
 */
static inline uint8_t mavlink_msg_set_video_stream_settings_get_camera_id(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint8_t(msg,  16);
}

/**
 * @brief Get field framerate from set_video_stream_settings message
 *
 * @return Frames per second (set to -1 for highest framerate possible)
 */
static inline float mavlink_msg_set_video_stream_settings_get_framerate(const mavlink_message_t* msg)
{
    return _MAV_RETURN_float(msg,  0);
}

/**
 * @brief Get field resolution_h from set_video_stream_settings message
 *
 * @return Resolution horizontal in pixels (set to -1 for highest resolution possible)
 */
static inline uint16_t mavlink_msg_set_video_stream_settings_get_resolution_h(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint16_t(msg,  8);
}

/**
 * @brief Get field resolution_v from set_video_stream_settings message
 *
 * @return Resolution vertical in pixels (set to -1 for highest resolution possible)
 */
static inline uint16_t mavlink_msg_set_video_stream_settings_get_resolution_v(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint16_t(msg,  10);
}

/**
 * @brief Get field bitrate from set_video_stream_settings message
 *
 * @return Bit rate in bits per second (set to -1 for auto)
 */
static inline uint32_t mavlink_msg_set_video_stream_settings_get_bitrate(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint32_t(msg,  4);
}

/**
 * @brief Get field rotation from set_video_stream_settings message
 *
 * @return Video image rotation clockwise (0-359 degrees)
 */
static inline uint16_t mavlink_msg_set_video_stream_settings_get_rotation(const mavlink_message_t* msg)
{
    return _MAV_RETURN_uint16_t(msg,  12);
}

/**
 * @brief Get field uri from set_video_stream_settings message
 *
 * @return Video stream URI
 */
static inline uint16_t mavlink_msg_set_video_stream_settings_get_uri(const mavlink_message_t* msg, char *uri)
{
    return _MAV_RETURN_char_array(msg, uri, 230,  17);
}

/**
 * @brief Decode a set_video_stream_settings message into a struct
 *
 * @param msg The message to decode
 * @param set_video_stream_settings C-struct to decode the message contents into
 */
static inline void mavlink_msg_set_video_stream_settings_decode(const mavlink_message_t* msg, mavlink_set_video_stream_settings_t* set_video_stream_settings)
{
#if MAVLINK_NEED_BYTE_SWAP || !MAVLINK_ALIGNED_FIELDS
    set_video_stream_settings->framerate = mavlink_msg_set_video_stream_settings_get_framerate(msg);
    set_video_stream_settings->bitrate = mavlink_msg_set_video_stream_settings_get_bitrate(msg);
    set_video_stream_settings->resolution_h = mavlink_msg_set_video_stream_settings_get_resolution_h(msg);
    set_video_stream_settings->resolution_v = mavlink_msg_set_video_stream_settings_get_resolution_v(msg);
    set_video_stream_settings->rotation = mavlink_msg_set_video_stream_settings_get_rotation(msg);
    set_video_stream_settings->target_system = mavlink_msg_set_video_stream_settings_get_target_system(msg);
    set_video_stream_settings->target_component = mavlink_msg_set_video_stream_settings_get_target_component(msg);
    set_video_stream_settings->camera_id = mavlink_msg_set_video_stream_settings_get_camera_id(msg);
    mavlink_msg_set_video_stream_settings_get_uri(msg, set_video_stream_settings->uri);
#else
        uint8_t len = msg->len < MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_LEN? msg->len : MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_LEN;
        memset(set_video_stream_settings, 0, MAVLINK_MSG_ID_SET_VIDEO_STREAM_SETTINGS_LEN);
    memcpy(set_video_stream_settings, _MAV_PAYLOAD(msg), len);
#endif
}
