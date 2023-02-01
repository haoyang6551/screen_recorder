#include "record_audio.h"

namespace am {
	RecordAudio::RecordAudio()
	{
		running_ = false;
		inited_ = false;
		paused_ = false;

		sample_rate_ = 48000;
		bit_rate_ = 3072000;
		channel_num_ = 2;
		bit_per_sample_ = bit_rate_ / sample_rate_ / channel_num_;
		fmt_ = AV_SAMPLE_FMT_FLT;
		on_data_ = nullptr;
		on_error_ = nullptr;

		device_name_ = "";
		device_id_ = "";
		is_input_ = false;
	}

	RecordAudio::~RecordAudio()
	{

	}
}
