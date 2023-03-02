#include "mux_mp4.h"
#include "mux_define.h"

#include "record_desktop.h"
#include "encode_264.h"
#include "sws_video.h"

#include "record_audio.h"
#include "encode_aac.h"
#include "resample_audio.h"
#include "filter_audio.h"

#include "ring_buffer.h"

#include "error_define.h"


namespace am {

	MuxMP4::MuxMP4()
	{
		av_register_all();

		have_a_ = false;
		have_v_ = false;

		output_file_ = "";

		v_stream_ = NULL;
		a_stream_ = NULL;

		fmt_ = NULL;
		fmt_ctx_ = NULL;

		inited_ = false;
		running_ = false;
		paused_ = false;

		base_time_ = -1;
	}

	MuxMP4::~MuxMP4()
	{
		Stop();
		CleanUp();
	}

	int MuxMP4::Init(
		const char* output_file,
		RecordDesktop* source_desktop,
		RecordAudio** source_audios,
		const int source_audios_nb,
		const MuxSetting& setting
	)
	{
		int error = AE_NO;
		int ret = 0;

		do {
			error = alloc_oc(output_file, setting);
			if (error != AE_NO)
				break;

			if (fmt_->video_codec != AV_CODEC_ID_NONE) {
				error = add_video_stream(setting, source_desktop);
				if (error != AE_NO)
					break;
			}

			if (fmt_->audio_codec != AV_CODEC_ID_NONE) {
				error = add_audio_stream(setting, source_audios, source_audios_nb);
				if (error != AE_NO)
					break;
			}

			error = open_output(output_file, setting);
			if (error != AE_NO)
				break;

			av_dump_format(fmt_ctx_, 0, NULL, 1);

			inited_ = true;

		} while (0);

		if (error != AE_NO) {
			CleanUp();
			std::cout << "muxer mp4 initialize failed:" << err2str(error) << ret << std::endl;
		}

		return error;
	}

	int MuxMP4::Start()
	{
		std::lock_guard<std::mutex> lock(mutex_);
		
		int error = AE_NO;

		if (running_ == true) {
			return AE_NO;
		}

		if (inited_ == false) {
			return AE_NEED_INIT;
		}

		base_time_ = -1;

		if (v_stream_ && v_stream_->v_enc_)
			v_stream_->v_enc_->Start();

		if (a_stream_ && a_stream_->a_enc_)
			a_stream_->a_enc_->Start();

		if (a_stream_ && a_stream_->a_filter_)
			a_stream_->a_filter_->Start();

		if (a_stream_ && a_stream_->a_src_) {
			for (int i = 0; i < a_stream_->a_nb_; i++) {
				if (a_stream_->a_src_[i])
					a_stream_->a_src_[i]->Start();
			}
		}

		if (v_stream_ && v_stream_->v_src_)
			v_stream_->v_src_->Start();

		running_ = true;

		return error;
	}

	int MuxMP4::Stop()
	{
		std::lock_guard<std::mutex> lock(mutex_);

		if (running_ == false)
			return AE_NO;

		running_ = false;

		std::cout << "try to stop muxer...." << std::endl;

		std::cout << "stop audio recorder..." << std::endl;
		if (a_stream_ && a_stream_->a_src_) {
			for (int i = 0; i < a_stream_->a_nb_; i++) {
				a_stream_->a_src_[i]->Stop();
			}
		}

		std::cout << "stop video recorder..." << std::endl;
		if (v_stream_ && v_stream_->v_src_)
			v_stream_->v_src_->Stop();

		std::cout << "stop audio filter..." << std::endl;
		if (a_stream_ && a_stream_->a_filter_)
			a_stream_->a_filter_->Stop();


		std::cout << "stop video encoder..." << std::endl;
		if (v_stream_ && v_stream_->v_enc_)
			v_stream_->v_enc_->Stop();

		std::cout << "stop audio encoder..." << std::endl;
		if (a_stream_ && a_stream_->a_enc_) 
			a_stream_->a_enc_->Stop();

		std::cout << "write mp4 trailer..." << std::endl;
		if (fmt_ctx_)
			av_write_trailer(fmt_ctx_);//must write trailer ,otherwise mp4 can not play

		std::cout << "muxer stopped..." << std::endl;

		return AE_NO;
	}

