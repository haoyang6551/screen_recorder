#ifndef RECORD_DESKTOP_FACTORY
#define RECORD_DESKTOP_FACTORY

#include "record_desktop.h"

int record_desktop_new(RecordDesktopTypes type, am::RecordDesktop** recorder);

void record_desktop_destroy(am::RecordDesktop** recorder);

#endif
