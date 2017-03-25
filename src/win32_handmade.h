#pragma once

#include "handmade.hh"

#define HANDMADE_WIN32 1
#define HANDMADE_DEBUG 1 // not slow code allowed
#define HANDMADE_INTERNAL 1 // developer only

#ifdef HANDMADE_DEBUG
#define Assert(condition) if (!(condition)) { *(int*)0 = 0;}
#else
#define Assert(condition)
#endif

#define Kilobytes(value) ((value)*1024)
#define Megabytes(value) (Kilobytes(value)*1024)
#define Gigabytes(value) (Megabytes(value)*1024)
#define Terabytes(value) (Gigabytes(value)*1024)

#define ArrayCount(arr) ((sizeof (arr)) / (sizeof (arr)[0]))

struct win32_offscreen_buffer
{
	BITMAPINFO info;
	void *memory;
	int width;
	int height;
	int pitch;
};

struct win32_window_dimensions
{
	int width;
	int height;
};

struct win32_sound_output
{
	int samplesPerSecond;
	uint32 runningSampleIndex;
	int bytesPerSample;
	int secondaryBufferSize;
	//real32 tSine;
	int latencySampleCount;
};