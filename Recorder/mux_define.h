#ifndef MUX_DEFINE
#define MUX_DEFINE

#include "headers_ffmpeg.h"

namespace am {
	struct AudioSample {
		uint8_t* buff_;
		int size_;
		int sample_in_;
	};

	class encoder_264;
	class record_desktop;
	class sws_helper;

	class encoder_aac;
	class filter_audio;
	class record_audio;
	class resample_pcm;

	struct MuxSetting {
		int v_frame_rate_;
		int v_bit_rate_;
		int v_width_;
		int v_height_;
		int v_qb_;

		int a_nb_channel_;
		int a_sample_rate_;
		AVSampleFormat a_sample_fmt_;
		int a_bit_rate_;
	};

	struct MuxStream {
		//common
		AVStream* st_;               // av stream
		AVBitStreamFilterContext* filter_; //pps|sps adt

		uint64_t pre_pts_;

		MuxSetting setting_;        // output setting

		//video
		encoder_264* v_enc_;         // video encoder
		record_desktop* v_src_;      // video source
		sws_helper* v_sws_;          // video sws

		//audio
		encoder_aac* a_enc_;          // audio encoder
		filter_audio* a_filter_;      // audio mixer
		int a_nb_;                    // audio source num
		record_audio** a_src_;        // audio sources
		resample_pcm** a_rs_;         // audio resamplers
		AudioSample** a_samples_;     // audio sample data
		AudioSample** a_resamples_;   // audio resampled data
	}MUX_STREAM;
}

#endif