	int MuxMP4::Pause()
	{
		paused_ = true;

		return 0;
	}

	int MuxMP4::Resume()
	{
		paused_ = false;
		return 0;
	}

	void MuxMP4::on_desktop_data(AVFrame* frame)
	{
		if (running_ == false || !v_stream_ || !v_stream_->v_enc_ || !v_stream_->v_sws_) {
			return;
		}

		int len = 0, ret = AE_NO;
		uint8_t* yuv_data = NULL;

		ret = v_stream_->v_sws_->Convert(frame, &yuv_data, &len);

		if (ret == AE_NO && yuv_data && len) {
			v_stream_->v_enc_->put(yuv_data, len, frame);
		}
	}

	void MuxMP4::on_desktop_error(int error)
	{
		std::cout << "on desktop capture error:" << error << std::endl;
	}

	void MuxMP4::on_audio_data(AVFrame* frame, int index)
	{
		if (running_ == false
			|| !a_stream_
			|| !a_stream_->a_samples_
			|| !a_stream_->a_samples_[index]
			|| !a_stream_->a_resamples_
			|| !a_stream_->a_resamples_[index]
			|| !a_stream_->a_rs_
			|| !a_stream_->a_rs_[index])
			return;

		a_stream_->a_filter_->add_frame(frame, index);

		return;
	}

	void MuxMP4::on_audio_error(int error, int index)
	{
		std::cout << "on audio capture error:%d with stream index:" << error << index << std::endl;
	}

	void MuxMP4::on_filter_audio_data(AVFrame* frame)
	{
		if (running_ == false || !a_stream_->a_enc_)
			return;


		AudioSample* resamples = a_stream_->a_resamples_[0];

		int copied_len = 0;
		int sample_len = av_samples_get_buffer_size(frame->linesize, frame->channels, frame->nb_samples, (AVSampleFormat)frame->format, 1);
		sample_len = av_samples_get_buffer_size(NULL, frame->channels, frame->nb_samples, (AVSampleFormat)frame->format, 1);

		int remain_len = sample_len;

		//for data is planar,should copy data[0] data[1] to correct buff pos
		if (av_sample_fmt_is_planar((AVSampleFormat)frame->format) == 0) {
			while (remain_len > 0) {
				//cache pcm
				copied_len = min(resamples->size_ - resamples->sample_in_, remain_len);
				if (copied_len) {
					memcpy(resamples->buff_ + resamples->sample_in_, frame->data[0] + sample_len - remain_len, copied_len);
					resamples->sample_in_ += copied_len;
					remain_len = remain_len - copied_len;
				}

				//got enough pcm to encoder,resample and mix
				if (resamples->sample_in_ == resamples->size_) {
					a_stream_->a_enc_->put(resamples->buff_, resamples->size_, frame);

					resamples->sample_in_ = 0;
				}
			}
		}
		else {//resample size is channels*frame->linesize[0],for 2 channels
			while (remain_len > 0) {
				copied_len = min(resamples->size_ - resamples->sample_in_, remain_len);
				if (copied_len) {
					memcpy(resamples->buff_ + resamples->sample_in_ / 2, frame->data[0] + (sample_len - remain_len) / 2, copied_len / 2);
					memcpy(resamples->buff_ + resamples->size_ / 2 + resamples->sample_in_ / 2, frame->data[1] + (sample_len - remain_len) / 2, copied_len / 2);
					resamples->sample_in_ += copied_len;
					remain_len = remain_len - copied_len;
				}

				if (resamples->sample_in_ == resamples->size_) {
					a_stream_->a_enc_->put(resamples->buff_, resamples->size_, frame);

					resamples->sample_in_ = 0;
				}
			}
		}
	}

	void MuxMP4::on_filter_audio_error(int error)
	{
		std::cout << "on filter audio error:" << error << std::endl;
	}

