#include "dec_thread.h"
#include "ren_thread.h"
#include "media_track.h"
#include "r_util.h"

extern "C"
{
#include "libavformat/avformat.h"
};

dec_thread::dec_thread()
    : m_head(NULL)
    , m_arthread(NULL)
    , m_vrthread(NULL)
    , m_vdec_ctx(NULL)
    , m_adec_ctx(NULL)
{
}

dec_thread::~dec_thread()
{
    this->ClearQue();
}

AVPacket* dec_thread::Deque()
{
    AVPacket* pkt = NULL;

    while (true)
    {
        RLock();

        if(m_exit_flag)
        {
            this->ClearQue();
            RUnlock();
            return NULL;
        }

        if(m_head)
        {
            pkt = m_head->m_packet;

            if(m_head->m_next)
            {
                packet_queue* temp = m_head;
                m_head = m_head->m_next;
                free(temp);
            }
            else
            {
                free(m_head);
                m_head = NULL;
            }

            RUnlock();
            return pkt;
        }
        else
        {
            RUnlock();
            Sleep(10);
        }
    }
}

int dec_thread::Run()
{
    av_register_all();

    AVPacket* pkt = this->Deque();

    while(pkt)
    {
        if(pkt->stream_index == 0)
        {
            this->DecodeVideo(pkt);
        } 
        else if(pkt->stream_index == 1)
        {
            this->DecodeAudio(pkt);
        }        

        av_packet_unref(pkt);
        av_packet_free(&pkt);
        pkt = this->Deque();
    }

    if(m_vdec_ctx)
    {
        avcodec_free_context(&m_vdec_ctx);
    }
    if(m_adec_ctx)
    {
        avcodec_free_context(&m_adec_ctx);
    }

    return 0;
}

void dec_thread::Enque(AVPacket* pkt)
{
    RLock();

    if (m_exit_flag)
    {
        RUnlock();
        av_packet_unref(pkt);
        av_packet_free(&pkt);
        return;
    }

    if (!m_head) {
        packet_queue* pp = (packet_queue*)malloc(sizeof(packet_queue));
        pp->m_packet = pkt;
        pp->m_next = NULL;

        m_head = pp;
    }
    else
    {
        packet_queue* p = m_head;
        while(p->m_next) {
            p = p->m_next;
        }

        packet_queue* pp = (packet_queue*)malloc(sizeof(packet_queue));
        pp->m_packet = pkt;
        pp->m_next = NULL;
        p->m_next = pp;
    }

    RUnlock();
}

void dec_thread::ClearQue()
{
    while (m_head)
    {
        packet_queue* p = m_head;
        m_head = m_head->m_next;
        av_packet_unref(p->m_packet);
        av_packet_free(&p->m_packet);
        free(p);
    }
}

int dec_thread::DecodeVideo(AVPacket* pkt)
{
    if(m_vdec_ctx == NULL)
    {
        AVCodec *dec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if (!dec) {
            r_log("Failed to find  codec\n");
            return -1;
        }

        m_vdec_ctx = avcodec_alloc_context3(dec);
        if (!m_vdec_ctx) {
            r_log("Failed to allocate the codec context\n");
            return -1;
        }

        if (avcodec_open2(m_vdec_ctx, dec, NULL) < 0) {
            r_log("Could not open codec\n");
            return -1;
        }
    }    

    int ret = avcodec_send_packet(m_vdec_ctx, pkt);
    if (ret < 0) {
        r_log("Error sending a packet for decoding\n");
        return -1;
    }

    while (ret >= 0) {
        AVFrame* frame = av_frame_alloc();
        if (!frame) {
            r_log("Could not allocate video frame\n");
            return -1;
        }

        ret = avcodec_receive_frame(m_vdec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            av_frame_unref(frame);
            av_frame_free(&frame);
            break;
        }
        else if (ret < 0) {
            r_log("Error during decoding\n");
            av_frame_unref(frame);
            av_frame_free(&frame);
            return -1;
        }
        else
        {
            m_vrthread->Enque(frame);
        }
    }

    return 0;
}

int dec_thread::DecodeAudio(AVPacket* pkt)
{
    if(m_adec_ctx == NULL)
    {
        AVCodecID codec_id;
        int sample, channel;
        if(this->GetAudioParam(codec_id, sample, channel))
        {
            return -1;
        }

        AVCodec *dec = avcodec_find_decoder((AVCodecID)codec_id);
        if (!dec) {
            r_log("Failed to find  codec\n");
            return -1;
        }

        m_adec_ctx = avcodec_alloc_context3(dec);
        if (!m_adec_ctx) {
            r_log("Failed to allocate the codec context\n");
            return -1;
        }

        m_adec_ctx->sample_rate = sample;
        m_adec_ctx->channels = channel;

        int ret = avcodec_open2(m_adec_ctx, dec, NULL);
        if ( ret < 0) {
            r_log("Could not open codec\n");
            return -1;
        }
    }

    int ret = avcodec_send_packet(m_adec_ctx, pkt);
    if (ret < 0) {
        r_log("Error submitting the packet to the decoder\n");
        return -1;
    }

    /* read all the output frames (in general there may be any number of them */
    while (ret >= 0) {
        AVFrame* frame = av_frame_alloc();
        if (!frame) {
            r_log("Could not allocate video frame\n");
            return -1;
        }

        ret = avcodec_receive_frame(m_adec_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
        {
            av_frame_unref(frame);
            av_frame_free(&frame);
            break;
        }
        else if (ret < 0) {
            r_log("Error during decoding\n");
            av_frame_unref(frame);
            av_frame_free(&frame);
            return -1;
        }

        m_arthread->Enque(frame);
    }

    return 0;
}

int dec_thread::GetAudioParam(AVCodecID& codec_id, int& sample, int& channel)
{
    if(m_atrack->playload == 8)
    {
        codec_id = AV_CODEC_ID_PCM_ALAW;
        sample = 8000;
        channel = 1;

        return 0;
    }
    else if(m_atrack->playload == 0)
    {
        codec_id = AV_CODEC_ID_PCM_MULAW;
        sample = 8000;
        channel = 1;

        return 0;
    }
	else if(m_atrack->is_aac)
	{
		codec_id = AV_CODEC_ID_AAC;
		sample = m_atrack->sample;
		channel = m_atrack->channel;

		return 0;
	}

    return -1;
}
