#include "encode_aac.h"

#include "ring_buffer.h"

#include "error_define.h"

namespace am {

	EncoderAAC::EncoderAAC()
	{
		av_register_all();

		ring_buffer_ = new RingBuffer<AVFrame>(1024 * 1024 * 10);

		inited_ = false;
		running_ = false;

		encoder_ = NULL;
		encoder_ctx_ = NULL;
		frame_ = NULL;
		buff_ = NULL;
		buff_size_ = 0;

		cond_notify_ = false;
	}

	EncoderAAC::~EncoderAAC()
	{
		Stop();

		CleanUp();

		delete ring_buffer_;
	}

	int EncoderAAC::Init(int nb_channels, int sample_rate, AVSampleFormat fmt, int bit_rate)
	{
		int err = AE_NO;
		int ret = 0;

		if (inited_ == true)
			return err;

		do {
			encoder_ = avcodec_find_encoder(AV_CODEC_ID_AAC);
			if (!encoder_) {
				err = AE_FFMPEG_FIND_ENCODER_FAILED;
				break;
			}

			encoder_ctx_ = avcodec_alloc_context3(encoder_);
			if (!encoder_ctx_) {
				err = AE_FFMPEG_ALLOC_CONTEXT_FAILED;
				break;
			}

			encoder_ctx_->channels = nb_channels;
			encoder_ctx_->channel_layout = av_get_default_channel_layout(nb_channels);
			encoder_ctx_->sample_rate = sample_rate;
			encoder_ctx_->sample_fmt = fmt;
			encoder_ctx_->bit_rate = bit_rate;

			encoder_ctx_->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
			encoder_ctx_->time_base.den = sample_rate;
			encoder_ctx_->time_base.num = 1;
			encoder_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

			ret = avcodec_open2(encoder_ctx_, encoder_, NULL);
			if (ret < 0) {
				err = AE_FFMPEG_OPEN_CODEC_FAILED;
				break;
			}

			buff_size_ = av_samples_get_buffer_size(NULL, nb_channels, encoder_ctx_->frame_size, fmt, 1);
			buff_ = (uint8_t*)av_malloc(buff_size_);

			frame_ = av_frame_alloc();
			if (!frame_) {
				err = AE_FFMPEG_ALLOC_FRAME_FAILED;
				break;
			}

			frame_->channels = nb_channels;
			frame_->nb_samples = encoder_ctx_->frame_size;
			frame_->channel_layout = av_get_default_channel_layout(nb_channels);
			frame_->format = fmt;
			frame_->sample_rate = sample_rate;

			ret = avcodec_fill_audio_frame(frame_, nb_channels, fmt, buff_, buff_size_, 0);


			inited_ = true;

		} while (0);

		if (err != AE_NO) {
			std::cout << err2str(err) << "error: " << ret << std::endl;
			CleanUp();
		}

		return err;
	}

	int EncoderAAC::get_extradata_size()
	{
		return encoder_ctx_->extradata_size;
	}

	const uint8_t* EncoderAAC::get_extradata()
	{
		return (const uint8_t*) encoder_ctx_->extradata;
	}

	int EncoderAAC::get_nb_samples()
	{
		return encoder_ctx_->frame_size;
	}

	int EncoderAAC::Start()
	{
		int error = AE_NO;

		if (running_ == true) {
			return error;
		}

		if (inited_ == false) {
			return AE_NEED_INIT;
		}

		running_ = true;
		thread_ = std::thread(std::bind(&EncoderAAC::EncodeLoop, this));

		return error;
	}

	void EncoderAAC::Stop()
	{
		running_ = false;

		cond_notify_ = true;
		cond_var_.notify_all();

		if (thread_.joinable())
			thread_.join();
	}

	int EncoderAAC::put(const uint8_t* data, int data_len, AVFrame* frame)
	{
		std::unique_lock<std::mutex> lock(mutex_);

		AVFrame frame_cp;
		memcpy(&frame_cp, frame, sizeof(AVFrame));

		ring_buffer_->put(data, data_len, frame_cp);

		cond_notify_ = true;
		cond_var_.notify_all();
		return 0;
	}

	const AVRational& EncoderAAC::get_time_base()
	{
		return encoder_ctx_->time_base;
	}

	int EncoderAAC::Encode(AVFrame* frame, AVPacket* packet)
	{
		int ret = avcodec_send_frame(encoder_ctx_, frame_);
		if (ret < 0) {
			return AE_FFMPEG_ENCODE_FRAME_FAILED;
		}

		while (ret >= 0) {
			av_init_packet(packet);

			ret = avcodec_receive_packet(encoder_ctx_, packet);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
				break;
			}

			if (ret < 0) {
				return AE_FFMPEG_READ_PACKET_FAILED;
			}

			//al_debug("AP:%lld", packet->pts);

			if (ret == 0 && on_data_)
				on_data_(packet);

			av_packet_unref(packet);
		}

		return AE_NO;
	}

	void EncoderAAC::EncodeLoop()
	{
		int len = 0;
		int error = AE_NO;

		AVPacket* packet = av_packet_alloc();
		AVFrame pcm_frame;


		while (running_)
		{
			std::unique_lock<std::mutex> lock(mutex_);
			while (!cond_notify_ && running_)
				cond_var_.wait_for(lock, std::chrono::milliseconds(300));

			while ((len = ring_buffer_->get(buff_, buff_size_, pcm_frame))) {

				frame_->pts = pcm_frame.pts;
				frame_->pkt_pts = pcm_frame.pkt_pts;
				frame_->pkt_dts = pcm_frame.pkt_dts;

				if ((error = Encode(frame_, packet)) != AE_NO) {
					if (on_error_)
						on_error_(error);

					std::cout << "read aac packet failed:" << error << std::endl;

					break;
				}
			}

			cond_notify_ = false;
		}

		//flush pcm data in encoder
		Encode(NULL, packet);

		av_packet_free(&packet);

	}

	void EncoderAAC::CleanUp()
	{
		if (encoder_) {
			avcodec_close(encoder_ctx_);
		}

		encoder_ = NULL;

		if (encoder_ctx_) {
			avcodec_free_context(&encoder_ctx_);
		}

		if (frame_)
			av_free(frame_);
		frame_ = NULL;

		if (buff_)
			av_free(buff_);

		buff_ = NULL;

		encoder_ctx_ = NULL;

	}
}