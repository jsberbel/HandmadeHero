#include <windows.h>
#include <cstdint>
#include <XInput.h>
#include <dsound.h>

#define internal static
#define local_persist static
#define global_var static

typedef int8_t int8; //char
typedef int16_t int16; //short
typedef int32_t int32; //int
typedef int64_t int64; //long long

typedef uint8_t uint8; //uchar
typedef uint16_t uint16; //ushort
typedef uint32_t uint32; //uint
typedef uint64_t uint64; //ulong long

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

global_var bool globalRunning; //temporary global
global_var win32_offscreen_buffer globalBackBuffer;
global_var LPDIRECTSOUNDBUFFER globalSecondaryBuffer;

// ----------------------------------------------------------------------------------------------------	//
// DYNAMIC LINKING OF WINDOWS GAMEPAD CONTROLLERS														//
// ----------------------------------------------------------------------------------------------------	//
// Allows us to avoid the .lib because of platform lacks, limited to Win8, WinVista & DirectX			//
// ----------------------------------------------------------------------------------------------------	//
DWORD WINAPI XInputGetStateStub(DWORD dwUserIndex, XINPUT_STATE* pState) {
	return ERROR_DEVICE_NOT_CONNECTED;
}
global_var auto XInputGetState_ = &XInputGetStateStub;
#define XInputGetState XInputGetState_

DWORD WINAPI XInputSetStateStub(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration) {
	return ERROR_DEVICE_NOT_CONNECTED;
}
global_var auto XInputSetState_ = &XInputSetStateStub;
#define XInputSetState XInputSetState_
// ----------------------------------------------------------------------------------------------------	//

internal void Win32LoadXInput(void) {
	HMODULE xInputLibrary = LoadLibraryA("xinput1_4.dll"); //only works on windows 8
	if (!xInputLibrary) xInputLibrary = LoadLibraryA("xinput1_3.dll"); //works on any other windows version

	if (xInputLibrary) {
		XInputGetState = reinterpret_cast<decltype(XInputGetState)>(GetProcAddress(xInputLibrary, "XInputGetState"));
		if (!XInputGetState) XInputGetState = XInputGetStateStub;
		XInputSetState = reinterpret_cast<decltype(XInputSetState)>(GetProcAddress(xInputLibrary, "XInputSetState"));
		if (!XInputSetState) XInputSetState = XInputSetStateStub;
	}
}

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI direct_sound_create (LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void Win32InitDSound(const HWND &window, int32 samplesPerSecond, int32 bufferSize) {
	HMODULE dSoundLibrary = LoadLibraryA("dsound.dll");

	if (dSoundLibrary) {
		direct_sound_create* directSoundCreate = reinterpret_cast<direct_sound_create*>(GetProcAddress(dSoundLibrary, "DirectSoundCreate"));
		
		LPDIRECTSOUND directSound;
		if (directSoundCreate && SUCCEEDED(directSoundCreate(nullptr, &directSound, nullptr))) {
			WAVEFORMATEX waveFormat = {};
			waveFormat.wFormatTag = WAVE_FORMAT_PCM;
			waveFormat.nChannels = 2;
			waveFormat.nSamplesPerSec = samplesPerSecond;
			waveFormat.wBitsPerSample = 16;
			waveFormat.nBlockAlign = (waveFormat.nChannels*waveFormat.wBitsPerSample) / 8;
			waveFormat.nAvgBytesPerSec = waveFormat.nBlockAlign*waveFormat.nSamplesPerSec;
			waveFormat.cbSize = 0;

			if (SUCCEEDED(directSound->SetCooperativeLevel(window, DSSCL_PRIORITY))) {
				DSBUFFERDESC bufferDescription = {};
				bufferDescription.dwSize = sizeof bufferDescription;
				bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
				
				//PRIMARY BUFFER: handle to sound card
				LPDIRECTSOUNDBUFFER primaryBuffer;
				if (SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription, &primaryBuffer, nullptr))) {
					if (SUCCEEDED(primaryBuffer->SetFormat(&waveFormat))) {
						
					}
				}
			}

			DSBUFFERDESC bufferDescription = {};
			bufferDescription.dwSize = sizeof(bufferDescription);
			bufferDescription.dwFlags = 0;
			bufferDescription.dwBufferBytes = bufferSize;
			bufferDescription.lpwfxFormat = &waveFormat;

			//SECONDARY BUFFER
			
			if (SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription, &globalSecondaryBuffer, nullptr))) {
				/*if (SUCCEEDED(secondaryBuffer->SetFormat(&waveFormat))) {

				}*/
			}
		} else {
			
		}
	}
}

