#ifndef AUDIO_COMMON_DEFINE
#define AUDIO_COMMON_DEFINE

#include <stdint.h>

/**
* Record type
*
*/
typedef enum {
	AT_AUDIO_NO = 0,
	AT_AUDIO_WAVE,    ///< wave api
	AT_AUDIO_WAS,     ///< wasapi(core audio)
	AT_AUDIO_DSHOW,   ///< direct show api
}RecordAudioTypes;

#endif 

