#include "record_desktop.h"

am::RecordDesktop::RecordDesktop()
{
	running_ = false;
	paused_ = false;
	inited_ = false;

	on_data_ = nullptr;
	on_error_ = nullptr;

	device_name_ = "";
	data_type_ = RecordDesktopDataTypes::AT_DESKTOP_BGRA;

	/*time_base_ = { 1,AV_TIME_BASE };
	start_time_ = 0;*/
	pixel_fmt_ = AV_PIX_FMT_NONE;
}

am::RecordDesktop::~RecordDesktop()
{
}
