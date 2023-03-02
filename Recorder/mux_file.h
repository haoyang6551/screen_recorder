#ifndef MUX_FILE
#define MUX_FILE

#include <functional>

namespace am {

	class RecordAudio;
	class RecordDesktop;

	struct MuxStream;
	struct MuxSetting;

	class MuxFile
	{
	public:
		MuxFile();
		virtual ~MuxFile();

		virtual int Init(
			const char* output_file,
			RecordDesktop* source_desktop,
			RecordAudio** source_audios,
			const int source_audios_nb,
			const MuxSetting& setting
		) = 0;

		virtual int Start() = 0;
		virtual int Stop() = 0;

		virtual int Pause() = 0;
		virtual int Resume() = 0;
	};


}

#endif