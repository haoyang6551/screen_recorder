#ifndef RESAMPLE_AUDIO
#define RESAMPLE_AUDIO

#include <stdint.h>

#include "headers_ffmpeg.h"

namespace am {
	struct SAMPLE_SETTING {
		int nb_samples;
		int64_t channel_layout;
		int nb_channels;
		AVSampleFormat fmt;
		int sample_rate;
	};

	class ResampleAudio
	{
	public:
		ResampleAudio();
		~ResampleAudio();

		int Init(const SAMPLE_SETTING* sample_src, const SAMPLE_SETTING* sample_dst, __out int* resapmled_frame_size);
		int Convert(const uint8_t* src, int src_len, uint8_t* dst, int dst_len);
	protected:
		void CleanUp();
	private:
		SwrContext* ctx_;
		SAMPLE_SETTING* sample_src_;
		SAMPLE_SETTING* sample_dst_;
	};
}
#endif
