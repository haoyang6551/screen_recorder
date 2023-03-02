#pragma once

#include <atomic>

#include "headers_ffmpeg.h"

namespace am {

	class SwsVideo
	{
	public:
		SwsVideo();
		~SwsVideo();

		int Init(
			AVPixelFormat src_fmt, int src_width, int src_height,
			AVPixelFormat dst_fmt, int dst_width, int dst_height
		);

		int Convert(const AVFrame* frame, uint8_t** out_data, int* len);

	private:
		void CleanUp();

	private:
		std::atomic_bool inited_;

		AVFrame* frame_;

		uint8_t* buffer_;
		int buffer_size_;

		struct SwsContext* ctx_;
	};

}