#ifndef RECORD_AUDIO_FACTORY
#define RECORD_AUDIO_FACTORY

#include "record_audio.h"

/**
*  Create a new audio record context
*
*/
int record_audio_new(RecordAudioTypes type, am::RecordAudio** recorder);

/**
* Destroy audio record context
*
*/
void record_audio_destroy(am::RecordAudio** recorder);

#endif 


