#ifndef RECORD_DESKTOP_GDI
#define RECORD_DESKTOP_GDI

#include "record_desktop.h"

namespace am {

	class RecordDesktopFFmpegGdi :public RecordDesktop
	{
	public:
		RecordDesktopFFmpegGdi();
		~RecordDesktopFFmpegGdi();

		virtual int Init(
			const RecordDesktopRect& rect,
			const int fps) override;

		virtual int Start() override;
		virtual int Pause() override;
		virtual int Resume() override;
		virtual int Stop() override;

	protected:
		virtual void CleanUp() override;

	private:
		int Decode(AVFrame* frame, AVPacket* packet);

		void RecordFunc();

		int stream_index_;
		AVFormatContext* fmt_ctx_;
		AVInputFormat* input_fmt_;
		AVCodecContext* codec_ctx_;
		AVCodec* codec_;
	};

}
#endif