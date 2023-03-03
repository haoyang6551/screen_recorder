#include "encode_264.h"

#include "ring_buffer.h"

#include "error_define.h"

namespace am {

	Encoder264::Encoder264()
	{
		av_register_all();

		inited_ = false;
		running_ = false;

		encoder_ = NULL;
		encoder_ctx_ = NULL;
		frame_ = NULL;
		buff_ = NULL;
		buff_size_ = 0;
		y_size_ = 0;

		cond_notify_ = false;

		ring_buffer_ = new RingBuffer<AVFrame>();
	}

	Encoder264::~Encoder264()
	{
		Stop();

		CleanUp();

		if (ring_buffer_)
			delete ring_buffer_;
	}

	int Encoder264::Init(int pic_width, int pic_height, int frame_rate, int bit_rate, int qb, int gop_size)
	{
		if (inited_ == true)
			return AE_NO;

		int err = AE_NO;
		int ret = 0;

		AVDictionary* options = 0;

		av_dict_set(&options, "preset", "superfast", 0);
		av_dict_set(&options, "tune", "zerolatency", 0);

		do {
			encoder_ = avcodec_find_encoder(AV_CODEC_ID_H264);
			if (!encoder_) {
				err = AE_FFMPEG_FIND_ENCODER_FAILED;
				break;
			}

			encoder_ctx_ = avcodec_alloc_context3(encoder_);
			if (!encoder_ctx_) {
				err = AE_FFMPEG_ALLOC_CONTEXT_FAILED;
				break;
			}

			encoder_ctx_->codec_id = AV_CODEC_ID_H264;
			encoder_ctx_->codec_type = AVMEDIA_TYPE_VIDEO;
			encoder_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
			encoder_ctx_->width = pic_width;
			encoder_ctx_->height = pic_height;
			encoder_ctx_->time_base.num = 1;
			encoder_ctx_->time_base.den = frame_rate;
			encoder_ctx_->framerate = { frame_rate,1 };
			encoder_ctx_->bit_rate = bit_rate;
			encoder_ctx_->gop_size = gop_size;

			encoder_ctx_->qmin = 20;
			encoder_ctx_->qmax = 40;
			int qb_float = (encoder_ctx_->qmax - encoder_ctx_->qmin) * qb / 100;
			encoder_ctx_->qmin = encoder_ctx_->qmin + qb_float;
			encoder_ctx_->qmax = encoder_ctx_->qmax - qb_float;
			encoder_ctx_->max_b_frames = 0;//NO B Frame
			encoder_ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

			ret = avcodec_open2(encoder_ctx_, encoder_, &options);
			if (ret != 0) {
				err = AE_FFMPEG_OPEN_CODEC_FAILED;
				break;
			}

			frame_ = av_frame_alloc();

			buff_size_ = av_image_get_buffer_size(encoder_ctx_->pix_fmt, encoder_ctx_->width, encoder_ctx_->height, 1);

			buff_ = (uint8_t*)av_malloc(buff_size_);
			if (!buff_) {
				break;
			}

			av_image_fill_arrays(frame_->data, frame_->linesize, buff_, encoder_ctx_->pix_fmt, encoder_ctx_->width, encoder_ctx_->height, 1);

			frame_->format = encoder_ctx_->pix_fmt;
			frame_->width = encoder_ctx_->width;
			frame_->height = encoder_ctx_->height;

			y_size_ = encoder_ctx_->width * encoder_ctx_->height;

			inited_ = true;
		} while (0);

		if (err != AE_NO) {
			std::cout << err2str(err) << "error is " <<  ret << " " << GetLastError() << std::endl;
			CleanUp();
		}

		if (options)
			av_dict_free(&options);

		return err;
	}

	int Encoder264::get_extradata_size()
	{
		return encoder_ctx_->extradata_size;
	}

	const uint8_t* Encoder264::get_extradata()
	{
		return (const uint8_t*) encoder_ctx_->extradata;
	}

	const AVRational& Encoder264::get_time_base()
	{
		return encoder_ctx_->time_base;
	}

	int Encoder264::Start()
	{
		int error = AE_NO;

		if (running_ == true) {
			return error;
		}

		if (inited_ == false) {
			return AE_NEED_INIT;
		}

		running_ = true;
		thread_ = std::thread(&Encoder264::EncodeLoop, this);

		return error;
	}

	void Encoder264::Stop()
	{
		running_ = false;

		cond_notify_ = true;
		cond_var_.notify_all();

		if (thread_.joinable())
			thread_.join();

	}

	int Encoder264::put(const uint8_t* data, int data_len, AVFrame* frame)
	{
		std::unique_lock<std::mutex> lock(mutex_);

		AVFrame frame_cp;
		memcpy(&frame_cp, frame, sizeof(AVFrame));

		ring_buffer_->put(data, data_len, frame_cp);

		cond_notify_ = true;
		cond_var_.notify_all();
		return 0;
	}

	void Encoder264::CleanUp()
	{
		if (frame_)
			av_free(frame_);
		frame_ = NULL;

		if (buff_)
			av_free(buff_);

		buff_ = NULL;

		if (encoder_)
			avcodec_close(encoder_ctx_);

		encoder_ = NULL;

		if (encoder_ctx_)
			avcodec_free_context(&encoder_ctx_);

		encoder_ctx_ = NULL;
	}

	int Encoder264::Encode(AVFrame* frame, AVPacket* packet)
	{
		int ret = avcodec_send_frame(encoder_ctx_, frame);
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

			if (ret == 0 && on_data_)
				on_data_(packet);

			av_packet_unref(packet);
		}

		return AE_NO;
	}

	void Encoder264::EncodeLoop()
	{
		AVPacket* packet = av_packet_alloc();
		AVFrame yuv_frame;

		int error = AE_NO;

		while (running_)
		{
			std::unique_lock<std::mutex> lock(mutex_);
			while (!cond_notify_ && running_)
				cond_var_.wait(lock);

			while (ring_buffer_->get(buff_, buff_size_, yuv_frame)) {
				frame_->pkt_dts = yuv_frame.pkt_dts;
				frame_->pkt_dts = yuv_frame.pkt_dts;
				frame_->pts = yuv_frame.pts;

				if ((error = Encode(frame_, packet)) != AE_NO) {
					if (on_error_)
						on_error_(error);

					if (error)
						std::cout << "encode 264 packet failed:%d" << std::endl;

					break;
				}
			}

			cond_notify_ = false;
		}

		//flush frame in encoder
		Encode(NULL, packet);

		av_packet_free(&packet);
	}

}