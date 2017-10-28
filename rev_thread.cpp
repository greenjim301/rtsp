#include "rev_thread.h"
#include "r_util.h"
#include "media_track.h"
#include "dec_thread.h"
#include <stdio.h>

extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavutil/base64.h"
};

rev_thread::rev_thread()
    : m_rev_len(0)
    , m_video_size(0)
	, m_seq(0)
	, m_state(RTSP_INIT)
	, m_hwhd(NULL)
	, m_time_out(0)
	, m_last_tt(0)
{
    m_video_buf = (char*)malloc(5 * 1024 * 1024);
    m_rev_buf = (char*)malloc(10 * 1024);
}

rev_thread::~rev_thread()
{
    free(m_video_buf);
    free(m_rev_buf);
}

int rev_thread::Connect()
{
	r_string host;
	int port;

	if(get_rtsp_addr(m_url, host, port))
	{
		return -1;
	}

	return m_sock.Connect(host, port);
}

int rev_thread::SendRtspCmd(const char* cmd)
{
	int off = 0;
	int ret = sprintf(m_send_buf, "%s %s RTSP/1.0\r\n", cmd, m_url.c_str());
	off += ret;
	ret = sprintf(m_send_buf + off, "CSeq: %d\r\n", ++m_seq);
	off += ret;

	if(m_session.Size())
	{
		ret = sprintf(m_send_buf + off, "Session: %s\r\n", m_session.c_str());
		off += ret;
	}
	ret = sprintf(m_send_buf + off, "User-Agent: lrtsp v0.1\r\n");
	off += ret;
	ret = sprintf(m_send_buf + off, "\r\n");
	off += ret;

	m_send_buf[off] = '\0';
	//r_log("%s\n", m_send_buf);

	ret = m_sock.Send(m_send_buf, off);

	if(ret != off)
	{
		r_log("send %s ret:%d, err:%d\n", cmd, ret, errno);
		return -1;
	}

	return 0;
}


int rev_thread::Run()
{
	if(this->Connect())
	{
		return -1;
	}

	if(this->SendRtspCmd("OPTIONS"))
	{
		return -1;
	}

	m_state = RTSP_OPTIONS_SEND;

	int ret;
	while(true)
	{
		if(m_exit_flag && m_state != RTSP_TEARDOWN_SEND)
		{
			this->StopPlay();
		}

		if(m_state == RTSP_REV_STREAM)
		{
			SYSTEMTIME ft;
			GetSystemTime(&ft);

			if(ft.wMinute != m_last_tt)
			{
				r_log("send keep\n");
				this->SendRtspCmd(m_klive_method.c_str());
				m_last_tt = ft.wMinute;
			}
		}		

		m_rev_len = m_sock.Recv(m_rev_buf, 1500);
		if (m_rev_len > 0)
		{
			m_rev_buf[m_rev_len] = '\0';
			ret = this->ParseRespone();
			if(-1 == ret)
			{
				return -1;
			}
			else if(1 == ret)
			{
				break;
			}
		}
		else if(m_rev_len < 0)
		{
			r_log("run rev err:%d\n", errno);
			return -1;
		}
	}

	return 0;
}