	void MuxMP4::on_enc_264_data(AVPacket* packet)
	{
		if (running_ && v_stream_) {
			write_video(packet);
		}
	}

	void MuxMP4::on_enc_264_error(int error)
	{
		std::cout << "on desktop encode error: " << error << std::endl;
	}

	void MuxMP4::on_enc_aac_data(AVPacket* packet)
	{
		if (running_ && a_stream_) {
			write_audio(packet);
		}
	}

	void MuxMP4::on_enc_aac_error(int error)
	{
		std::cout << "on audio encode error: " << error << std::endl;
	}

	int MuxMP4::alloc_oc(const char* output_file, const MuxSetting& setting)
	{
		output_file_ = std::string(output_file);

		int error = AE_NO;
		int ret = 0;

		do {
			ret = avformat_alloc_output_context2(&fmt_ctx_, NULL, NULL, output_file);
			if (ret < 0 || !fmt_ctx_) {
				error = AE_FFMPEG_ALLOC_CONTEXT_FAILED;
				break;
			}

			fmt_ = fmt_ctx_->oformat;
		} while (0);

		return error;
	}

	int MuxMP4::add_video_stream(const MuxSetting& setting, RecordDesktop* source_desktop)
	{
		int error = AE_NO;
		int ret = 0;

		v_stream_ = new MuxStream();
		memset(v_stream_, 0, sizeof(MuxStream));

		v_stream_->v_src_ = source_desktop;

		v_stream_->pre_pts_ = -1;

		v_stream_->v_src_->registe_cb(
			std::bind(&MuxMP4::on_desktop_data, this, std::placeholders::_1),
			std::bind(&MuxMP4::on_desktop_error, this, std::placeholders::_1)
		);

		RecordDesktopRect v_rect = v_stream_->v_src_->get_rect();

		do {
			v_stream_->v_enc_ = new Encoder264();
			error = v_stream_->v_enc_->Init(setting.v_width_, setting.v_height_, setting.v_frame_rate_, setting.v_bit_rate_, setting.v_qb_);
			if (error != AE_NO)
				break;

			v_stream_->v_enc_->registe_cb(
				std::bind(&MuxMP4::on_enc_264_data, this, std::placeholders::_1),
				std::bind(&MuxMP4::on_enc_264_error, this, std::placeholders::_1)
			);

			v_stream_->v_sws_ = new SwsVideo();
			error = v_stream_->v_sws_->Init(
				v_stream_->v_src_->get_pixel_fmt(),
				v_rect.right_ - v_rect.left_,
				v_rect.bottom_ - v_rect.top_,
				AV_PIX_FMT_YUV420P,
				setting.v_width_,
				setting.v_height_
			);
			if (error != AE_NO)
				break;

			AVCodec* codec = avcodec_find_encoder(fmt_->video_codec);
			if (!codec) {
				error = AE_FFMPEG_FIND_ENCODER_FAILED;
				break;
			}

			AVStream* st = avformat_new_stream(fmt_ctx_, codec);
			if (!st) {
				error = AE_FFMPEG_NEW_STREAM_FAILED;
				break;
			}

			st->codec->codec_id = AV_CODEC_ID_H264;
			st->codec->bit_rate_tolerance = setting.v_bit_rate_;
			st->codec->codec_type = AVMEDIA_TYPE_VIDEO;
			st->codec->time_base.den = setting.v_frame_rate_;
			st->codec->time_base.num = 1;
			st->codec->pix_fmt = AV_PIX_FMT_YUV420P;

			st->codec->coded_width = setting.v_width_;
			st->codec->coded_height = setting.v_height_;
			st->codec->width = setting.v_width_;
			st->codec->height = setting.v_height_;
			st->codec->max_b_frames = 0;//NO B Frame
			st->time_base = { 1,90000 };//fixed?
			st->avg_frame_rate = av_inv_q(st->codec->time_base);

			if (fmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER) {//without this,normal player can not play,extradata will write with avformat_write_header
				st->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

				st->codec->extradata_size = v_stream_->v_enc_->get_extradata_size();// +AV_INPUT_BUFFER_PADDING_SIZE;
				st->codec->extradata = (uint8_t*)av_memdup(v_stream_->v_enc_->get_extradata(), v_stream_->v_enc_->get_extradata_size());
			}

			v_stream_->st_ = st;

			v_stream_->setting_ = setting;
			v_stream_->filter_ = av_bitstream_filter_init("h264_mp4toannexb");
		} while (0);

		return error;
	}

