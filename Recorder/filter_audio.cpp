#include "filter_audio.h"

#include <chrono>

#include "error_define.h"

namespace am {

	static void print_frame(const AVFrame* frame, int index)
	{
		std::cout << "index:" << index << frame->pts << frame->nb_samples << std::endl;
	}

	FilterAudio::FilterAudio()
	{
		av_register_all();
		avfilter_register_all();

		memset(&ctx_in_0_, 0, sizeof(FILTER_CTX));
		memset(&ctx_in_1_, 0, sizeof(FILTER_CTX));
		memset(&ctx_out_, 0, sizeof(FILTER_CTX));

		filter_graph_ = NULL;

		inited_ = false;
		running_ = false;

		cond_notify_ = false;

	}


	FilterAudio::~FilterAudio()
	{
		Stop();
		CleanUp();
	}

	static void format_pad_arg(char* arg, int size, const FILTER_CTX& ctx)
	{
		sprintf_s(arg, size, "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%I64x",
			ctx.time_base.num,
			ctx.time_base.den,
			ctx.sample_rate,
			av_get_sample_fmt_name(ctx.sample_fmt),
			av_get_default_channel_layout(ctx.nb_channel));
	}

	int FilterAudio::Init(const FILTER_CTX& ctx_in0, const FILTER_CTX& ctx_in1, const FILTER_CTX& ctx_out)
	{
		int error = AE_NO;
		int ret = 0;

		if (inited_) return AE_NO;

		do {
			ctx_in_0_ = ctx_in0;
			ctx_in_1_ = ctx_in1;
			ctx_out_ = ctx_out;

			filter_graph_ = avfilter_graph_alloc();
			if (!filter_graph_) {
				error = AE_FILTER_ALLOC_GRAPH_FAILED;
				break;
			}

			const std::string filter_desrc = "[in0][in1]amix=inputs=2:duration=first:dropout_transition=0[out]";

			ctx_in_0_.inout = avfilter_inout_alloc();
			ctx_in_1_.inout = avfilter_inout_alloc();
			ctx_out_.inout = avfilter_inout_alloc();

			char pad_args0[512] = { 0 }, pad_args1[512] = { 0 };

			format_pad_arg(pad_args0, 512, ctx_in_0_);
			format_pad_arg(pad_args1, 512, ctx_in_1_);

			ret = avfilter_graph_create_filter(&ctx_in_0_.ctx, avfilter_get_by_name("abuffer"), "in0", pad_args0, NULL, filter_graph_);
			if (ret < 0) {
				error = AE_FILTER_CREATE_FILTER_FAILED;
				break;
			}

			ret = avfilter_graph_create_filter(&ctx_in_1_.ctx, avfilter_get_by_name("abuffer"), "in1", pad_args1, NULL, filter_graph_);
			if (ret < 0) {
				error = AE_FILTER_CREATE_FILTER_FAILED;
				break;
			}

			ret = avfilter_graph_create_filter(&ctx_out_.ctx, avfilter_get_by_name("abuffersink"), "out", NULL, NULL, filter_graph_);
			if (ret < 0) {
				error = AE_FILTER_CREATE_FILTER_FAILED;
				break;
			}

			av_opt_set_bin(ctx_out_.ctx, "sample_fmts", (uint8_t*)&ctx_out_.sample_fmt, sizeof(ctx_out_.sample_fmt), AV_OPT_SEARCH_CHILDREN);
			av_opt_set_bin(ctx_out_.ctx, "channel_layouts", (uint8_t*)&ctx_out_.channel_layout, sizeof(ctx_out_.channel_layout), AV_OPT_SEARCH_CHILDREN);
			av_opt_set_bin(ctx_out_.ctx, "sample_rates", (uint8_t*)&ctx_out_.sample_rate, sizeof(ctx_out_.sample_rate), AV_OPT_SEARCH_CHILDREN);

			ctx_in_0_.inout->name = av_strdup("in0");
			ctx_in_0_.inout->filter_ctx = ctx_in_0_.ctx;
			ctx_in_0_.inout->pad_idx = 0;
			ctx_in_0_.inout->next = ctx_in_1_.inout;

			ctx_in_1_.inout->name = av_strdup("in1");
			ctx_in_1_.inout->filter_ctx = ctx_in_1_.ctx;
			ctx_in_1_.inout->pad_idx = 0;
			ctx_in_1_.inout->next = NULL;

			ctx_out_.inout->name = av_strdup("out");
			ctx_out_.inout->filter_ctx = ctx_out_.ctx;
			ctx_out_.inout->pad_idx = 0;
			ctx_out_.inout->next = NULL;

			AVFilterInOut* inoutputs[2] = { ctx_in_0_.inout,ctx_in_1_.inout };

			ret = avfilter_graph_parse_ptr(filter_graph_, filter_desrc.c_str(), &ctx_out_.inout, inoutputs, NULL);
			if (ret < 0) {
				error = AE_FILTER_PARSE_PTR_FAILED;
				break;
			}

			ret = avfilter_graph_config(filter_graph_, NULL);
			if (ret < 0) {
				error = AE_FILTER_CONFIG_FAILED;
				break;
			}


			inited_ = true;
		} while (0);

		if (error != AE_NO) {
			std::cout << "filter init failed" << err2str(error) << ret << std::endl;
			CleanUp();
		}

		//if (_ctx_in_0.inout)
		//	avfilter_inout_free(&_ctx_in_0.inout);

		//if (_ctx_in_1.inout)
		//	avfilter_inout_free(&_ctx_in_1.inout);

		//if (_ctx_out.inout)
		//	avfilter_inout_free(&_ctx_out.inout);

		return error;
	}

