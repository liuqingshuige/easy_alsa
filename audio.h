/*
 * ALSA录音封装
 * Copyright FreeCode. All Rights Reserved.
 * MIT License (https://opensource.org/licenses/MIT)
 * 2024 by lingqingshuige
 */
#ifndef __FREE_AUDIO_H__
#define __FREE_AUDIO_H__
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#include <queue>
#include <vector>
#include <string>

#include <alsa/asoundlib.h>
#include "mutex.h"
#include "resampler.h"

using namespace std;

///////////////////////////////////////////////////////////////////////////////////////////////////
// 一帧音/视频
typedef struct PcmFrame_t
{
	PcmFrame_t()
    {
		size = 0;
		data = NULL;
	}
	~PcmFrame_t()
	{
		//ReleaseFrame();
	}
	inline bool setFrame(const char *pdata, int sz)
	{
		this->data = new char[sz];
		this->size = sz;
		memcpy(this->data, pdata, sz);
		return true;
	}
	inline void releaseFrame()
	{
		if (data)
			delete []data;
		data = NULL;
		size = 0;
	}
	char *getData() {return data;}
	int getSize() {return size;}

	int size;
	char *data;
}PcmFrame_t;
typedef std::queue<PcmFrame_t> PcmFrameQueue_t;

typedef struct PcmFrameQueueOps_t
{
	Mutex lock;
    Condition cond;
	PcmFrameQueue_t frameQueue;
	int queueDepth;

	PcmFrameQueueOps_t()
    {
		queueDepth = 4;
	}
	~PcmFrameQueueOps_t() {}
	void setQueueDepth(int depth)
	{
        MutexLockGuard mutexlockGuard(&lock);
		if (depth > 0)
            queueDepth = depth;
	}
	void clearFrame()
	{
		MutexLockGuard mutexlockGuard(&lock);
		for (int i=0; i<frameQueue.size(); ++i)
        {
			PcmFrame_t stFrame = frameQueue.front();
			stFrame.releaseFrame();
			frameQueue.pop();
		}
	}
    /* 需要调用frame.releaseFrame() */
	bool getFrame(PcmFrame_t &frame, int timeout_ms)
	{
		MutexLockGuard mutexlockGuard(&lock);
		if (frameQueue.size() == 0)
        {
			if (cond.timedWait(&lock, timeout_ms))
			{
                frame = frameQueue.front();
                frameQueue.pop();
                return true;
            }
			return false;
		}

		frame = frameQueue.front();
		frameQueue.pop();
		return true;
	}
	void putFrame(const char *pData, int dwSize)
	{
        MutexLockGuard mutexlockGuard(&lock);
		PcmFrame_t stFrame;

		if (stFrame.setFrame(pData, dwSize))
			frameQueue.push(stFrame);

		if (frameQueue.size() > queueDepth)
        {
			frameQueue.front().releaseFrame();
			frameQueue.pop();
		}
        cond.signal();
	}
}PcmFrameQueueOps_t;

