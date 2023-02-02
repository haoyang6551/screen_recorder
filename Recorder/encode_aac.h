#ifndef ENCODER_AAC
#define ENCODER_AAC

#include <atomic>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>

#include "headers_ffmpeg.h"
#include "ring_buffer.h"

//#define SAVE_AAC

namespace am {
	typedef std::function<void(AVPacket* packet)> FuncAacData;
	typedef std::function<void(int)> FuncAacError;


	class EncoderAAC {
	public:
		EncoderAAC();
		~EncoderAAC();

		int Init(
			int nb_channels,
			int sample_rate,
			AVSampleFormat fmt,
			int bit_rate
		);

		int get_extradata_size();
		const uint8_t* get_extradata();

		int get_nb_samples();

		int Start();

		void Stop();

		int put(const uint8_t* data, int data_len, AVFrame* frame);

		inline void registe_cb(
			FuncAacData on_data,
			FuncAacError on_error) {
			on_data_ = on_data;
			on_error_ = on_error;
		}

		const AVRational& get_time_base();

	private:
		int Encode(AVFrame* frame, AVPacket* packet);

		void EncodeLoop();

		void CleanUp();

	private:
		FuncAacData on_data_;
		FuncAacError on_error_;

		RingBuffer<AVFrame>* ring_buffer_;

		std::atomic_bool inited_;
		std::atomic_bool running_;

		std::thread thread_;

		AVCodec* encoder_;
		AVCodecContext* encoder_ctx_;
		AVFrame* frame_;
		uint8_t* buff_;
		int buff_size_;

		std::mutex mutex_;
		std::condition_variable cond_var_;
		bool cond_notify_;
	};
}



#endif
