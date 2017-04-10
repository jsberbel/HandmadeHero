/*
* ========================================================================
* File: handmade.cc
* Revision: 1.0
* Creator: Jordi Serrano Berbel
* Notice: Copyright © 2017 by Jordi Serrano Berbel. All Rights Reserved.
* =======================================================================
*/

#include "handmade.hh"

internal void
GameOutputSound(game_sound_output_buffer &soundBuffer, int toneHz)
{
	local_persist real32 tSine;
	const int16 toneVolume	{ 3000 };
	const int wavePeriod	{ soundBuffer.samplesPerSecond / toneHz };

	int16 *&sampleOut = soundBuffer.samples;
	for (int sampleIndex{ 0 };
		 sampleIndex < soundBuffer.sampleCount;
		 ++sampleIndex)
	{
		real32 sineValue	{ sinf(tSine) };
		int16 sampleValue	{ int16(sineValue * toneVolume) };
		*sampleOut++ = sampleValue;
		*sampleOut++ = sampleValue;

		tSine += 2.0f*PI32*1.0f/real32(wavePeriod);
	}
}

internal void
RenderWeirdGradient(game_offscreen_buffer &buffer, int blueOffset, int greenOffset)
{
	//const int &width{ buffer.width };
	//const int &height{ buffer.height };

	uint8 *row{ static_cast<uint8*>(buffer.memory) };
	for (int y{ 0 }; y < buffer.height; ++y)
	{
		uint32 *pixel{ reinterpret_cast<uint32*>(row) };
		for (int x{ 0 }; x < buffer.width; ++x)
		{
			// Pixel (32 bits)
			// Memory:		BB GG RR xx
			// Register:	xx RR GG BB
			uint8 blue	{ uint8(x + blueOffset) };
			uint8 green	{ uint8(y + greenOffset) };

			*pixel++ = (green << 8) | blue;
		}
		row += buffer.pitch;
	}
}

internal game_memory *
GameStartUp(void)
{
	game_memory *memory = new game_memory;
	if (memory)
	{
		/*memory->toneHz = 256;
		memory->blueOffset = 0;
		memory->greenOffset = 0;*/
	}
	return memory;
}

internal void
GameShutDown(game_memory *memory)
{
	delete memory;
}

internal void
GameUpdateAndRender(game_memory &memory,
					game_input &input,
					game_offscreen_buffer &offscreenBuffer,
					game_sound_output_buffer &soundBuffer)
{
	Assert(sizeof(game_state) <= memory.permanentStorageSize);

	game_state *gameState = (game_state*)memory.permanentStorage;
	if (!memory.isInitialized)
	{
		const char *filename = __FILE__;

		DEBUG::read_file_result file = DEBUG::PlatformReadEntireFile(filename);
		if (file.data)
		{
			DEBUG::PlatformWriteEntireFile("test.out", file.dataSize, file.data);
			DEBUG::PlatformFreeFileMemory(file.data);
		}

		gameState->toneHz = 256;
		gameState->blueOffset = 0;
		gameState->greenOffset = 0;
		memory.isInitialized = true;
	}

	game_controller_input &input0 = input.controllers[0];

	if (input0.isAnalog)
	{
		// NOTE: Use analog movement tuning
		gameState->blueOffset += int(4.f*input0.endX);
		gameState->toneHz = 256 + int(128.f*input0.endY);
	}
	else
	{
		// NOTE: Use digital movement tuning
	}

	// Input.AButtonEndedDown;
	// Input.AButtonHalfTransitionCount;
	if (input0.down.endedDown)
	{
		gameState->greenOffset += 1;
	}

	// TODO: Allow sample offsets here for more robust platform options
	GameOutputSound(soundBuffer, gameState->toneHz);
	RenderWeirdGradient(offscreenBuffer, gameState->blueOffset, gameState->greenOffset);
}