int rev_thread::ParseRtpRtcp(int8_t channel, uint8_t* buf, int len)
{
    if (channel == 0 || channel == 2) {
        if (len < 12)
            return -1;
        
        if ((buf[0] & 0xc0) != (RTP_VERSION << 6))
            return -1;
        
        unsigned int ssrc;
        int payload_type, seq, flags = 0;
        int ext, csrc;
        uint32_t timestamp;
        
        csrc         = buf[0] & 0x0f;
        ext          = buf[0] & 0x10;
        payload_type = buf[1] & 0x7f;
        if (buf[1] & 0x80)
            flags |= RTP_FLAG_MARKER;
        seq       = R_RB16(buf + 2);
        timestamp = R_RB32(buf + 4);
        ssrc      = R_RB32(buf + 8);

        if (buf[0] & 0x20) {
            int padding = buf[len - 1];
            if (len >= 12 + padding)
                len -= padding;
        }
        
        len   -= 12;
        buf   += 12;
        
        len   -= 4 * csrc;
        buf   += 4 * csrc;
        if (len < 0)
            return -1;
        
        if (ext) {
            if (len < 4)
                return -1;
            /* calculate the header extension length (stored as number
             * of 32-bit words) */
            ext = (R_RB16(buf + 2) + 1) << 2;
            
            if (len < ext)
                return -1;
            // skip past RTP header extension
            len -= ext;
            buf += ext;
        }

        if(channel == 0)
        {
            if (payload_type != m_track[0].playload) 
            {
                return -1;
            }

            if(m_track[0].m_last_time_stamp == 0)
            {
                m_track[0].m_last_time_stamp = timestamp;
            }

            if(timestamp != m_track[0].m_last_time_stamp)
            {
                AVPacket* pkt = av_packet_alloc();

                if (av_new_packet(pkt, m_video_size) < 0)
                {
                    av_packet_free(&pkt);
                    return -1;
                }

                memcpy(pkt->data, m_video_buf, m_video_size);                    
                this->finalize_packet(&m_track[0], pkt);
                m_dec_thread.Enque(pkt);
                m_video_size = 0;
                m_track[0].m_last_time_stamp = timestamp;
            }
        
            uint8_t nal;
            uint8_t type;
            int result = 0;
        
            if (!len) {
                r_log("Empty H.264 RTP packet\n");
                return -1;
            }
            nal  = buf[0];
            type = nal & 0x1f;
						      
            /* Simplify the case (these are all the NAL types used internally by
             * the H.264 codec). */
            if (type >= 1 && type <= 23)
                type = 1;
            switch (type) {
                case 0:                    // undefined, but pass them through
                case 1:
                    {
                        memcpy(m_video_buf + m_video_size, start_sequence, sizeof(start_sequence));
                        m_video_size += sizeof(start_sequence);
                        memcpy(m_video_buf + m_video_size, buf, len);
                        m_video_size +=  len;
                    }
                    break;

                
                case 24:                   // STAP-A (one packet, multiple nals)
                    // consume the STAP-A NAL
                    r_log("RTP H.264 NAL unit type %d\n", type);
                    result = -1;
                    break;
                
                case 25:                   // STAP-B
                case 26:                   // MTAP-16
                case 27:                   // MTAP-24
                case 29:                   // FU-B
                    r_log("RTP H.264 NAL unit type %d\n", type);
                    result = -1;
                    break;
                
                case 28:                   // FU-A (fragmented nal)
                    {
                        uint8_t fu_indicator, fu_header, start_bit, nal_type, ori_nal;

                        fu_indicator = buf[0];
                        fu_header    = buf[1];
                        start_bit    = fu_header >> 7;
                        nal_type     = fu_header & 0x1f;
                        ori_nal      = fu_indicator & 0xe0 | nal_type;

                        // skip the fu_indicator and fu_header
                        buf += 2;
                        len -= 2;

                        if (start_bit) {
                            memcpy(m_video_buf + m_video_size, start_sequence, sizeof(start_sequence));
                            m_video_size += sizeof(start_sequence);
                            memcpy(m_video_buf + m_video_size, &ori_nal, 1);
                            m_video_size += 1;
                        }
                        memcpy(m_video_buf + m_video_size, buf, len);
                        m_video_size += len;
                    }
                    break;
                
                case 30:                   // undefined
                case 31:                   // undefined
                default:
                    r_log("RTP H.264 NAL unit type %d\n", type);
                    result = -1;
                    break;
            }        
            return result;
        }
        else if(channel == 2)
        {
            if (payload_type != m_track[1].playload) 
            {
                return -1;
            }

            m_track[1].m_last_time_stamp = timestamp;

			if(m_track[1].is_aac)
			{
				buf += 4;
				len -= 4;
			}

            AVPacket* pkt = av_packet_alloc();

            if (av_new_packet(pkt, len) < 0)
            {
                av_packet_free(&pkt);
                return -1;
            }

            memcpy(pkt->data, buf, len);          
            this->finalize_packet(&m_track[1], pkt);
            m_dec_thread.Enque(pkt);
        }
    }
    
    return 0;
}

void rev_thread::finalize_packet(media_track* tr, AVPacket* pkt)
{
    if (!tr->m_base_timestamp)
        tr->m_base_timestamp = tr->m_last_time_stamp;
    /* assume that the difference is INT32_MIN < x < INT32_MAX,
     * but allow the first timestamp to exceed INT32_MAX */
    if (!tr->m_timestamp)
        tr->m_unwrapped_timestamp += tr->m_last_time_stamp;
    else
        tr->m_unwrapped_timestamp += (int32_t)(tr->m_last_time_stamp - tr->m_timestamp);
    tr->m_timestamp = tr->m_last_time_stamp;
    pkt->pts     = tr->m_unwrapped_timestamp + tr->m_base_timestamp;
    pkt->stream_index = tr->type;
}


