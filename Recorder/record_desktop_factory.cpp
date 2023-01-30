#include "record_desktop_factory.h"
#include "record_desktop_ffmpeg_gdi.h"
#include "error_define.h"


int record_desktop_new(RecordDesktopTypes type, am::RecordDesktop** recorder)
{
	int err = AE_NO;
	switch (type)
	{
	case DT_DESKTOP_FFMPEG_GDI:
		// *recorder = (am::record_desktop*)new am::record_desktop_ffmpeg_gdi();
		break;
	default:
		err = AE_UNSUPPORT;
		break;
	}

	return err;
}

void record_desktop_destroy(am::RecordDesktop** recorder)
{
	if (*recorder != nullptr) {
		(*recorder)->Stop();

		delete* recorder;
	}

	*recorder = nullptr;
}
