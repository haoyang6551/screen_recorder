#ifndef ENCODER_264
#define ENCODER_264

#include <atomic>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>

#include "headers_ffmpeg.h"
#include "ring_buffer.h"

namespace am {
	typedef std::function<void(AVPacket* packet)> Func264Data;
	typedef std::function<void(int)> Func264Error;

	class Encoder264
	{
	public:
		Encoder264();
		~Encoder264();

		int Init(int pic_width, int pic_height, int frame_rate, int bit_rate, int qb, int gop_size = 40);

		int get_extradata_size();
		const uint8_t* get_extradata();

		inline void registe_cb(
			Func264Data on_data,
			Func264Error on_error) {
			on_data_ = on_data;
			on_error_ = on_error;
		}

		const AVRational& get_time_base();

		int Start();

		void Stop();

		int put(const uint8_t* data, int data_len, AVFrame* frame);

	protected:
		void CleanUp();

	private:
		int Encode(AVFrame* frame, AVPacket* packet);
		void EncodeLoop();

	private:
		Func264Data on_data_;
		Func264Error on_error_;

		RingBuffer<AVFrame>* ring_buffer_;

		std::atomic_bool inited_;
		std::atomic_bool running_;

		std::thread thread_;

		AVCodec* encoder_;
		AVCodecContext* encoder_ctx_;
		AVFrame* frame_;
		uint8_t* buff_;
		int buff_size_;
		int y_size_;

		std::mutex mutex_;
		std::condition_variable cond_var_;
		bool cond_notify_;
	};
}


#endif
