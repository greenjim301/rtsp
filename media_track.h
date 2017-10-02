#ifndef __MEDIA_TRACK_H__
#define __MEDIA_TRACK_H__

#include <stdint.h>
#include "r_string.h"

class media_track
{
public:
    media_track()
        : m_timestamp(0)
        , m_base_timestamp(0)
        , m_unwrapped_timestamp(0)
        , m_last_time_stamp(0)
        , playload(0)
        , channel(1)
        , sample(8000)
		, is_aac(0)
    {
    }

    int type;
    int playload;
    int sample;
    int channel;
	int is_aac;
    r_string url;
    r_string codec;
    uint32_t m_last_time_stamp;
    uint32_t m_timestamp;
    uint32_t m_base_timestamp;
    int64_t  m_unwrapped_timestamp;
};

#endif