	int FilterAudio::Start()
	{
		if (!inited_)
			return AE_NEED_INIT;

		if (running_)
			return AE_NO;

		running_ = true;
		thread_ = std::thread(std::bind(&FilterAudio::FilterLoop, this));

		return 0;
	}

	int FilterAudio::Stop()
	{
		if (!inited_ || !running_)
			return AE_NO;

		running_ = false;

		cond_notify_ = true;
		cond_var_.notify_all();

		if (thread_.joinable())
			thread_.join();

		return AE_NO;
	}

	int FilterAudio::add_frame(AVFrame* frame, int index)
	{
		std::unique_lock<std::mutex> lock(mutex_);

		int error = AE_NO;
		int ret = 0;

		do {
			AVFilterContext* ctx = NULL;
			switch (index) {
			case 0:
				ctx = ctx_in_0_.ctx;
				break;
			case 1:
				ctx = ctx_in_1_.ctx;
				break;
			default:
				ctx = NULL;
				break;
			}

			if (!ctx) {
				error = AE_FILTER_INVALID_CTX_INDEX;
				break;
			}

			//print_frame(frame, index);
			int ret = av_buffersrc_add_frame_flags(ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
			if (ret < 0) {
				error = AE_FILTER_ADD_FRAME_FAILED;
				break;
			}

		} while (0);

		if (error != AE_NO) {
			std::cout << "add frame failed" << err2str(error) << ret << std::endl;
		}

		cond_notify_ = true;
		cond_var_.notify_all();

		return error;
	}

	const AVRational& FilterAudio::get_time_base()
	{
		return av_buffersink_get_time_base(ctx_out_.ctx);
	}

	void FilterAudio::CleanUp()
	{
		if (filter_graph_)
			avfilter_graph_free(&filter_graph_);

		memset(&ctx_in_0_, 0, sizeof(FILTER_CTX));
		memset(&ctx_in_1_, 0, sizeof(FILTER_CTX));
		memset(&ctx_out_, 0, sizeof(FILTER_CTX));

		inited_ = false;
	}

	void FilterAudio::FilterLoop()
	{
		AVFrame* frame = av_frame_alloc();

		int ret = 0;
		while (running_) {
			std::unique_lock<std::mutex> lock(mutex_);
			while (!cond_notify_ && running_)
				cond_var_.wait_for(lock, std::chrono::milliseconds(300));

			while (running_ && cond_notify_) {
				ret = av_buffersink_get_frame(ctx_out_.ctx, frame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					break;;
				}

				if (ret < 0) {
					std::cout << "avfilter get frame error:%d" << ret << std::endl;
					if (on_filter_error_) on_filter_error_(ret);
					break;
				}

				if (on_filter_data_)
					on_filter_data_(frame);

				av_frame_unref(frame);
			}

			cond_notify_ = false;
		}

		av_frame_free(&frame);
	}

}