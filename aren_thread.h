#ifndef __AREN_THREAD_H__
#define __AREN_THREAD_H__

#include "ren_thread.h"
#include "xaudio2.h"
#include <stdint.h>

class media_track;
struct SwrContext;

struct StreamingVoiceContext : public IXAudio2VoiceCallback
{
	STDMETHOD_( void, OnVoiceProcessingPassStart )( UINT32 )
	{
	}
	STDMETHOD_( void, OnVoiceProcessingPassEnd )()
	{
	}
	STDMETHOD_( void, OnStreamEnd )()
	{
	}
	STDMETHOD_( void, OnBufferStart )( void* )
	{
	}
	STDMETHOD_( void, OnBufferEnd )( void* pBufferContext)
	{
		free(pBufferContext);
	}
	STDMETHOD_( void, OnLoopEnd )( void* )
	{
	}
	STDMETHOD_( void, OnVoiceError )( void*, HRESULT )
	{
	}
};


class aren_thread : public ren_thread
{
public:
    aren_thread();
    ~aren_thread();

    inline void SetParam( media_track* atrack)
    {
        m_atrack = atrack;
    }

private:
    int Render(AVFrame* frame);
    void DestroyRender();
    int InitRender();
	int InitSwr(AVFrame* frame);
	void DestroySwr();

private:
    IXAudio2 * m_pXAudio2;
    IXAudio2MasteringVoice* m_pMasteringVoice;
    IXAudio2SourceVoice* m_pSourceVoice;
    media_track* m_atrack;
    int64_t m_last_pts;
	SwrContext * swr_ctx;
	uint8_t  **dst_data;
	int m_last_nb_sample;
	StreamingVoiceContext m_call_back;
};

#endif
