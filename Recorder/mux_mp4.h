#ifndef MUXER_MP4
#define MUXER_MP4

#include <atomic>
#include <thread>
#include <list>
#include <functional>
#include <math.h>
#include <mutex>

#include "mux_file.h"

#include "headers_ffmpeg.h"

namespace am {

	class MuxMP4 : public MuxFile
	{
	public:
		MuxMP4();
		~MuxMP4();

		int Init(
			const char* output_file,
			RecordDesktop* source_desktop,
			RecordAudio** source_audios,
			const int source_audios_nb,
			const MuxSetting& setting
		);

		int Start();
		int Stop();

		int Pause();
		int Resume();

	private:
		void on_desktop_data(AVFrame* frame);

		void on_desktop_error(int error);

		void on_audio_data(AVFrame* frame, int index);

		void on_audio_error(int error, int index);

		void on_filter_audio_data(AVFrame* frame);

		void on_filter_audio_error(int error);



		void on_enc_264_data(AVPacket* packet);

		void on_enc_264_error(int error);

		void on_enc_aac_data(AVPacket* packet);

		void on_enc_aac_error(int error);



		int alloc_oc(const char* output_file, const MuxSetting& setting);

		int add_video_stream(const MuxSetting& setting, RecordDesktop* source_desktop);

		int add_audio_stream(const MuxSetting& setting, RecordAudio** source_audios, const int source_audios_nb);

		int open_output(const char* output_file, const MuxSetting& setting);

		void CleanUpVideo();
		void CleanUpAudio();
		void CleanUp();

		uint64_t get_current_time();

		int write_video(AVPacket* packet);

		int write_audio(AVPacket* packet);

	private:
		std::atomic_bool inited_;
		std::atomic_bool running_;
		std::atomic_bool paused_;

		bool have_v_, have_a_;

		std::string output_file_;

		MuxStream* v_stream_;
		MuxStream* a_stream_;

		AVOutputFormat* fmt_;
		AVFormatContext* fmt_ctx_;

		int64_t base_time_;

		char ff_error_[4096];

		std::mutex mutex_;
		std::mutex time_mutex_;
	};
}



#endif
