#include "sws_video.h"

#include "error_define.h"

namespace am {

	SwsVideo::SwsVideo()
	{
		inited_ = false;

		frame_ = NULL;

		buffer_ = NULL;

		ctx_ = NULL;
	}


	SwsVideo::~SwsVideo()
	{
		CleanUp();
	}

	int SwsVideo::Init(AVPixelFormat src_fmt, int src_width, int src_height, AVPixelFormat dst_fmt, int dst_width, int dst_height)
	{
		if (inited_)
			return AE_NO;

		ctx_ = sws_getContext(
			src_width,
			src_height,
			src_fmt,
			dst_width,
			dst_height,
			dst_fmt,
			SWS_BICUBIC,
			NULL, NULL, NULL
		);

		if (!ctx_) {
			return AE_FFMPEG_NEW_SWSCALE_FAILED;
		}

		buffer_size_ = av_image_get_buffer_size(dst_fmt, dst_width, dst_height, 1);
		buffer_ = new uint8_t[buffer_size_];

		frame_ = av_frame_alloc();

		av_image_fill_arrays(frame_->data, frame_->linesize, buffer_, dst_fmt, dst_width, dst_height, 1);

		inited_ = true;

		return AE_NO;
	}

	int SwsVideo::Convert(const AVFrame* frame, uint8_t** out_data, int* len)
	{
		int error = AE_NO;
		if (!inited_ || !ctx_ || !buffer_)
			return AE_NEED_INIT;

		int ret = sws_scale(
			ctx_,
			(const uint8_t* const*)frame->data,
			frame->linesize,
			0, frame->height,
			frame_->data, frame_->linesize
		);

		*out_data = buffer_;
		*len = buffer_size_;

		return error;
	}

	void SwsVideo::CleanUp()
	{
		inited_ = false;

		if (ctx_)
			sws_freeContext(ctx_);

		ctx_ = NULL;

		if (frame_)
			av_frame_free(&frame_);

		frame_ = NULL;

		if (buffer_)
			delete[] buffer_;

		buffer_ = NULL;
	}

}