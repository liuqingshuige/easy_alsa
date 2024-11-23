/*
 * 音频重采样封装：libsamplerate实现
 * Copyright FreeCode. All Rights Reserved.
 * MIT License (https://opensource.org/licenses/MIT)
 * 2024 by liuqingshuige
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "resampler.h"
#include "samplerate.h"

//////>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>
// libsamplerate impl
CResampleEx::CResampleEx()
{
    state = NULL;
    in_samples = out_samples = 8000;
    frame_in = frame_out = NULL;
    in_extra = out_extra = 0;
    ratio = 1.0;
}

CResampleEx::~CResampleEx()
{
    resample_destroy();
}

int CResampleEx::resample_create(
    bool high_quality,
    bool large_filter,
    unsigned int channel_count,
    unsigned int rate_in,
    unsigned int rate_out,
    unsigned int samples_per_frame)
{
    int type, err;

    /* Select conversion type */
    if (high_quality)
        type = large_filter ? SRC_SINC_BEST_QUALITY : SRC_SINC_MEDIUM_QUALITY;
    else
        type = large_filter ? SRC_SINC_FASTEST : SRC_LINEAR;

    /* Create converter */
    state = src_new(type, channel_count, &err);
    if (state == NULL)
    {
        printf("Error creating resample: %s\n", src_strerror(err));
        return -1;
    }

    /* Calculate ratio */
    ratio = rate_out * 1.0 / rate_in;

    /* Calculate number of samples for input and output */
    in_samples = samples_per_frame; /* 160 samples  */
    out_samples = rate_out / (rate_in / samples_per_frame);

    frame_in = (float *)calloc(in_samples + 8, sizeof(float));
    frame_out = (float *)calloc(out_samples + 8, sizeof(float));

    /* Set the converter ratio */
    err = src_set_ratio((SRC_STATE *)state, ratio);
    if (err != 0)
    {
        printf("Error creating resample: %s\n", src_strerror(err));
        return -1;
    }

    /* Done */
    printf("type=%s (%s), ch=%d, in/out rate=%d/%d\n", 
        src_get_name(type), src_get_description(type),
        channel_count, rate_in, rate_out);
    return 0;
}

void CResampleEx::resample_run(const short *input, short *output)
{
    SRC_DATA src_data;

    /* Check! */
    if (!state)
        return;

    /* Convert samples to float */
    src_short_to_float_array(input, frame_in, in_samples);

    if (in_extra)
    {
        for (unsigned int i=0; i<in_extra; ++i)
            frame_in[in_samples+i] = frame_in[in_samples-1];
    }

    /* Prepare SRC_DATA */
    memset(&src_data, 0, sizeof(src_data));
    src_data.data_in = frame_in;
    src_data.data_out = frame_out;
    src_data.input_frames = in_samples + in_extra;
    src_data.output_frames = out_samples + out_extra;
    src_data.src_ratio = ratio;

    /* Process! */
    src_process((SRC_STATE *)state, &src_data);

    /* Convert output back to short */
    src_float_to_short_array(frame_out, output, src_data.output_frames_gen);

    /* Replay last sample if conversion couldn't fill up the whole 
     * frame. This could happen for example with 22050 to 16000 conversion.
     */
    if (src_data.output_frames_gen < (int)out_samples)
    {
        if (in_extra < 4)
            in_extra++;

        for (unsigned int i=src_data.output_frames_gen; i<out_samples; ++i)
        {
            output[i] = output[src_data.output_frames_gen-1];
        }
    }
}

unsigned int CResampleEx::resample_get_input_size(void)
{
    return in_samples;
}

unsigned int CResampleEx::resample_get_output_size(void)
{
    return out_samples;
}

void CResampleEx::resample_destroy(void)
{
    if (state)
    {
        src_delete((SRC_STATE *)state);
        state = NULL;
    }

    if (frame_in)
        free(frame_in);
    frame_in = NULL;

    if (frame_out)
        free(frame_out);
    frame_out = NULL;
}




















