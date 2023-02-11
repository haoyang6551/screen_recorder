#ifndef FILTER_AUDIO
#define FILTER_AUDIO

#include <thread>
#include <atomic>
#include <functional>
#include <string>
#include <mutex>
#include <condition_variable>

#include "headers_ffmpeg.h"

namespace am {
	typedef struct {
		AVFilterContext* ctx;
		AVFilterInOut* inout;

		AVRational time_base;
		int sample_rate;
		AVSampleFormat sample_fmt;
		int nb_channel;
		int64_t channel_layout;
	}FILTER_CTX;

	typedef std::function<void(AVFrame*)> FuncFilterData;
	typedef std::function<void(int)> FuncFilterError;

	class FilterAudio
	{
	public:
		FilterAudio();
		~FilterAudio();

		int Init(const FILTER_CTX& ctx_in0, const FILTER_CTX& ctx_in1, const FILTER_CTX& ctx_out);

		inline void registe_cb(FuncFilterData cb_on_filter_data, FuncFilterError cb_on_filter_error) {
			on_filter_data_ = cb_on_filter_data;
			on_filter_error_ = cb_on_filter_error;
		}

		int Start();

		int Stop();

		int add_frame(AVFrame* frame, int index);

		const AVRational& get_time_base();

	private:
		void CleanUp();
		void FilterLoop();



	private:
		FILTER_CTX ctx_in_0_;
		FILTER_CTX ctx_in_1_;
		FILTER_CTX ctx_out_;

		AVFilterGraph* filter_graph_;

		FuncFilterData on_filter_data_;
		FuncFilterError on_filter_error_;

		std::atomic_bool inited_;
		std::atomic_bool running_;

		std::thread thread_;

		std::mutex mutex_;
		std::condition_variable cond_var_;
		bool cond_notify_;
	};

}

#endif
