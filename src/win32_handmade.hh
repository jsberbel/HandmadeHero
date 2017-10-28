/*
* ========================================================================
* File: win32_handmade.hh
* Revision: 1.0
* Creator: Jordi Serrano Berbel
* Notice: Copyright © 2017 by Jordi Serrano Berbel. All Rights Reserved.
* =======================================================================
*/

#pragma once

namespace Win32
{
	struct OffscreenBuffer
	{
		BITMAPINFO info;
		void *memory;
		int width;
		int height;
		int pitch;
	};

	struct WindowDimensions
	{
		int width;
		int height;
	};

	struct SoundOutput
	{
		int samplesPerSecond;
		uint32 runningSampleIndex;
		int bytesPerSample;
		int secondaryBufferSize;
		real32 tSine; // TODO: NOT NEEDED
		int latencySampleCount;
	};
}