	int MuxMP4::add_audio_stream(const MuxSetting& setting, RecordAudio** source_audios, const int source_audios_nb)
	{
		int error = AE_NO;
		int ret = 0;

		a_stream_ = new MuxStream();
		memset(a_stream_, 0, sizeof(MuxStream));

		a_stream_->a_nb_ = source_audios_nb;
		a_stream_->a_rs_ = new ResampleAudio * [a_stream_->a_nb_];
		a_stream_->a_resamples_ = new AudioSample * [a_stream_->a_nb_];
		a_stream_->a_samples_ = new AudioSample * [a_stream_->a_nb_];
		a_stream_->a_src_ = new RecordAudio * [a_stream_->a_nb_];
		a_stream_->pre_pts_ = -1;


		do {
			a_stream_->a_enc_ = new EncoderAAC();
			error = a_stream_->a_enc_->Init(
				setting.a_nb_channel_,
				setting.a_sample_rate_,
				setting.a_sample_fmt_,
				setting.a_bit_rate_
			);
			if (error != AE_NO)
				break;

			a_stream_->a_enc_->registe_cb(
				std::bind(&MuxMP4::on_enc_aac_data, this, std::placeholders::_1),
				std::bind(&MuxMP4::on_enc_aac_error, this, std::placeholders::_1)
			);

			for (int i = 0; i < a_stream_->a_nb_; i++) {

				a_stream_->a_src_[i] = source_audios[i];
				a_stream_->a_src_[i]->registe_cb(
					std::bind(&MuxMP4::on_audio_data, this, std::placeholders::_1, std::placeholders::_2),
					std::bind(&MuxMP4::on_audio_error, this, std::placeholders::_1, std::placeholders::_2),
					i
				);

				SAMPLE_SETTING src_setting = {
					a_stream_->a_enc_->get_nb_samples(),
					av_get_default_channel_layout(a_stream_->a_src_[i]->get_channel_num()),
					a_stream_->a_src_[i]->get_channel_num(),
					a_stream_->a_src_[i]->get_fmt(),
					a_stream_->a_src_[i]->get_sample_rate()
				};
				SAMPLE_SETTING dst_setting = {
					a_stream_->a_enc_->get_nb_samples(),
					av_get_default_channel_layout(setting.a_nb_channel_),
					setting.a_nb_channel_,
					setting.a_sample_fmt_,
					setting.a_sample_rate_
				};

				a_stream_->a_rs_[i] = new ResampleAudio();
				a_stream_->a_resamples_[i] = new AudioSample({ NULL,0,0 });
				a_stream_->a_rs_[i]->Init(&src_setting, &dst_setting, &a_stream_->a_resamples_[i]->size_);
				a_stream_->a_resamples_[i]->buff_ = new uint8_t[a_stream_->a_resamples_[i]->size_];

				a_stream_->a_samples_[i] = new AudioSample({ NULL,0,0 });
				a_stream_->a_samples_[i]->size_ = av_samples_get_buffer_size(NULL, src_setting.nb_channels, src_setting.nb_samples, src_setting.fmt, 1);
				a_stream_->a_samples_[i]->buff_ = new uint8_t[a_stream_->a_samples_[i]->size_];
			}

			a_stream_->a_filter_ = new am::FilterAudio();
			error = a_stream_->a_filter_->Init(
				{
					NULL,NULL,
					a_stream_->a_src_[0]->get_time_base(),
					a_stream_->a_src_[0]->get_sample_rate(),
					a_stream_->a_src_[0]->get_fmt(),
					a_stream_->a_src_[0]->get_channel_num(),
					av_get_default_channel_layout(a_stream_->a_src_[0]->get_channel_num())
				},
			{
				NULL,NULL,
				a_stream_->a_src_[1]->get_time_base(),
				a_stream_->a_src_[1]->get_sample_rate(),
				a_stream_->a_src_[1]->get_fmt(),
				a_stream_->a_src_[1]->get_channel_num(),
				av_get_default_channel_layout(a_stream_->a_src_[1]->get_channel_num())
			},
			{
				NULL,NULL,
				{ 1,AV_TIME_BASE },
				setting.a_sample_rate_,
				setting.a_sample_fmt_,
				setting.a_nb_channel_,
				av_get_default_channel_layout(setting.a_nb_channel_)
			}
			);

			if (error != AE_NO) {
				break;
			}

			a_stream_->a_filter_->registe_cb(
				std::bind(&MuxMP4::on_filter_audio_data, this, std::placeholders::_1),
				std::bind(&MuxMP4::on_filter_audio_error, this, std::placeholders::_1)
			);

			AVCodec* codec = avcodec_find_encoder(fmt_->audio_codec);
			if (!codec) {
				error = AE_FFMPEG_FIND_ENCODER_FAILED;
				break;
			}

			AVStream* st = avformat_new_stream(fmt_ctx_, codec);
			if (!st) {
				error = AE_FFMPEG_NEW_STREAM_FAILED;
				break;
			}

			st->time_base = { 1,setting.a_sample_rate_ };

			st->codec->bit_rate = setting.a_bit_rate_;
			st->codec->channels = setting.a_nb_channel_;
			st->codec->sample_rate = setting.a_sample_rate_;
			st->codec->sample_fmt = setting.a_sample_fmt_;
			st->codec->time_base = { 1,setting.a_sample_rate_ };
			st->codec->channel_layout = av_get_default_channel_layout(setting.a_nb_channel_);

			if (fmt_ctx_->oformat->flags & AVFMT_GLOBALHEADER) {//without this,normal player can not play
				st->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

				st->codec->extradata_size = a_stream_->a_enc_->get_extradata_size();// +AV_INPUT_BUFFER_PADDING_SIZE;
				st->codec->extradata = (uint8_t*)av_memdup(a_stream_->a_enc_->get_extradata(), a_stream_->a_enc_->get_extradata_size());
			}

			a_stream_->st_ = st;

			a_stream_->setting_ = setting;
			a_stream_->filter_ = av_bitstream_filter_init("aac_adtstoasc");

		} while (0);

		return error;
	}

