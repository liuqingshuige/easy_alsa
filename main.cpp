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

static inline int make_thread(void *(*pfn)(void *), void *arg, int detach, pthread_t *thid=0)
{
    int err = 0;
    pthread_t tid;
    pthread_t *ptid = &tid;
    pthread_attr_t attr;

    if (thid)
        ptid = thid;

    err = pthread_attr_init(&attr);
    if (err != 0)
        return err;

    if (detach)
        err = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    if (err == 0)
        err = pthread_create(ptid, &attr, pfn, arg);

    pthread_attr_destroy(&attr);
    return err;
}

static inline int make_thread_detached(void *(*pfn)(void *), void *arg)
{
    return make_thread(pfn, arg, 1);
}


static int g_8k_finish = 0;
static void *pfn1(void *param)
{
    void *handle = AI_EnableChn(8000, 1);
    FILE *fp = fopen("8k-mono.pcm", "wb");
    int ret = 0;
    char buffer[7680] = {0};
    int count = 400;
    int cnt = 0;

    while (count--)
    {
        if (handle && fp)
        {
            ret = AI_GetFrame(handle, buffer, sizeof(buffer), 80);
            if (ret > 0)
            {
                fwrite(buffer, 1, ret, fp);
                cnt++;
            }
        }
    }

    if (fp) fclose(fp);
    AI_DisableChn(handle);

    LOG("8k cnt: %d\n", cnt);
    g_8k_finish = 1;
    return 0;
}

static int g_16k_finish = 0;
static void *pfn2(void *param)
{
    void *handle = AI_EnableChn(16000, 1);
    FILE *fp = fopen("16k-mono.pcm", "wb");
    int ret = 0;
    char buffer[7680] = {0};
    int count = 400;
    int cnt = 0;

    while (count--)
    {
        if (handle && fp)
        {
            ret = AI_GetFrame(handle, buffer, sizeof(buffer), 80);
            if (ret > 0)
            {
                fwrite(buffer, 1, ret, fp);
                cnt++;
            }
        }
    }

    if (fp) fclose(fp);
    AI_DisableChn(handle);

    LOG("16k cnt: %d\n", cnt);
    g_16k_finish = 1;
    return 0;
}

static int g_22k_finish = 0;
static void *pfn3(void *param)
{
    void *handle = AI_EnableChn(22050, 2);
    FILE *fp = fopen("22k-stereo.pcm", "wb");
    int ret = 0;
    char buffer[7680] = {0};
    int count = 400;
    int cnt = 0;

    while (count--)
    {
        if (handle && fp)
        {
            ret = AI_GetFrame(handle, buffer, sizeof(buffer), 80);
            if (ret > 0)
            {
                fwrite(buffer, 1, ret, fp);
                cnt++;
            }
        }
    }

    if (fp) fclose(fp);
    AI_DisableChn(handle);

    LOG("22k cnt: %d\n", cnt);
    g_22k_finish = 1;
    return 0;
}

static int g_44k_finish = 0;
static void *pfn4(void *param)
{
    void *handle = AI_EnableChn(44100, 1);
    FILE *fp = fopen("44k-mono.pcm", "wb");
    int ret = 0;
    char buffer[7680] = {0};
    int count = 400;
    int cnt = 0;

    while (count--)
    {
        if (handle && fp)
        {
            ret = AI_GetFrame(handle, buffer, sizeof(buffer), 80);
            if (ret > 0)
            {
                fwrite(buffer, 1, ret, fp);
                cnt++;
            }
        }
    }

    if (fp) fclose(fp);
    AI_DisableChn(handle);

    LOG("44k cnt: %d\n", cnt);
    g_44k_finish = 1;
    return 0;
}

static int g_48k_finish = 0;
static void *pfn5(void *param)
{
    void *handle = AI_EnableChn(48000, 2);
    FILE *fp = fopen("48k-stereo.pcm", "wb");
    int ret = 0;
    char buffer[7680] = {0};
    int count = 400;
    int cnt = 0;

    while (count--)
    {
        if (handle && fp)
        {
            ret = AI_GetFrame(handle, buffer, sizeof(buffer), 80);
            if (ret > 0)
            {
                fwrite(buffer, 1, ret, fp);
                cnt++;
            }
        }
    }

    if (fp) fclose(fp);
    AI_DisableChn(handle);

    LOG("48k cnt: %d\n", cnt);
    g_48k_finish = 1;
    return 0;
}

int main(int argc, char **argv)
{
    make_thread_detached(pfn1, 0);
    make_thread_detached(pfn2, 0);
    make_thread_detached(pfn3, 0);
    make_thread_detached(pfn4, 0);
    make_thread_detached(pfn5, 0);

    //int count = 1000;
    while (!g_8k_finish || !g_16k_finish || !g_22k_finish || !g_44k_finish || !g_48k_finish)
    //while (count--)
    {
        usleep(10000);
    }

    return 0;
}

