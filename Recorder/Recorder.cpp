// Recorder.cpp : Defines the entry point for the console application.
//

#include "headers_ffmpeg.h"
#include "record_desktop_ffmpeg_gdi.h"
#include <iostream>

int main()
{
	auto RecordVideo = new am::RecordDesktopFfmpegGdi();
	RecordDesktopRect rect = {10, 10, 30, 30};
	RecordVideo->Init(rect, 24);
	RecordVideo->Start();
	getchar();
	RecordVideo->Stop();
	std::cout << "ok" << std::endl;
    return 0;
}

