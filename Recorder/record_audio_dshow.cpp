#include "record_audio_dshow.h"

#include "error_define.h"

namespace am {

	RecordAudioDshow::RecordAudioDshow()
	{
		av_register_all();
		avdevice_register_all();

		fmt_ctx_ = NULL;
		input_fmt_ = NULL;
		codec_ctx_ = NULL;
		codec_ = NULL;

		stream_index_ = -1;
	}


	RecordAudioDshow::~RecordAudioDshow()
	{
		Stop();
		CleanUp();
	}

	int RecordAudioDshow::Init(const std::string& device_name, const std::string& device_id, bool is_input)
	{
		int error = AE_NO;
		int ret = 0;

		if (inited_ == true)
			return error;

		do {

			device_name_ = device_name;
			device_id_ = device_id;
			is_input_ = is_input;

			input_fmt_ = av_find_input_format("dshow");
			if (!input_fmt_) {
				error = AE_FFMPEG_FIND_INPUT_FMT_FAILED;
				break;
			}

			fmt_ctx_ = avformat_alloc_context();
			ret = avformat_open_input(&fmt_ctx_, device_name_.c_str(), input_fmt_, NULL);
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
				if (fmt_ctx_->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
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

			inited_ = true;

			sample_rate_ = codec_ctx_->sample_rate;
			bit_rate_ = codec_ctx_->bit_rate;
			bit_per_sample_ = codec_ctx_->bits_per_coded_sample;
			channel_num_ = codec_ctx_->channels;
			fmt_ = codec_ctx_->sample_fmt;

			inited_ = true;
		} while (0);

		if (error != AE_NO) {
			std::cout << err2str(error) << "error: " << ret << std::endl;
			CleanUp();
		}

		return error;
	}

	int RecordAudioDshow::Start()
	{
		if (running_ == true) {
			std::cout << "record audio dshow is already running" << std::endl;
			return AE_NO;
		}

		if (inited_ == false) {
			return AE_NEED_INIT;
		}


		running_ = true;
		thread_ = std::thread(std::bind(&RecordAudioDshow::RecordLoop, this));

		return AE_NO;
	}

	int RecordAudioDshow::Pause()
	{
		return 0;
	}

	int RecordAudioDshow::Resume()
	{
		return 0;
	}

	int RecordAudioDshow::Stop()
	{
		running_ = false;
		if (thread_.joinable())
			thread_.join();

		return AE_NO;
	}

	const AVRational& RecordAudioDshow::get_time_base()
	{
		if (inited_ && fmt_ctx_ && stream_index_ != -1) {
			return fmt_ctx_->streams[stream_index_]->time_base;
		}
		else {
			return{ 1,AV_TIME_BASE };
		}
	}

	int64_t RecordAudioDshow::get_start_time()
	{
		return fmt_ctx_->streams[stream_index_]->start_time;
	}

	int RecordAudioDshow::Decode(AVFrame* frame, AVPacket* packet)
	{
		int ret = avcodec_send_packet(codec_ctx_, packet);
		if (ret < 0) {
			std::cout << "avcodec_send_packet failed " << ret << std::endl;

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

			if (ret == 0 && on_data_)
				on_data_(frame, extra_index_);

			av_frame_unref(frame);
		}

		return AE_NO;
	}

	void RecordAudioDshow::RecordLoop()
	{
		int ret = 0;

		AVPacket* packet = av_packet_alloc();

		AVFrame* frame = av_frame_alloc();

		while (running_ == true) {
			av_init_packet(packet);

			ret = av_read_frame(fmt_ctx_, packet);

			if (ret < 0) {
				if (on_error_) on_error_(AE_FFMPEG_READ_FRAME_FAILED, extra_index_);

				std::cout << "read frame failed:%d %ld" << ret << std::endl;
				break;
			}

			if (packet->stream_index == stream_index_) {
				ret = Decode(frame, packet);
				if (ret != AE_NO) {
					if (on_error_) on_error_(AE_FFMPEG_DECODE_FRAME_FAILED, extra_index_);

					std:: cout << "decode pcm packet failed:%d" << ret << std::endl;
					break;
				}
			}

			av_packet_unref(packet);
		}

		//flush packet left in decoder
		Decode(frame, NULL);

		av_packet_free(&packet);
		av_frame_free(&frame);
	}

	void RecordAudioDshow::CleanUp()
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

}