/*
 * ALSA录音封装
 * Copyright FreeCode. All Rights Reserved.
 * MIT License (https://opensource.org/licenses/MIT)
 * 2024 by lingqingshuige
 */
#include "audio.h"

static char *log_time(void)
{
    static char ctime_buf[128] = {0};
    struct tm* t;
    struct timeval tv;
    gettimeofday(&tv, NULL);

    t = localtime(&tv.tv_sec);
    snprintf(ctime_buf, sizeof(ctime_buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d", 
        t->tm_year+1900,
        t->tm_mon+1,
        t->tm_mday,
        t->tm_hour,
        t->tm_min,
        t->tm_sec,
        (int)tv.tv_usec/1000);
    return ctime_buf;
}
#define LOG(fmt, ...) printf("[%s %s:%d] " fmt, log_time(), __FUNCTION__, __LINE__, ##__VA_ARGS__)


PcmRecord PcmRecord::m_instance;
PcmRecord::PcmRecord()
{
    m_running = false;
    m_pcmHandle = NULL;
    m_threadId = 0;
    start(16000, 1, 16, 20); // 当前仅支持位宽16bit，帧长20ms
}

PcmRecord::~PcmRecord()
{
    stop();
    clearChannel();
}

bool PcmRecord::start(unsigned int samplerate, unsigned int channel_cnt, unsigned char bits, unsigned int ptime)
{
    m_running = true;
    m_samplerate = samplerate;
    m_channel = channel_cnt;
    m_bits = bits;
    m_ptime = ptime;
    pthread_create(&m_threadId, NULL, PcmRecordThreadStub, this);
    return m_running;
}

void PcmRecord::stop(void)
{
    m_running = false;
    if (m_threadId)
        pthread_join(m_threadId, 0);
    m_threadId = 0;
}

bool PcmRecord::open(unsigned int samplerate, unsigned int channel_cnt, unsigned char bits, unsigned int ptime)
{
    snd_pcm_format_t format;
    int ret = 0, dir = 0;
    unsigned int sampleRate = samplerate;
    unsigned int buffer_time, period_time;
    snd_pcm_uframes_t frames;
    snd_pcm_hw_params_t *pcm_params; // 配置硬件参数结构体

    /* 打开一个PCM采集设备 */
    ret = snd_pcm_open(&m_pcmHandle, "default", SND_PCM_STREAM_CAPTURE, 0);
    if (ret < 0)
    {
        LOG("unable to open pcm device: %s\n", snd_strerror(ret));
        return false;
    }

    /* params申请内存--这是栈内存 */
    snd_pcm_hw_params_alloca(&pcm_params);
    /* 使用pcm设备初始化params */
    ret = snd_pcm_hw_params_any(m_pcmHandle, pcm_params);
    if (ret < 0)
    {
        LOG("config pcm device: %s\n", snd_strerror(ret));
        goto exit_1;
    }

    /* 设置多路数据在buffer中的存储方式
    SND_PCM_ACCESS_RW_INTERLEAVED每个周期(period)左右声道的数据交叉存放 */
    ret = snd_pcm_hw_params_set_access(m_pcmHandle, pcm_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (ret < 0)
    {
        LOG("config set_access: %s\n", snd_strerror(ret));
        goto exit_1;
    }

    /* 设置采样格式 */
    switch (bits)
    {
    case 8: format = SND_PCM_FORMAT_S8; break;
    case 16: format = SND_PCM_FORMAT_S16_LE; break;
    case 24: format = SND_PCM_FORMAT_S24_LE; break;
    case 32: format = SND_PCM_FORMAT_S32_LE; break;
    default: format = SND_PCM_FORMAT_S16_LE; break;
    }
    ret = snd_pcm_hw_params_set_format(m_pcmHandle, pcm_params, format);
    if (ret < 0)
    {
        LOG("config set_format: %s\n", snd_strerror(ret));
        goto exit_1;
    }

    /* 设置声道数 */
    ret = snd_pcm_hw_params_set_channels(m_pcmHandle, pcm_params, channel_cnt);
    if (ret < 0)
    {
        LOG("config set_channel: %s\n", snd_strerror(ret));
        goto exit_1;
    }

    /* 设置采样率,如果采样率不支持，会用硬件支持最接近的采样率 */
    ret = snd_pcm_hw_params_set_rate_near(m_pcmHandle, pcm_params, &sampleRate, &dir);
    if (ret < 0)
    {
        LOG("config set_rate_near: %s\n", snd_strerror(ret));
        goto exit_1;
    }

    /* 获取中最大缓冲时间，单位us */
    snd_pcm_hw_params_get_buffer_time_max(pcm_params, &buffer_time, 0);
    if (buffer_time > 1000000)
        buffer_time = 1000000;
    /* 设置缓冲时间 */
    ret = snd_pcm_hw_params_set_buffer_time_near(m_pcmHandle, pcm_params, &buffer_time, 0);
    if (ret < 0)
    {
        LOG("config set_buffer_time_near: %s\n", snd_strerror(ret));
        goto exit_1;
    }

    /* 设置周期 */
    period_time = ptime*1000; // 20ms
    ret = snd_pcm_hw_params_set_period_time_near(m_pcmHandle, pcm_params, &period_time, 0);
    if (ret < 0)
    {
        LOG("config set_period_time_near: %s\n", snd_strerror(ret));
        goto exit_1;
    }

    /* 设置非阻塞--2024-6-6*/
	//ret = snd_pcm_nonblock(m_pcmHandle, 1); // 0 = block, 1 = nonblock mode, 2 = abort
	//LOG("set pcm_nonblock ret: %d\n", ret);

    /* 让这些参数作用于PCM设备 */
    ret = snd_pcm_hw_params(m_pcmHandle, pcm_params);
    if (ret < 0)
    {
        LOG("unable toset hw params: %s\n", snd_strerror(ret));
        goto exit_1;
    }

    /* 获取帧大小 */
    snd_pcm_hw_params_get_period_size(pcm_params, &m_captureFrames, &dir);
    m_captureSize = snd_pcm_frames_to_bytes(m_pcmHandle, m_captureFrames);
    LOG("snd_pcm_uframes_t: %lu frame, bytes: %u\n", m_captureFrames, m_captureSize);
    return true;

exit_1:
    snd_pcm_close(m_pcmHandle);
    m_pcmHandle = 0;
    return false;
}

void PcmRecord::close(void)
{
    if (m_pcmHandle)
    {
        snd_pcm_drain(m_pcmHandle);
        snd_pcm_close(m_pcmHandle);
        m_pcmHandle = NULL;
    }
}

/*
 * 读取一帧PCM数据
 * buffer：保存读取的PCM数据
 * buflen：buffer长度，单位字节
 * return：返回实际读取的字节数，失败返回0
 */
int PcmRecord::read(char *buffer, int buflen)
{
    int ret = 0;

    if (m_pcmHandle)
    {
        ret = snd_pcm_readi(m_pcmHandle, buffer, m_captureFrames); // 从PCM读取交错帧
        if (ret == -EPIPE) // -EPIPE for the xrun and -ESTRPIPE for the suspended status
        {
            LOG("overrun...\n");
            ret = 0;
            snd_pcm_prepare(m_pcmHandle);
        }
        else if (ret < 0)
        {
            LOG("error read: %d, %s\n", ret, snd_strerror(ret)); // -ENODEV
            ret = 0;
        }
        else if (ret != m_captureFrames)
        {
            LOG("less read: %s\n", snd_strerror(ret));
        }
    }
    return ret * (m_bits>>3);
}

void *PcmRecord::PcmRecordThreadStub(void *param)
{
    PcmRecord *inst = (PcmRecord *)param;
    inst->PcmRecordThread();
    return NULL;
}

void PcmRecord::PcmRecordThread(void)
{
    int ret = 0;
    char buffer[7680] = {0}; // 48000Hz 2chn 16bit(2byte) 20ms --> 48000*20/1000=960*2*2=1920*2=3840
    bool success = false;
    unsigned int try_sleep[10] = {2000, 4000, 6000, 8000, 16000, 24000, 32000, 40000, 45000, 90000};
    unsigned char try_times = 0, fail_times = 0;

    success = open(m_samplerate, m_channel, m_bits, m_ptime);
    LOG("start capture thread\n");
    while (m_running)
    {
        if (!success)
        {
            usleep(try_sleep[try_times%10] * 1000);
            try_times++;
            success = open(m_samplerate, m_channel, m_bits, m_ptime);
            continue;
        }

        ret = read(buffer, sizeof(buffer));
        if (ret > 0) // 将数据给到注册的音频通道
        {
            fail_times = 0;
            feedChannel(buffer, ret);
        }
        else
        {
            fail_times++;
            if (fail_times >= 100) // 连续100帧没有取得数据说明声卡可能被移除了或者其他错误
            {
				fail_times = 0;
                success = false; // 尝试重新打开
                close();
            }
        }
    }
    close();
    LOG("exit capture thread\n");
}

/*
 * 创建一个录音通道
 * samplerate：采样率，如8000，16000，44100等
 * channel_cnt：1：单声道，2：双声道，仅支持这2个
 * bits：位宽，当前仅支持16bit
 * return：成功返回通道句柄，失败返回NULL
 */
void *PcmRecord::createChannel(unsigned int samplerate, unsigned int channel_cnt, unsigned char bits)
{
    PcmChannel_t *ch = NULL;
    if ((bits == 16) && (channel_cnt == 1 || channel_cnt == 2))
    {
        ch = new PcmChannel_t(samplerate, channel_cnt, bits, m_samplerate, m_channel, m_bits, m_ptime);
        MutexLockGuard mutexlockGuard(&m_mutex);
        m_channels.push_back(ch);
    }
    return ch;
}

/*
 * 销毁通道，同时从数组中移除
 */
void PcmRecord::destroyChannel(void *channel)
{
    MutexLockGuard mutexlockGuard(&m_mutex);
    for (int i=0; i<m_channels.size(); i++)
    {
        PcmChannel_t *ch = m_channels[i];
        if (ch == channel)
        {
            m_channels.erase(m_channels.begin() + i);
            delete ch;
            LOG("remain %lu chn\n", m_channels.size());
            break;
        }
    }
}

/*
 * PcmRecordThread()调用
 */
void PcmRecord::feedChannel(const char *buffer, int len)
{
    MutexLockGuard mutexlockGuard(&m_mutex);
    for (int i=0; i<m_channels.size(); i++)
    {
        PcmChannel_t *ch = m_channels[i];
        ch->queue.putFrame(buffer, len);
    }
}

/*
 * 析构函数调用
 */
void PcmRecord::clearChannel(void)
{
    MutexLockGuard mutexlockGuard(&m_mutex);
    PcmChannelVec::iterator it = m_channels.begin();
    while ((it = m_channels.begin()) != m_channels.end())
    {
        PcmChannel_t *ch = *it;
        m_channels.erase(it);
        delete ch;
    }
}

/*
 * 获取一帧音频数据保存在buffer中
 * buflen：buffer长度，单位字节
 * return：成功返回实际的音频长度，单位字节，失败返回0，缓冲区长度不够返回-1
 */
int PcmRecord::readChannel(void *channel, char *buffer, int buflen, int timeout_ms)
{
    PcmChannel_t *ch = (PcmChannel_t *)channel;
    return ch ? ch->getData(buffer, buflen, timeout_ms) : 0;
}

/////>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
// 实例定义
void *AI_EnableChn(unsigned int samplerate, unsigned int channel_cnt)
{
    return PcmRecord::instance()->createChannel(samplerate, channel_cnt, 16);
}

void AI_DisableChn(void *ChnID)
{
    PcmRecord::instance()->destroyChannel(ChnID);
}

int AI_GetFrame(void *ChnID, char *pstFrm, int len, int timeout_ms)
{
    return PcmRecord::instance()->readChannel(ChnID, pstFrm, len, timeout_ms);
}


