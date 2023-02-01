#pragma once

#include "record_audio.h"

namespace am {

	class RecordAudioDshow :public RecordAudio
	{
	public:
		RecordAudioDshow();
		~RecordAudioDshow();

		virtual int Init(const std::string& device_name,
			const std::string& device_id,
			bool is_input) override;

		virtual int Start() override;

		virtual int Pause() override;

		virtual int Resume() override;

		virtual int Stop() override;

		virtual const AVRational& get_time_base() override;
		virtual int64_t get_start_time() override;

	private:
		int Decode(AVFrame* frame, AVPacket* packet);
		void RecordLoop();
		void CleanUp();
	private:
		AVFormatContext* fmt_ctx_;
		AVInputFormat* input_fmt_;
		AVCodecContext* codec_ctx_;
		AVCodec* codec_;

		int stream_index_;
	};

}