int rev_thread::ParseHead(char* buf, int& con_len)
{
	char* p = buf;
	while (*p != '\0')
	{
		if(0 == memcmp(p, "RTSP/1.0", strlen("RTSP/1.0")))
		{
			p += strlen("RTSP/1.0");
			SkipSpace(p);
			int code = atoi(p);

			if(code != 200)
			{
				r_log("unexpected code:%d\n", code );
				return -1;
			}
		}
		else if(0 == memcmp(p, "Public:", strlen("Public:")))
		{
			char* pp = strstr(p, "SET_PARAMETER");
			if(pp)
			{
				m_klive_method.assign("SET_PARAMETER", strlen("SET_PARAMETER"));				    
			}
			else
			{
				pp = strstr(p, "GET_PARAMETER");
				if(pp)
				{
					m_klive_method.assign("GET_PARAMETER", strlen("GET_PARAMETER"));
				}
			}

			r_log("keep alive method:%s\n", m_klive_method.c_str());
		}
		else if(0 == memcmp(p, "Content-Base:", strlen("Content-Base:")))
		{
			p += strlen("Content-Base:");
			SkipSpace(p);
			char* pp = strstr(p, "\r\n");
			if(pp)
			{
				m_content_base.assign(p, pp - p);
				r_log("Content-Base:%s\n", m_content_base.c_str());
			}
		}
		else if(0 == memcmp(p, "Content-Length:", strlen("Content-Length:"))
			|| 0 == memcmp(p, "Content-length:", strlen("Content-length:")))
		{
			p += strlen("Content-Length:");
			SkipSpace(p);
			con_len = atoi(p);
			r_log("Content-Length:%d\n", con_len);
		}
		else if(0 == memcmp(p, "Session:", strlen("Session:")))
		{
			p += strlen("Session:");
			SkipSpace(p);
			char* pp = p;
			while (*pp != ';' && *pp != '\r')
			{
				++pp;
			}
			m_session.assign(p, pp - p);
			r_log("Session:%s\n", m_session.c_str());

			if(*pp == ';')
			{
				++pp;
				if(0 == memcmp(pp, "timeout=", strlen("timeout=")))
				{
					pp += strlen("timeout=");
					m_time_out = atoi(pp);
					r_log("timeout=%d\n", m_time_out);
				}
			}
		}

		SkipLine(p);		
	}

	return 0;
}

