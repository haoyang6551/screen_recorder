#include "record_audio_factory.h"
#include "record_desktop_factory.h"

#include "mux_define.h"
#include "mux_mp4.h"

#include "error_define.h"



#define V_FRAME_RATE 20
#define V_BIT_RATE 64000
#define V_WIDTH 2560
#define V_HEIGHT 1440

#define A_SAMPLE_RATE 44100
#define A_BIT_RATE 128000



static am::RecordAudio* recorder_speaker = nullptr;
static am::RecordAudio* recorder_microphone = nullptr;
static am::RecordDesktop* recorder_desktop = nullptr;

static am::MuxMP4* muxer;

static am::RecordAudio* audio;

int start_muxer() {
	std::string input_id, input_name, out_id, out_name;

	record_audio_new(RecordAudioTypes::AT_AUDIO_DSHOW, &recorder_speaker);
	recorder_speaker->Init(
		"audio=virtual-audio-capturer",
		"audio=virtual-audio-capturer",
		false
	);

	record_desktop_new(RecordDesktopTypes::DT_DESKTOP_FFMPEG_GDI, &recorder_desktop);

	RecordDesktopRect rect;
	rect.left_ = 0;
	rect.top_ = 0;
	rect.right_ = V_WIDTH;
	rect.bottom_ = V_HEIGHT;

	recorder_desktop->Init(rect, V_FRAME_RATE);

	audio = recorder_speaker;

	muxer = new am::MuxMP4();


	am::MuxSetting setting;
	setting.v_frame_rate_ = V_FRAME_RATE;
	setting.v_bit_rate_ = V_BIT_RATE;
	setting.v_width_ = V_WIDTH;
	setting.v_height_ = V_HEIGHT;
	setting.v_qb_ = 30;

	setting.a_nb_channel_ = 2;
	setting.a_sample_fmt_ = AV_SAMPLE_FMT_FLTP;
	setting.a_sample_rate_ = A_SAMPLE_RATE;
	setting.a_bit_rate_ = A_BIT_RATE;

	int error = muxer->Init("..\\..\\save.mp4", recorder_desktop, audio, setting);
	if (error != AE_NO) {
		return error;
	}

	muxer->Start();

	return error;
}

void stop_muxer()
{
	muxer->Stop();

	if (recorder_desktop)
		delete recorder_desktop;

	if (recorder_speaker)
		delete recorder_speaker;

	if (muxer)
		delete muxer;
}

void test_recorder()
{

	start_muxer();

	getchar();

	//stop have bug that sometime will stuck
	stop_muxer();

}



int main(int argc, char** argv)
{

	//test_audio();

	test_recorder();

	std::cout << "press any key to exit..." << std::endl;
	system("pause");

	return 0;
}




