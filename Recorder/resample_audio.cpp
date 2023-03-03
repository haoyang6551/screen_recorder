#include "resample_audio.h"

#include "error_define.h"

namespace am {
	ResampleAudio::ResampleAudio()
	{
		sample_src_ = NULL;
		sample_dst_ = NULL;
		ctx_ = NULL;
	}

	ResampleAudio::~ResampleAudio()
	{
		CleanUp();
	}

	int ResampleAudio::Init(const SAMPLE_SETTING* sample_src, const SAMPLE_SETTING* sample_dst, int* resapmled_frame_size)
	{
		int err = AE_NO;

		do {
			sample_src_ = (SAMPLE_SETTING*)malloc(sizeof(SAMPLE_SETTING));
			sample_dst_ = (SAMPLE_SETTING*)malloc(sizeof(SAMPLE_SETTING));

			memcpy(sample_src_, sample_src, sizeof(SAMPLE_SETTING));
			memcpy(sample_dst_, sample_dst, sizeof(SAMPLE_SETTING));

			ctx_ = swr_alloc_set_opts(nullptr,
				sample_dst_->channel_layout, sample_dst_->fmt, sample_dst_->sample_rate,
				sample_src_->channel_layout, sample_src_->fmt, sample_src_->sample_rate,
				0, nullptr);

			if (ctx_ == nullptr) {
				err = AE_RESAMPLE_INIT_FAILED;
				break;
			}

			int ret = swr_init(ctx_);
			if (ret < 0) {
				err = AE_RESAMPLE_INIT_FAILED;
				break;
			}

			*resapmled_frame_size = av_samples_get_buffer_size(nullptr, sample_dst_->nb_channels, sample_dst_->nb_samples, sample_dst_->fmt, 1);

		} while (0);

		if (err != AE_NO) {
			CleanUp();
		}

		return err;
	}

	int ResampleAudio::Convert(const uint8_t* src, int src_len, uint8_t* dst, int dst_len)
	{

		uint8_t* out[2] = { 0 };
		out[0] = dst;
		out[1] = dst + dst_len / 2;

		const uint8_t* in1[2] = { src,src + src_len / 2 };

		return swr_convert(ctx_, out, sample_dst_->nb_samples, in1, sample_src_->nb_samples);
	}

	void ResampleAudio::CleanUp()
	{
		if (sample_src_)
			free(sample_src_);

		if (sample_dst_)
			free(sample_dst_);

		if (ctx_)
			swr_free(&ctx_);
	}
}