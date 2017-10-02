#ifndef __R_UTIL_H__
#define __R_UTIL_H__

#include "r_string.h"
#include <stdint.h>
#include <stdarg.h>
#include <debugapi.h>

enum
{
    RTSP_INIT,
    RTSP_OPTIONS_SEND,
    RTSP_DESCRIBE_SEND,
    RTSP_SETUP0_SEND,
    RTSP_SETUP1_SEND,
    RTSP_PLAY_SEND,
    RTSP_REV_STREAM,
    RTSP_TEARDOWN_SEND,
};

#define VIDEO_TYPE 0
#define AUDIO_TYPE 1

#define RTP_VERSION 2
#define RTP_FLAG_MARKER 0x2 ///< RTP marker bit was set for this packet

#define R_RB16(x)  ((((const uint8_t*)(x))[0] << 8) | ((const uint8_t*)(x))[1])
#define R_RB32(x)  ((((const uint8_t*)(x))[0] << 24) | \
    (((const uint8_t*)(x))[1] << 16) | \
    (((const uint8_t*)(x))[2] <<  8) | \
    ((const uint8_t*)(x))[3])

static const uint8_t start_sequence[] = { 0, 0, 0, 1 };

void SkipSpace(char*& p);
int SkipLine(char*& p);
int get_rtsp_addr(r_string& url, r_string&  host, int& port);
void r_log(const char* pszFormat, ...) ;

#endif