	int MuxMP4::open_output(const char* output_file, const MuxSetting& setting)
	{
		int error = AE_NO;
		int ret = 0;

		do {
			if (!(fmt_->flags & AVFMT_NOFILE)) {
				ret = avio_open(&fmt_ctx_->pb, output_file, AVIO_FLAG_WRITE);
				if (ret < 0) {
					error = AE_FFMPEG_OPEN_IO_FAILED;
					break;
				}
			}

			AVDictionary* opt = NULL;
			av_dict_set_int(&opt, "video_track_timescale", v_stream_->setting_.v_frame_rate_, 0);

			//ret = avformat_write_header(_fmt_ctx, &opt);//no need to set this
			ret = avformat_write_header(fmt_ctx_, NULL);

			av_dict_free(&opt);

			if (ret < 0) {
				error = AE_FFMPEG_WRITE_HEADER_FAILED;
				break;
			}
		} while (0);

		return error;
	}

	void MuxMP4::CleanUpVideo()
	{
		if (!v_stream_)
			return;

		if (v_stream_->v_enc_)
			delete v_stream_->v_enc_;

		if (v_stream_->v_sws_)
			delete v_stream_->v_sws_;

		delete v_stream_;

		v_stream_ = nullptr;
	}

	void MuxMP4::CleanUpAudio()
	{
		if (!a_stream_)
			return;

		if (a_stream_->a_enc_)
			delete a_stream_->a_enc_;

		if (a_stream_->a_filter_)
			delete a_stream_->a_filter_;

		if (a_stream_->a_nb_) {
			for (int i = 0; i < a_stream_->a_nb_; i++) {
				if (a_stream_->a_rs_ && a_stream_->a_rs_[i])
					delete a_stream_->a_rs_[i];

				if (a_stream_->a_samples_ && a_stream_->a_samples_[i]) {
					delete[] a_stream_->a_samples_[i]->buff_;
					delete a_stream_->a_samples_[i];
				}

				if (a_stream_->a_resamples_ && a_stream_->a_resamples_[i]) {
					delete[] a_stream_->a_resamples_[i]->buff_;
					delete a_stream_->a_resamples_[i];
				}
			}

			if (a_stream_->a_rs_)
				delete[] a_stream_->a_rs_;

			if (a_stream_->a_samples_)
				delete[] a_stream_->a_samples_;

			if (a_stream_->a_resamples_)
				delete[] a_stream_->a_resamples_;
		}

		delete a_stream_;

		a_stream_ = nullptr;
	}

