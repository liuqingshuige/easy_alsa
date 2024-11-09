/*
 * 音频重采样封装：libsamplerate实现
 * Copyright FreeCode. All Rights Reserved.
 * MIT License (https://opensource.org/licenses/MIT)
 * 2024 by lingqingshuige
 */
#ifndef __AUDIO_RESAMPLE_EX_H__
#define __AUDIO_RESAMPLE_EX_H__

class CResampleEx
{
public:
	CResampleEx();
	~CResampleEx();

public:
    int resample_create(
        bool high_quality,
        bool large_filter,
        unsigned int channel_count,
        unsigned int rate_in,
        unsigned int rate_out,
        unsigned int samples_per_frame);
    void resample_run(const short *input, short *output);
    unsigned int resample_get_input_size(void);
    unsigned int resample_get_output_size(void);
    void resample_destroy(void);

private:
    void *state;
    unsigned int in_samples;
    unsigned int out_samples;
    float *frame_in, *frame_out;
    unsigned in_extra, out_extra;
    double ratio;
};


#endif