int rev_thread::ParseRespone()
{
	char* p = m_rev_buf;
	int size = m_rev_len;

	while (size != 0)
	{
		if(*p == '$')
		{
			if (size < 4)
			{
				int need = 4 - size;
				while(need)
				{
					m_rev_len = m_sock.Recv(p + size, need);

					if (m_rev_len < 0)
					{
						r_log("4 rev err:%d\n", errno);
						return -1;
					}

					size += m_rev_len;
					need -= m_rev_len;
				}
			}

			++p;
			int8_t channel = *p;
			++p;
			int16_t len = ntohs(*(int16_t*)(p));
			p += 2;
			size -= 4;

			r_log("chan:%d, len:%d\n", channel, len);

			if(size >= len)
			{
				this->ParseRtpRtcp(channel, (uint8_t*)p, len);
				p += len;
				size -= len;
			}
			else
			{
				int need = len - size;
				while(need)
				{
					m_rev_len = m_sock.Recv(p + size, need);

					if (m_rev_len < 0)
					{
						r_log("pkt rev err:%d\n", errno);
						return -1;
					}

					size += m_rev_len;
					need -= m_rev_len;
				}

				this->ParseRtpRtcp(channel, (uint8_t*)p, len);

				p += len;
				size = 0;                    
			}
		}
		else
		{
			if(0 != memcmp(p, "RTSP/1.0", strlen("RTSP/1.0")))
			{
				r_log("rev rsp illegal\n");
				return -1;
			}

			char* p_end = strstr(p, "\r\n\r\n");
			if(!p_end)
			{
				r_log("rev rsp no end\n");
				return -1;
			}

			p_end += strlen("\r\n\r\n");
			char temp = *p_end;
			*p_end = '\0';
			int  con_len = 0;

			if(this->ParseHead(p, con_len))
			{
				r_log("fail to parse hed\n");
				return -1;
			}

			*p_end = temp;
			size -= (p_end - p);
			p = p_end;


			switch(m_state)
			{
			case RTSP_OPTIONS_SEND:
				if(this->SendRtspCmd("DESCRIBE"))
				{
					return -1;
				}
				else
				{
					m_state = RTSP_DESCRIBE_SEND;
				}
				break;

			case RTSP_DESCRIBE_SEND:
				{
					if(size < con_len)
					{
						int need = con_len - size;
						while(need)
						{
							r_log("sdp len:%d, size:%d, need:%d\n", con_len, size, need);

							m_rev_len = m_sock.Recv(p + size, need);

							if (m_rev_len < 0)
							{
								r_log("sdp rev err:%d\n", errno);
								return -1;
							}

							size += m_rev_len;
							need -= m_rev_len;
						}
					}

					p_end = p + con_len;
					char temp = *p_end;
					*p_end = '\0';

					if(this->ParseSdp(p, con_len))
					{
						r_log("fail to parse sdp\n");
						return -1;
					}

					*p_end = temp;
					size -= (p_end - p);
					p = p_end;

					if(m_track[0].url.Size())
					{
						this->SendRtspSetup(m_track[0].url, "RTP/AVP/TCP;unicast;interleaved=0-1");
						m_state = RTSP_SETUP0_SEND;
					} 
					else if(m_track[1].url.Size())
					{
						this->SendRtspSetup(m_track[1].url, "RTP/AVP/TCP;unicast;interleaved=2-3");
						m_state = RTSP_SETUP1_SEND;
					}
					else
					{
						r_log("no media\n");
						return -1;
					}					
				}
				break;

			case RTSP_SETUP0_SEND:
				{
					if(m_track[1].url.Size())
					{
						this->SendRtspSetup(m_track[1].url, "RTP/AVP/TCP;unicast;interleaved=2-3");
						m_state = RTSP_SETUP1_SEND;
					}
					else
					{
						this->SendRtspCmd("PLAY");
						m_state = RTSP_PLAY_SEND;
						if (this->StartPlay())
						{
							return -1;
						}						
					}
				}
				break;

			case RTSP_SETUP1_SEND:
				{
					this->SendRtspCmd("PLAY");
					m_state = RTSP_PLAY_SEND;
					if (this->StartPlay())
					{
						return -1;
					}
				}
				break;

			case RTSP_PLAY_SEND:
				m_state = RTSP_REV_STREAM;
				break;

			case RTSP_REV_STREAM:
				break;

			case RTSP_TEARDOWN_SEND:
				return 1;

			default:
				return -1;
			}
		}
	}

	return 0;
}

