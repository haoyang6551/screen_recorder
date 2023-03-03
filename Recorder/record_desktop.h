#ifndef RECORD_DESKTOP
#define RECORD_DESKTOP

#include "desktop_common_define.h"

#include "headers_ffmpeg.h"

#include <atomic>
#include <thread>
#include <functional>

namespace am {
	typedef std::function<void(AVFrame* frame)> DesktopDataFunc;
	typedef std::function<void(int)> DesktopErrorFunc;

	class RecordDesktop
	{
	public:
		RecordDesktop();
		virtual ~RecordDesktop();

		virtual int Init(
			const RecordDesktopRect& rect,
			const int fps
		) = 0;

		virtual int Start() = 0;
		virtual int Pause() = 0;
		virtual int Resume() = 0;
		virtual int Stop() = 0;

		/*inline const AVRational& get_time_base() { return time_base_; }

		inline int64_t get_start_time() { return start_time_; }*/

		inline AVPixelFormat get_pixel_fmt() { return pixel_fmt_; }

	public:
		inline bool get_is_recording() { return running_; }
		inline const std::string& get_device_name() { return device_name_; }
		inline const RecordDesktopDataTypes get_data_type() { return data_type_; }
		inline void registe_cb(
			DesktopDataFunc on_data,
			DesktopErrorFunc on_error) {
			on_data_ = on_data;
			on_error_ = on_error;
		}
		inline const RecordDesktopRect& get_rect() { return rect_; }

		inline const int get_frame_rate() { return fps_; }

	protected:
		virtual void CleanUp() = 0;

	protected:
		std::atomic_bool running_;
		std::atomic_bool paused_;
		std::atomic_bool inited_;

		std::thread thread_;

		std::string device_name_;

		RecordDesktopRect rect_;
		RecordDesktopDataTypes data_type_;

		int fps_;

		DesktopDataFunc on_data_;
		DesktopErrorFunc on_error_;

		/*AVRational time_base_;
		int64_t start_time_;*/
		AVPixelFormat pixel_fmt_;
	};
}


#endif