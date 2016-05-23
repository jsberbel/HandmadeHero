/* ========================================================================
File: handmade.cpp
Revision: 0.1
Creator: Jordi Serrano Berbel
Notice: (C) Copyright 2016 by Jordi Serrano Berbel. All Rights Reserved.
======================================================================== */

#include "handmade.h"
#include <cmath>

void GameOutputSound(game_sound_output_buffer &soundBuffer) {
	local_persist real32 tSine;
	const int16 toneVolume = 3000;
	const int toneHz = 256;
	const int wavePeriod = soundBuffer.samplesPerSecond / toneHz;

	int16 *sampleOut = soundBuffer.samples;
	for (int sampleIndex{ 0 }; sampleIndex < soundBuffer.sampleCount; ++sampleIndex) {
		real32 sineValue{ sinf(tSine) };
		int16 sampleValue{ int16(sineValue * toneVolume) };
		*sampleOut++ = sampleValue;
		*sampleOut++ = sampleValue;
		tSine = 2.0f*PI32*1.0f/real32(wavePeriod);
	}
}

internal void RenderWeirdGradient(game_offscreen_buffer &buffer, int blueOffset, int greenOffset) {
	const int width{ buffer.width };
	const int height{ buffer.height };

	uint8 *row{ static_cast<uint8*>(buffer.memory) };
	for (int y{ 0 }; y < height; ++y) {
		uint32 *pixel{ reinterpret_cast<uint32*>(row) };
		for (int x{ 0 }; x < width; ++x) {
			// Pixel (32 bits)
			// Memory:		BB GG RR xx
			// Register:	xx RR GG BB
			uint8 blue{ uint8(x + blueOffset) };
			uint8 green{ uint8(y + greenOffset) };
			*pixel++ = (green << 8) | blue;
		}
		row += buffer.pitch;
	}
}

void GameUpdateAndRender(game_sound_output_buffer &soundBuffer, game_offscreen_buffer &offscreenBuffer) {
	GameOutputSound(soundBuffer);
	int blueOffset = 0, greenOffset = 0;
	RenderWeirdGradient(offscreenBuffer, blueOffset, greenOffset);
}
