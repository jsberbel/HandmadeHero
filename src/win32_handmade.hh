#pragma once
/*
* ========================================================================
* File: win32_handmade.hh
* Revision: 1.0
* Creator: Jordi Serrano Berbel
* Notice: Copyright © 2017 by Jordi Serrano Berbel. All Rights Reserved.
* =======================================================================
*/

namespace Win32
{
	struct offscreen_buffer
	{
		BITMAPINFO info;
		void *memory;
		int width;
		int height;
		int pitch;
	};

	struct window_dimensions
	{
		int width;
		int height;
	};

	struct sound_output
	{
		int samplesPerSecond;
		uint32 runningSampleIndex;
		int bytesPerSample;
		int secondaryBufferSize;
		real32 tSine; // TODO: NOT NEEDED
		int latencySampleCount;
	};

	void LoadXInput(void);
	void InitDSound(const HWND &window, int32 samplesPerSecond, int32 bufferSize);
	void ClearSoundBuffer(sound_output &soundOutput);
	void FillSoundBuffer(sound_output &soundOutput, DWORD byteToLock, DWORD bytesToWrite,
						 game_sound_output_buffer &sourceBuffer);
	void ProcessKeyboardMessage(game_button_state &newState, bool32 isDown);
	void ProcessXInputDigitalButton(DWORD xInputButtonState,
									game_button_state &oldState, DWORD buttonBit,
									game_button_state &newState);
	window_dimensions& GetWindowDimensions(const HWND &window);
	void ResizeDIBSection(Win32::offscreen_buffer &buffer, int width, int height);
	void DisplayBufferInWindow(Win32::offscreen_buffer &buffer, const HDC &deviceContext, int windowWidth, int windowHeight);
	void ProcessPendingMessages(game_controller_input &keyboardController);
	LRESULT CALLBACK Win32::MainWindowCallback(HWND window, UINT msg, WPARAM wParam, LPARAM lParam);
}