#ifndef __REV_THREAD_H__
#define __REV_THREAD_H__

#include "r_thread.h"
#include "r_sock.h"
#include "dec_thread.h"
#include "vren_thread.h"
#include "aren_thread.h"
#include "media_track.h"
#include <stdint.h>

struct AVPacket;
class media_track;
class dec_thread;

class rev_thread : public r_thread
{
public:
    rev_thread();
    ~rev_thread();

    int Run();
	inline void SetParam(HWND& hwnd, r_string& url)
	{
		m_hwhd = hwnd;
		m_url = url;
	}

	vren_thread m_vren_thread;

private:
    int ParseRtpRtcp(int8_t channel, uint8_t* buf, int len);
    void finalize_packet(media_track* tr, AVPacket* pkt);
	int ParseRespone();
	int Connect();
	int SendRtspCmd(const char* cmd);
	int ParseHead(char* buf, int& con_len);
	int ParseSdp(char* buf, int size);
	int SendRtspSetup(r_string& setup_url, const char* transport);
	int StartPlay();
	int StopPlay();


private:
	HWND m_hwhd;
	r_string m_url;

    char* m_rev_buf;
    int m_rev_len;

    char* m_video_buf;
    int m_video_size;

	r_sock m_sock;
	int m_seq;
	char m_send_buf[1024];

	int m_state;
	r_string m_content_base;
	r_string m_klive_method;
	media_track m_track[2];
	r_string m_session;
	int m_time_out;
	r_string m_sps;

	dec_thread m_dec_thread;
	aren_thread m_aren_thread; 
	int m_last_tt;
};

#endif