///>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
// 注册音频结构
typedef struct PcmChannel_t
{
    PcmChannel_t(unsigned int rate, unsigned int chan, unsigned char bits,
        unsigned int orate, unsigned int ochan, unsigned char obits, unsigned int ptime)
    {
        samplerate = rate; channel = chan; width = bits;
        origin_samplerate = orate; origin_channel = ochan; origin_width = obits;
        resampler = NULL;

        if (rate != orate)
        {
            samples_per_frame = (orate / 1000 ) * ochan * ptime; // 16bit
            resampler = new CResampleEx();
            resampler->resample_create(true, false, ochan, orate, rate, samples_per_frame);
        }
    }

    ~PcmChannel_t()
    {
        queue.clearFrame();
        if (resampler)
            delete resampler;
    }

    /* 单双声道互转
     * in_size：in_ptr数据大小，单位字节
     * out_size：out_ptr缓冲区大小，单位字节
     */
    int operateMonoStereo(char *in_ptr, int in_size, char *out_ptr, int out_size)
    {
        if (channel == origin_channel) // 声道数相同，直接返回数据
        {
            if (out_size >= in_size)
            {
                memcpy(out_ptr, in_ptr, in_size);
                return in_size;
            }
            return -1; // 缓冲区不够
        }
        else // 声道数不同
        {
            if (origin_channel == 1 && channel == 2) // 单声道转双声道
            {
                short *ptr = (short *)in_ptr;
                char *tmp = new char[in_size<<1];
                short *ptr1 = (short *)tmp;

                for (int i = 0; i < in_size>>1; i++)
                {
                    ptr1[2*i] = ptr1[2*i+1] = ptr[i];
                }

                if (out_size >= (in_size<<1))
                {
                    memcpy(out_ptr, ptr1, (in_size<<1));
                    delete []tmp;
                    return (in_size<<1);
                }

                delete []tmp;
                return -1; // 缓冲区不够
            }
            else if (origin_channel == 2 && channel == 1) // 双声道转单声道
            {
                short *ptr = (short *)in_ptr;
                short *ptr1 = (short *)out_ptr;

                if (out_size >= (in_size>>1))
                {
                    for(int n = 0; n < in_size>>2; n++)
                    {
                        *ptr1++ = *ptr++;
                        ptr++;
                    }
                    return (in_size>>2);
                }
                return -1;
            }

            return 0; // 其他声道不支持
        }
    }

    int getData(char *buf, int len, int timeout_ms)
    {
        PcmFrame_t frame;
        bool res = queue.getFrame(frame, timeout_ms); /* 获取一帧原始数据 */
        if (res)
        {
            char *pdata = frame.getData();
            int size = frame.getSize();
            int ret = 0;

            if (samplerate == origin_samplerate) // 采样率相同
            {
                ret = operateMonoStereo(pdata, size, buf, len);
            }
            else // 采样率不相同，需要重采样
            {
                unsigned int osize = resampler->resample_get_output_size(); // 重采样后的采样点数
                int real_size = osize << 1; // 单位字节
                short *out_ptr = new short[osize];

                resampler->resample_run((short *)pdata, out_ptr);
                ret = operateMonoStereo((char *)out_ptr, real_size, buf, len);
                delete []out_ptr;
            }
            frame.releaseFrame();
            return ret;
        }
        return 0;
    }

    unsigned int samplerate;
    unsigned int channel;
    unsigned char width; // 位宽，当前仅支持16bit

    unsigned int origin_samplerate;
    unsigned int origin_channel;
    unsigned char origin_width; // 位宽，当前仅支持16bit
    int samples_per_frame;

    PcmFrameQueueOps_t queue;
    CResampleEx *resampler; // 重采样
}PcmChannel_t;
typedef std::vector<PcmChannel_t *>PcmChannelVec;


/* 录音得到PCM数据 */
class PcmRecord
{
public:
    ~PcmRecord();
    static PcmRecord *instance()
    {
        return &m_instance; // 饿汉
    }

    void *createChannel(unsigned int samplerate, unsigned int channel_cnt, unsigned char bits);
    void destroyChannel(void *channel);
    int readChannel(void *channel, char *buffer, int buflen, int timeout_ms);

private:
	PcmRecord();
    void feedChannel(const char *buffer, int len);
    void clearChannel(void);

    bool start(unsigned int samplerate, unsigned int channel_cnt, unsigned char bits, unsigned int ptime);
	void stop(void);

	bool open(unsigned int samplerate, unsigned int channel_cnt, unsigned char bits, unsigned int ptime);
	void close(void);
    int read(char *buf, int buflen);

	static void *PcmRecordThreadStub(void *param);
    void PcmRecordThread(void);

private:
	bool m_running;
	MutexLock m_mutex;
	pthread_t m_threadId;
    PcmChannelVec m_channels; // 保存所有注册的音频通道

    unsigned int m_samplerate;
    unsigned int m_channel;
    unsigned char m_bits;
    unsigned int m_ptime;

	snd_pcm_t *m_pcmHandle; // PCM句柄
	snd_pcm_uframes_t m_captureFrames; // samples_per_frame
	unsigned int m_captureSize; // in bytes

    static PcmRecord m_instance; // 单实例
};

////>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
void *AI_EnableChn(unsigned int samplerate, unsigned int channel_cnt);
void AI_DisableChn(void *ChnID);
int AI_GetFrame(void *ChnID, char *pstFrm, int len, int timeout_ms);


#endif

