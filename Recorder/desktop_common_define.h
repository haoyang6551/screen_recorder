#ifndef DESKTOP_COMMON_DEFINE
#define EESKTOP_COMMON_DEFINE

/*
* Record type
*
*/
typedef enum {
	DT_DESKTOP_NO = 0,
	DT_DESKTOP_FFMPEG_GDI,
	DT_DESKTOP_FFMPEG_DSHOW,
	DT_DESKTOP_WIN_GDI,
}RecordDesktopTypes;

/*
* Record desktop data type
*
*/

typedef enum {
	AT_DESKTOP_NO = 0,
	AT_DESKTOP_RGBA,
	AT_DESKTOP_BGRA
}RecordDesktopDataTypes;

/*
* Record desktop rect
*
*/

typedef struct {
	int left_;
	int top_;
	int right_;
	int bottom_;
}RecordDesktopRect;





#endif