internal win32_window_dimensions& Win32GetWindowDimensions(const HWND &window) {
	static win32_window_dimensions result;
	RECT clientRect;
	GetClientRect(window, &clientRect);
	result.width = clientRect.right - clientRect.left;
	result.height = clientRect.bottom - clientRect.top;
	return result;
}

internal void RenderWeirdGradient(win32_offscreen_buffer &buffer, int blueOffset, int greenOffset)
{
	const int width = buffer.width;
	const int height = buffer.height;
	
	uint8 *row = static_cast<uint8*>(buffer.memory);
	for (int y = 0; y < height; ++y) {
		uint32 *pixel = reinterpret_cast<uint32*>(row);
		for (int x = 0; x < width; ++x) {
			// Pixel (32 bits)
			// Memory:		BB GG RR xx
			// Register:	xx RR GG BB
			uint8 blue = x + blueOffset;
			uint8 green = y + greenOffset;
			*pixel++ = (green << 8) | blue;
		}
		row += buffer.pitch;
	}
}

internal void Win32ResizeDIBSection(win32_offscreen_buffer &buffer, int width, int height)
{
	if (buffer.memory) VirtualFree(buffer.memory, 0, MEM_RELEASE);

	buffer.width = width; buffer.height = height;

	BITMAPINFOHEADER &bitmapInfoHeader = buffer.info.bmiHeader;
	bitmapInfoHeader.biSize = sizeof bitmapInfoHeader;
	bitmapInfoHeader.biWidth = width;
	bitmapInfoHeader.biHeight = -height;
	bitmapInfoHeader.biPlanes = 1;
	bitmapInfoHeader.biBitCount = 32;
	bitmapInfoHeader.biCompression = BI_RGB;

	int bytesPerPixel = 4;
	SIZE_T bitmapMemorySize = (width*height)*bytesPerPixel;
	buffer.memory = VirtualAlloc(nullptr, bitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

	buffer.pitch = width*bytesPerPixel; //how many bytes to move from one row to the next row
}

internal void Win32DisplayBufferInWindow(win32_offscreen_buffer &buffer, const HDC &deviceContext, int windowWidth, int windowHeight)
{
	StretchDIBits(deviceContext, 
				  0, 0, windowWidth, windowHeight,
				  0, 0, buffer.width, buffer.height,
				  buffer.memory, &buffer.info, DIB_RGB_COLORS, SRCCOPY);
}

internal LRESULT CALLBACK Win32MainWindowCallback(HWND window, UINT msg, WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;

	switch (msg) {
		case WM_SIZE: {

		} break;
		case WM_CLOSE: {
			globalRunning = false;
		} break;
		case WM_ACTIVATEAPP: {

		} break;
		case WM_DESTROY: {
			globalRunning = false;
		} break;
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP: {
			uint32 vKCode = wParam; //virtual-key code of nonsystem key
			bool wasDown = (lParam & (1 << 30)) != 0;
			bool isDown = (lParam & (1 << 31)) == 0;
			if (wasDown != isDown) {
				switch (vKCode) {
					case 'W':{
						OutputDebugStringA("W");
					} break;
					case 'A': {
						OutputDebugStringA("A");
					} break;
					case 'S':
					break;
					case 'D':
					break;
					case VK_UP:
					break;
					case VK_DOWN:
					break;
					case VK_RIGHT:
					break;
					case VK_LEFT:
					break;
					case VK_ESCAPE:
					break;
					case VK_SPACE:
					globalRunning = false;
					break;
					case VK_F4:
					if (lParam & (1 << 29)) globalRunning = false; // ALT + F4
					break;
				}
			}
		} break;
		case WM_PAINT: {
			PAINTSTRUCT paint;
			HDC deviceContext = BeginPaint(window, &paint);
			win32_window_dimensions dimensions = Win32GetWindowDimensions(window);
			Win32DisplayBufferInWindow(globalBackBuffer, deviceContext, dimensions.width, dimensions.height);
			EndPaint(window, &paint);
		} break;
		default: {
			result = DefWindowProc(window, msg, wParam, lParam);
		} break;
	}

	return result;
}

int CALLBACK WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR commandLine, int showCode) 
{
	Win32LoadXInput();

	WNDCLASSA windowClass = {};
	                  
	Win32ResizeDIBSection(globalBackBuffer, 1280, 720);

	windowClass.style = CS_HREDRAW|CS_VREDRAW; //repaint the whole window when resized
	windowClass.lpfnWndProc = Win32MainWindowCallback; //process messages sent to window
	windowClass.hInstance = instance; //actual process of the app
	///windowClass.hIcon
	windowClass.lpszClassName = "HandmadeHeroWindowClass";

	if (RegisterClassA(&windowClass)) {
		HWND window = CreateWindowExA(0, windowClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
									  CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
									  nullptr, nullptr, instance, nullptr);
		if (window) {

			HDC deviceContext = GetDC(window);
			int xOffset = 0, yOffset = 0;

			const int samplesPerSecond = 48000;
			const int toneHz = 256;
			uint32 runningSampleIndex = 0;
			int squareWavePeriod = samplesPerSecond / toneHz;
			int halfSquaredPeriod = squareWavePeriod / 2;
			int bytesPerSample = sizeof(int16) * 2;
			int secondaryBufferSize = samplesPerSecond*bytesPerSample;
			int16 toneVolume = 1000;

			Win32InitDSound(window, samplesPerSecond, secondaryBufferSize);
			bool soundIsPlaying = false;

			globalRunning = true;
			while (globalRunning) {

				MSG msg;
				while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
					if (msg.message == WM_QUIT) globalRunning = false;
					TranslateMessage(&msg);
					DispatchMessageA(&msg);
				}

				//loop through all possible input controllers
				for (DWORD controllerIndex = 0; controllerIndex < XUSER_MAX_COUNT; ++controllerIndex) {
					XINPUT_STATE controllerState;
					if (XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS) {
						//controller plugged in
						XINPUT_GAMEPAD *pad = &controllerState.Gamepad;
						auto up = pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
						auto down = pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
						auto left = pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
						auto right = pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
						auto start = pad->wButtons & XINPUT_GAMEPAD_START;
						auto back = pad->wButtons & XINPUT_GAMEPAD_BACK;
						auto leftShoulder = pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
						auto rightShoulder = pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
						auto aButton = pad->wButtons & XINPUT_GAMEPAD_A;
						auto bButton = pad->wButtons & XINPUT_GAMEPAD_B;
						auto xButton = pad->wButtons & XINPUT_GAMEPAD_X;
						auto yButton = pad->wButtons & XINPUT_GAMEPAD_Y;

						int16 stickX = pad->sThumbLX;
						int16 stickY = pad->sThumbLY;
					} else {
						//controller not available
					}
				}
				
				XINPUT_VIBRATION vibration;
				vibration.wLeftMotorSpeed = 60000;
				vibration.wRightMotorSpeed = 60000;
				XInputSetState(0, &vibration);

				RenderWeirdGradient(globalBackBuffer, xOffset, yOffset);

				DWORD playCursor;
				DWORD writeCursor;
				if (SUCCEEDED(globalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor))){
					DWORD byteToLock = runningSampleIndex*bytesPerSample % secondaryBufferSize;
					DWORD bytesToWrite = ((playCursor - byteToLock) + secondaryBufferSize) % secondaryBufferSize;
					/*if (bytesToWrite == playCursor) bytesToWrite = secondaryBufferSize;
					else if (byteToLock > playCursor) {
						bytesToWrite = (secondaryBufferSize - byteToLock);
						bytesToWrite += playCursor;
					} else bytesToWrite = playCursor - bytesToWrite;*/
					
					VOID *region1;
					DWORD region1Size;
					VOID *region2; 
					DWORD region2Size;
					if (SUCCEEDED(globalSecondaryBuffer->Lock(byteToLock, bytesToWrite, &region1, &region1Size, &region2, &region2Size, 0))) {
						DWORD region1SampleCount = region1Size / bytesPerSample;
						int16 *sampleOut = static_cast<int16*>(region1);
						for (DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex) {
							int16 sampleValue = (runningSampleIndex++ / halfSquaredPeriod) % 2 ? toneVolume : -toneVolume;
							*sampleOut++ = sampleValue;
							*sampleOut++ = sampleValue;
						}
						DWORD region2SampleCount = region2Size / bytesPerSample;
						sampleOut = static_cast<int16*>(region2);
						for (DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex) {
							int16 sampleValue = (runningSampleIndex++ / halfSquaredPeriod) % 2 ? toneVolume : -toneVolume;
							*sampleOut++ = sampleValue;
							*sampleOut++ = sampleValue;
						}

						globalSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);
					}				
				}
				if (!soundIsPlaying) {
					soundIsPlaying = true;
					globalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
				}

				win32_window_dimensions dimensions = Win32GetWindowDimensions(window);
				Win32DisplayBufferInWindow(globalBackBuffer, deviceContext, dimensions.width, dimensions.height);

				++xOffset;
				yOffset += 2;
			}
		}
	}

	return EXIT_SUCCESS;
}