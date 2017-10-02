#include "aren_thread.h"
#include "media_track.h"
#include "r_util.h"

extern "C"
{
#include "libavformat/avformat.h"
#include "libavutil/opt.h"
#include "libswresample/swresample.h"
};

aren_thread::aren_thread()
    : m_pXAudio2(NULL)
    , m_pMasteringVoice(NULL)
    , m_pSourceVoice(NULL)
    , m_last_pts(0)
	, swr_ctx(NULL)
	, dst_data(NULL)
	, m_last_nb_sample(0)
{
}

aren_thread::~aren_thread()
{
}

int aren_thread::Render(AVFrame* frame)
{
    if(m_pXAudio2 == NULL)
    {
        if(this->InitRender())
        {
            return -1;
        }
    }

    XAUDIO2_BUFFER buffer = {0};

	if(frame->format != AV_SAMPLE_FMT_S16)
	{
		if(swr_ctx == NULL)
		{
			if(this->InitSwr(frame))
			{
				return -1;
			}
		}

		int ret;
		if(frame->nb_samples != m_last_nb_sample)
		{
			if (dst_data)
				av_freep(&dst_data[0]);
			av_freep(&dst_data);
			dst_data = NULL;

			int  dst_linesize;
			ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, m_atrack->channel,
				frame->nb_samples, AV_SAMPLE_FMT_S16, 0);

			if (ret < 0) {
				r_log("Could not allocate destination samples\n");
				this->DestroySwr();
				return -1;
			}

			m_last_nb_sample = frame->nb_samples;
		}

		ret = swr_convert(swr_ctx, dst_data, frame->nb_samples, (const uint8_t **)frame->data, frame->nb_samples);
		if (ret < 0) {
			r_log("Error while converting\n");
			this->DestroySwr();
			return -1;
		}

		int data_size = 2;
		buffer.AudioBytes = frame->nb_samples * data_size * m_atrack->channel;  

		char* buf = (char* )malloc(buffer.AudioBytes);
		buffer.pAudioData = (BYTE*)buf; 
		buffer.pContext = buf;

		memcpy(buf, dst_data[0], buffer.AudioBytes);
	} 
	else
	{
		int data_size = 2;
		buffer.AudioBytes = frame->nb_samples * data_size * m_atrack->channel;  

		char* buf = (char* )malloc(buffer.AudioBytes);
		buffer.pAudioData = (BYTE*)buf; 
		buffer.pContext = buf;

		if(m_atrack->channel > 1)
		{
			char* pp = buf;

			for (int i = 0; i < frame->nb_samples; i++)
			{
				for (int ch = 0; ch < m_atrack->channel; ch++)
				{
					memcpy(pp, frame->data[ch] + data_size*i, data_size);
					pp += data_size;
				}
			}
		}
		else
		{
			memcpy(buf, frame->data[0], buffer.AudioBytes);
		}
	}

    HRESULT hr;
    if (FAILED(hr = m_pSourceVoice->SubmitSourceBuffer(&buffer)))
    {  
        return -1;  
    } 

    XAUDIO2_VOICE_STATE state;  
    m_pSourceVoice->GetState(&state);//获取状态  

    while (state.BuffersQueued > 3)  
    {		//r_log("que:%d\n", state.BuffersQueued);        Sleep(5);
        m_pSourceVoice->GetState(&state);  
    } 

    return 0;
}

void aren_thread::DestroyRender()
{
    if(m_pSourceVoice)
    {
        m_pSourceVoice->Stop(0);
        m_pSourceVoice->DestroyVoice();
        m_pSourceVoice = NULL;
    }

    if(m_pMasteringVoice)
    {
        m_pMasteringVoice->DestroyVoice();
        m_pMasteringVoice = NULL;
    }

    if(m_pXAudio2)
    {
        m_pXAudio2->Release();
        m_pXAudio2 = NULL;
        CoUninitialize();  
    }

	if(swr_ctx)
	{
		this->DestroySwr();
	}
}

int aren_thread::InitRender()
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED); 
    HRESULT hr;

    if (FAILED(hr = XAudio2Create(&m_pXAudio2)))  
    {  
        CoUninitialize();  
        return -1;  
    }  

    if (FAILED(hr = m_pXAudio2->CreateMasteringVoice(&m_pMasteringVoice)))  
    {  
        this->DestroyRender();
        return -1;  
    }  

	int data_size = 2;
    WAVEFORMATEX pwfx;  

    pwfx.wFormatTag         = WAVE_FORMAT_PCM;  //PCM格式  
    pwfx.wBitsPerSample     = data_size * 8;    //位数  
    pwfx.nChannels          = m_atrack->channel;                //声道数  
    pwfx.nSamplesPerSec     = m_atrack->sample;             //采样率  
    pwfx.nBlockAlign        = data_size * m_atrack->channel;         //数据块调整  
    pwfx.nAvgBytesPerSec    = pwfx.nBlockAlign * m_atrack->sample;      //平均传输速率  
    pwfx.cbSize             = 0;                //附加信息

    if (FAILED(hr = m_pXAudio2->CreateSourceVoice(&m_pSourceVoice, &pwfx, 0U, 2.0F, &m_call_back)))  
    {  
        this->DestroyRender();
        return -1;  
    }

    if (FAILED(hr = m_pSourceVoice->Start(0)))  
    {  
        this->DestroyRender();
        return -1;  
    }  

    return 0;
}

int aren_thread::InitSwr(AVFrame* frame)
{
	swr_ctx = swr_alloc();
	if (!swr_ctx) {
		r_log("Could not allocate resampler context\n");
		return -1;
	}

	/* set options */
	int64_t ch_layout = av_get_default_channel_layout(m_atrack->channel);

	av_opt_set_int(swr_ctx, "in_channel_layout",    ch_layout, 0);
	av_opt_set_int(swr_ctx, "in_sample_rate",       m_atrack->sample, 0);
	av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", (AVSampleFormat)frame->format, 0);

	av_opt_set_int(swr_ctx, "out_channel_layout",    ch_layout, 0);
	av_opt_set_int(swr_ctx, "out_sample_rate",       m_atrack->sample, 0);
	av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

	int ret;
	/* initialize the resampling context */
	if ((ret = swr_init(swr_ctx)) < 0) {
		r_log("Failed to initialize the resampling context\n");
		this->DestroySwr();
		return -1;
	}

	int  dst_linesize;
	ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, m_atrack->channel,
		frame->nb_samples, AV_SAMPLE_FMT_S16, 0);

	if (ret < 0) {
		r_log("Could not allocate destination samples\n");
		this->DestroySwr();
		return -1;
	}

	m_last_nb_sample = frame->nb_samples;

	return 0;
}

void aren_thread::DestroySwr()
{
	if (dst_data)
		av_freep(&dst_data[0]);
	av_freep(&dst_data);

	if(swr_ctx)
	{
		swr_free(&swr_ctx);
	}

	swr_ctx = NULL;
	dst_data = NULL;	
}