int rev_thread::ParseSdp(char* buf, int size)
{
	char* p = buf;
	bool pase_media = false;
	int type;

	while (p - buf < size) {
		if (0 == memcmp(p, "m=", strlen("m="))) {
			pase_media = true;
			p += strlen("m=");

			if (0 == memcmp(p, "video", strlen("video"))) {
				type = VIDEO_TYPE;
				p += strlen("video");
			} else if (0 == memcmp(p, "audio", strlen("audio"))){
				type = AUDIO_TYPE;
				p += strlen("audio");
			} else{
				r_log("illegal sdp\n");
				return -1;
			}

			char* pp = strstr(p, "RTP/AVP");
			if (!pp) {
				r_log("illegal sdp\n");
				return -1;
			}

			p = pp + strlen("RTP/AVP");
			SkipSpace(p);
			m_track[type].playload = atoi(p);
			m_track[type].type = type;
			r_log("type:%d, playload:%d\n", type, m_track[type].playload);

		}
		else if(pase_media)
		{
			if (0 == memcmp(p, "a=control:", strlen("a=control:"))) {
				p += strlen("a=control:");
				char* pp = strstr(p, "\r\n");
				if (0 == memcmp(p, m_content_base.c_str(), m_content_base.Size())) {
					m_track[type].url.assign(p, pp - p);
				}
				else
				{
					m_track[type].url = m_content_base;
					m_track[type].url.append(p, pp - p);
				}
				r_log("type:%d, url:%s\n", type, m_track[type].url.c_str());
			}
			else if (0 == memcmp(p, "a=rtpmap:", strlen("a=rtpmap:"))) {
				p += strlen("a=rtpmap:");
				while (*p != ' ' && *p != '\0')
				{
					++p;
				}
				SkipSpace(p);
				char* pp = strchr(p, '/');
				m_track[type].codec.assign(p, pp - p);
				r_log("type:%d, codec:%s\n", type, m_track[type].codec.c_str());

				if(0 == memcmp(m_track[type].codec.c_str(), "MPEG4-GENERIC", strlen("MPEG4-GENERIC")))
				{
					m_track[type].is_aac = 1;
				}

				p = pp + 1;
				m_track[type].sample = atoi(p);
				r_log("type:%d, sample:%d\n", type, m_track[type].sample);

				while (*p != '\r' && *p != '/')
				{
					++p;
				}
				if(*p == '/')
				{
					++p;
					m_track[type].channel = atoi(p);
					r_log("type:%d, channel:%d\n", type, m_track[type].channel);
				}
				else if(*p == '\r')
				{
					--p;
				}
			}
			else if (0 == memcmp(p, "a=fmtp:", strlen("a=fmtp:"))) {
				char* pp = strstr(p, "sprop-parameter-sets=");
				if (pp)
				{
					pp += strlen("sprop-parameter-sets=");
					p = pp;
					while (*pp != '\r' && *pp != ';')
					{
						++pp;
					}

					m_sps.assign(p, pp - p);

					char base64packet[1024];
					uint8_t decoded_packet[1024];
					int packet_size;
					char* value = m_sps.Data();

					while (*value) {
						char *dst = base64packet;

						while (*value && *value != ','
							&& (dst - base64packet) < sizeof(base64packet) - 1) {
								*dst++ = *value++;
						}
						*dst++ = '\0';

						if (*value == ',')
							value++;

						packet_size = av_base64_decode(decoded_packet, base64packet,
							sizeof(decoded_packet));
						if (packet_size > 0) {
							memcpy(m_video_buf + m_video_size, start_sequence,sizeof(start_sequence));
							m_video_size += sizeof(start_sequence);

							memcpy(m_video_buf + m_video_size, decoded_packet, packet_size);
							m_video_size += packet_size;
						}
					}
				}
				
			}
		}

		SkipLine(p);
	}

	return 0;
}

int rev_thread::SendRtspSetup(r_string& setup_url, const char* transport)
{
	int off = 0;
	int ret = sprintf(m_send_buf, "%s %s RTSP/1.0\r\n", "SETUP", setup_url.c_str());
	off += ret;
	ret = sprintf(m_send_buf + off, "CSeq: %d\r\n", ++m_seq);
	off += ret;

	if(m_session.Size())
	{
		ret = sprintf(m_send_buf + off, "Session: %s\r\n", m_session.c_str());
		off += ret;
	}

	ret = sprintf(m_send_buf + off, "Transport: %s\r\n", transport);
	off += ret;
	ret = sprintf(m_send_buf + off, "User-Agent: lrtsp v0.1\r\n");
	off += ret;
	ret = sprintf(m_send_buf + off, "\r\n");
	off += ret;

	m_send_buf[off] = '\0';
	//r_log("%s\n", m_send_buf);

	ret = m_sock.Send(m_send_buf, off);

	if(ret != off)
	{
		r_log("send ret:%d, err:%d\n", ret, errno);
		return -1;
	}

	return 0;
}

int rev_thread::StartPlay()
{
	m_dec_thread.SetParam(&m_vren_thread, &m_aren_thread, &m_track[1]);
	m_vren_thread.SetParam(m_hwhd);
	m_aren_thread.SetParam(&m_track[1]);

	if(m_dec_thread.Start())
	{
		return -1;
	}

	if(m_vren_thread.Start())
	{
		return -1;
	}

	if(m_aren_thread.Start())
	{
		return -1;
	}

	return 0;
}

int rev_thread::StopPlay()
{
	this->SendRtspCmd("TEARDOWN");
	m_state = RTSP_TEARDOWN_SEND;
	m_dec_thread.SetExitFlag();
	m_vren_thread.SetExitFlag();
	m_aren_thread.SetExitFlag();

	m_dec_thread.WaitStop();
	m_vren_thread.WaitStop();
	m_aren_thread.WaitStop();

	return 0;
}
