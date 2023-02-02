#include "record_desktop_ffmpeg_gdi.h"
#include "error_define.h"
#include "windows.h"


namespace am {

	RecordDesktopFFmpegGdi::RecordDesktopFFmpegGdi()
	{
		av_register_all();
		avdevice_register_all();

		fmt_ctx_ = NULL;
		input_fmt_ = NULL;
		codec_ctx_ = NULL;
		codec_ = NULL;

		stream_index_ = -1;
		data_type_ = RecordDesktopDataTypes::AT_DESKTOP_RGBA;
	}


	RecordDesktopFFmpegGdi::~RecordDesktopFFmpegGdi()
	{
		Stop();
		CleanUp();
	}

	int RecordDesktopFFmpegGdi::Init(const RecordDesktopRect& rect, const int fps)
	{
		int error = AE_NO;
		if (inited_ == true) {
			return error;
		}

		fps_ = fps;
		rect_ = rect;

		char buff_video_size[50] = { 0 };
		std::cout << buff_video_size << 50 << " width " << rect.right_ - rect.left_ << " height " << rect.bottom_ - rect.top_ << std::endl;

		AVDictionary* options = NULL;
		av_dict_set_int(&options, "framerate", fps, AV_DICT_MATCH_CASE);
		av_dict_set_int(&options, "offset_x", rect.left_, AV_DICT_MATCH_CASE);
		av_dict_set_int(&options, "offset_y", rect.top_, AV_DICT_MATCH_CASE);
		av_dict_set(&options, "video_size", buff_video_size, AV_DICT_MATCH_CASE);
		av_dict_set_int(&options, "draw_mouse", 1, AV_DICT_MATCH_CASE);

		int ret = 0;
		do {
			fmt_ctx_ = avformat_alloc_context();
			input_fmt_ = av_find_input_format("gdigrab");

			//the framerate must be the same as encoder & muxer's framerate,otherwise the video can not sync with audio
			ret = avformat_open_input(&fmt_ctx_, "desktop", input_fmt_, &options);
			if (ret != 0) {
				error = AE_FFMPEG_OPEN_INPUT_FAILED;
				break;
			}

			ret = avformat_find_stream_info(fmt_ctx_, NULL);
			if (ret < 0) {
				error = AE_FFMPEG_FIND_STREAM_FAILED;
				break;
			}

			int stream_index = -1;
			for (int i = 0; i < fmt_ctx_->nb_streams; i++) {
				if (fmt_ctx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
					stream_index = i;
					break;
				}
			}

			if (stream_index == -1) {
				error = AE_FFMPEG_FIND_STREAM_FAILED;
				break;
			}

			stream_index_ = stream_index;
			codec_ctx_ = fmt_ctx_->streams[stream_index]->codec;
			codec_ = avcodec_find_decoder(codec_ctx_->codec_id);
			if (codec_ == NULL) {
				error = AE_FFMPEG_FIND_DECODER_FAILED;
				break;
			}

			ret = avcodec_open2(codec_ctx_, codec_, NULL);
			if (ret != 0) {
				error = AE_FFMPEG_OPEN_CODEC_FAILED;
				break;
			}

			start_time_ = fmt_ctx_->streams[stream_index_]->start_time;
			time_base_ = fmt_ctx_->streams[stream_index_]->time_base;
			pixel_fmt_ = fmt_ctx_->streams[stream_index_]->codec->pix_fmt;

			inited_ = true;
		} while (0);

		if (error != AE_NO) {
			printf("%s,error: %d %ld", err2str(error), ret, GetLastError());  // 注意GetLastError函数的用法
			CleanUp();
		}

		av_dict_free(&options);

		return error;
	}

	int RecordDesktopFFmpegGdi::Start()
	{
		if (running_ == true) {
			std::cout << "record desktop gdi is already running" << std::endl;
			return AE_NO;
		}

		if (inited_ == false) {
			return AE_NEED_INIT;
		}

		running_ = true;
		thread_ = std::thread(std::bind(&RecordDesktopFFmpegGdi::RecordFunc, this));

		return AE_NO;
	}

	int RecordDesktopFFmpegGdi::Pause()
	{
		paused_ = true;
		return AE_NO;
	}

	int RecordDesktopFFmpegGdi::Resume()
	{
		paused_ = false;
		return AE_NO;
	}

	int RecordDesktopFFmpegGdi::Stop()
	{
		running_ = false;
		if (thread_.joinable())
			thread_.join();

		return AE_NO;
	}

	void RecordDesktopFFmpegGdi::CleanUp()
	{
		if (codec_ctx_)
			avcodec_close(codec_ctx_);

		if (fmt_ctx_)
			avformat_close_input(&fmt_ctx_);

		fmt_ctx_ = NULL;
		input_fmt_ = NULL;
		codec_ctx_ = NULL;
		codec_ = NULL;

		stream_index_ = -1;
		inited_ = false;
	}

	int RecordDesktopFFmpegGdi::Decode(AVFrame* frame, AVPacket* packet)
	{
		int ret = avcodec_send_packet(codec_ctx_, packet);
		if (ret < 0) {
			std:: cout << "avcodec_send_packet failed, ret is " << ret << std::endl;

			return AE_FFMPEG_DECODE_FRAME_FAILED;
		}

		while (ret >= 0)
		{
			ret = avcodec_receive_frame(codec_ctx_, frame);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				break;
			}

			if (ret < 0) {
				return AE_FFMPEG_READ_FRAME_FAILED;
			}

			if (ret == 0 && on_data_) {
				//use relative time instead of device time
				frame->pts = av_gettime_relative();// -_start_time;
				frame->pkt_dts = frame->pts;
				frame->pkt_pts = frame->pts;
				on_data_(frame);
			}

			av_frame_unref(frame);//need to do this? avcodec_receive_frame said will call unref before receive
		}

		return AE_NO;
	}

	void RecordDesktopFFmpegGdi::RecordFunc()
	{
		AVPacket* packet = av_packet_alloc();
		AVFrame* frame = av_frame_alloc();

		int ret = 0;

		int got_pic = 0;
		while (running_ == true) {
			ret = av_read_frame(fmt_ctx_, packet);

			if (ret < 0) {
				if (on_error_) on_error_(AE_FFMPEG_READ_FRAME_FAILED);

				std:: cout << "read frame failed, ret is " << ret << std::endl;
				break;
			}

			if (packet->stream_index == stream_index_) {

				ret = Decode(frame, packet);
				if (ret != AE_NO) {
					if (on_error_) on_error_(AE_FFMPEG_DECODE_FRAME_FAILED);
					std::cout << "decode desktop frame failed" << std::endl;
					break;
				}
			}

			av_packet_unref(packet);
		}

		//flush packet in decoder
		Decode(frame, NULL);

		av_packet_free(&packet);
		av_frame_free(&frame);
	}

}