	void MuxMP4::CleanUp()
	{
		CleanUpVideo();
		CleanUpAudio();

		if (fmt_ && !(fmt_->flags & AVFMT_NOFILE))
			avio_closep(&fmt_ctx_->pb);

		if (fmt_ctx_) {
			avformat_free_context(fmt_ctx_);
		}

		fmt_ctx_ = NULL;
		fmt_ = NULL;

		inited_ = false;
	}

	uint64_t MuxMP4::get_current_time()
	{
		std::lock_guard<std::mutex> lock(time_mutex_);

		return av_gettime_relative();
	}

	int MuxMP4::write_video(AVPacket* packet)
	{
		//must lock here,coz av_interleaved_write_frame will push packet into a queue,and is not thread safe
		std::lock_guard<std::mutex> lock(mutex_);

		if (paused_) return AE_NO;

		//if (a_stream_->pre_pts == (uint64_t)-1)
		//	return 0;

		packet->stream_index = v_stream_->st_->index;

		if (v_stream_->pre_pts_ == (uint64_t)-1) {
			v_stream_->pre_pts_ = packet->pts;
		}

		packet->pts = packet->pts - v_stream_->pre_pts_;
		//packet->pts = av_rescale_q_rnd(packet->pts, {1,AV_TIME_BASE}, v_stream_->st->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
		packet->pts = av_rescale_q_rnd(packet->pts, v_stream_->v_src_->get_time_base(), v_stream_->st_->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));


		packet->dts = packet->pts;//make sure that dts is equal to pts


		//al_debug("V:%lld", packet->pts);

#if 0
		static FILE* fp = NULL;
		if (fp == NULL) {
			fp = fopen("..\\..\\save.264", "wb+");
			//write sps pps
			fwrite(v_stream_->v_enc->get_extradata(), 1, v_stream_->v_enc->get_extradata_size(), fp);
		}

		fwrite(packet->data, 1, packet->size, fp);

		fflush(fp);
#endif

		av_assert0(packet->data != NULL);

		int ret = av_interleaved_write_frame(fmt_ctx_, packet);//no need to unref packet,this will be auto unref

	}

	int MuxMP4::write_audio(AVPacket* packet)
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (paused_) return AE_NO;


		packet->stream_index = a_stream_->st_->index;

		if (a_stream_->pre_pts_ == (uint64_t)-1) {
			a_stream_->pre_pts_ = packet->pts;
		}

		packet->pts = packet->pts - a_stream_->pre_pts_;
		packet->pts = av_rescale_q(packet->pts, a_stream_->a_filter_->get_time_base(), { 1,AV_TIME_BASE });
		packet->pts = av_rescale_q_rnd(packet->pts, { 1,AV_TIME_BASE }, a_stream_->st_->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

		packet->dts = packet->pts;//make sure that dts is equal to pts
		//al_debug("A:%lld %lld", packet->pts, packet->dts);

		av_assert0(packet->data != NULL);

		int ret = av_interleaved_write_frame(fmt_ctx_, packet);//no need to unref packet,this will be auto unref

		return ret;
	}
}