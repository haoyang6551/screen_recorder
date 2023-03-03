#ifndef RECORD_AUDIO
#define RECORD_AUDIO

#include "audio_common_define.h"

#include "headers_ffmpeg.h"

#include <atomic>
#include <thread>
#include <functional>

namespace am {
	typedef std::function<void(AVFrame* packet, int index)> AudioDataFunc;
	typedef std::function<void(int, int)> AudioErrorFunc;

	class RecordAudio
	{
	public:
		RecordAudio();
		virtual ~RecordAudio();

		virtual int Init(const std::string& device_name,
			const std::string& device_id,
			bool is_input) = 0;

		virtual int Start() = 0;

		virtual int Pause() = 0;

		virtual int Resume() = 0;

		virtual int Stop() = 0;

		virtual const AVRational& get_time_base() = 0;

		virtual int64_t get_start_time() = 0;

	public:
		inline bool get_is_recording() { return running_; }

		inline int get_sample_rate() { return sample_rate_; }

		inline int get_bit_rate() { return bit_rate_; }

		inline int get_bit_per_sample() { return bit_per_sample_; }

		inline int get_channel_num() { return channel_num_; }

		inline AVSampleFormat get_fmt() { return fmt_; }

		inline const std::string& get_device_name() { return device_name_; }

		inline void registe_cb(
			AudioDataFunc on_data,
			AudioErrorFunc on_error) {
			on_data_ = on_data;
			on_error_ = on_error;
		}

	protected:
		std::atomic_bool running_;
		std::atomic_bool inited_;
		std::atomic_bool paused_;

		std::thread thread_;

		int sample_rate_;

		int bit_rate_;

		int channel_num_;

		int bit_per_sample_;

		AVSampleFormat fmt_;

		std::string device_name_;
		std::string device_id_;

		bool is_input_;

		AudioErrorFunc on_error_;

		AudioDataFunc on_data_;

		int extra_index_;
	};
}

#endif 