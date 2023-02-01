#include "record_audio_factory.h"
#include "record_audio_dshow.h"


#include "error_define.h"


int record_audio_new(RecordAudioTypes type, am::RecordAudio** recorder)
{
	int err = AE_NO;

	switch (type)
	{
	case AT_AUDIO_WAVE:
		err = AE_UNSUPPORT;
		break;
	case AT_AUDIO_WAS:
		err = AE_UNSUPPORT;
		break;
	case AT_AUDIO_DSHOW:
		*recorder = (am::RecordAudio*)new am::RecordAudioDshow();
		break;
	default:
		err = AE_UNSUPPORT;
		break;
	}

	return err;
}

void record_audio_destroy(am::RecordAudio** recorder)
{
	if (*recorder != nullptr) {
		(*recorder)->Stop();
		delete* recorder;
	}

	*recorder = nullptr;
}