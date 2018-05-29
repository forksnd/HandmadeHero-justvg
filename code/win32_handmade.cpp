#include <Windows.h>
#include <stdint.h>
#include <Xinput.h>
#include <dsound.h>

// TODO(george): Implement sine ourselves
#include <math.h>

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32_t bool32;

typedef float real32;
typedef double real64;

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

struct win32_window_dimension
{
    int Width;
    int Height;
};

// TODO(george): This is a global for now.
global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackbuffer;
LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

#define D_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef D_SOUND_CREATE(d_sound_create);

// NOTE(george): XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE(george): XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return(ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

internal void Win32LoadInput(void)
{
    // TODO(george): Test this on Windows8
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");

	if (!XInputLibrary)
	{
		// TODO(george): Diagnostic		
		XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
	}

    if (!XInputLibrary)
    {
        // TODO(george): Diagnostic		
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }
    
    if (XInputLibrary)
    {
        XInputGetState = (x_input_get_state *) GetProcAddress(XInputLibrary, "XInputGetState");
        if (!XInputGetState){XInputGetState = XInputGetStateStub;}
        XInputSetState = (x_input_set_state *) GetProcAddress(XInputLibrary, "XInputSetState");
        if (!XInputSetState){XInputSetState = XInputSetStateStub;}
        
        // TODO(george): Diagnostic
    }
    else
    {
        // TODO(george): Diagnostic
    }
    
} 

internal void Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
    // NOTE(george): Load the library
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    if (DSoundLibrary)
    {
        // NOTE(george): Get a DirectSound object! - cooperative
        d_sound_create *DirectSoundCreate = (d_sound_create*) GetProcAddress(DSoundLibrary, "DirectSoundCreate");
        
        LPDIRECTSOUND DirectSound;
        if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0)))
        {
            WAVEFORMATEX WaveFormat = {};
            WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
            WaveFormat.nChannels = 2;
            WaveFormat.nSamplesPerSec = SamplesPerSecond;
            WaveFormat.wBitsPerSample = 16;
            WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
            WaveFormat.nAvgBytesPerSec = WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
            
            if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY)))
            {
                DSBUFFERDESC BufferDescription = {sizeof(DSBUFFERDESC)};
                BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
                
                // NOTE(george): "Create" a primary buffer
                // TODO(george): DSBCAPS_GLOBALFOCUS?
                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
                {
                    if (SUCCEEDED(PrimaryBuffer->SetFormat(&WaveFormat)))
                    {
                        // NOTE(george): We have finally set the format!
                        OutputDebugStringA("Primary buffer format was set.\n");
                    }
                    else
                    {
                        // TODO(george): Diagnostic
                    }
                }
                else
                {
                    // TODO(george): Diagnostic
                }
            }
            else
            {
                // TODO(george): Diagnostic				
            }
            
            // NOTE(george): Create a secondary buffer	
            DSBUFFERDESC BufferDescription = {sizeof(DSBUFFERDESC)};
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;
            if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0)))
            {
                OutputDebugStringA("Secondary buffer created\n");
            }
        }
        else
        {
            // TODO(george): Diagnostic
        }
    }
    else
    {
        // TODO(george): Diagnostic
    }
}

internal win32_window_dimension GetWindowDimenstion(HWND Window)
{
    win32_window_dimension Result;
    
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;
    
    return (Result);
}

internal void RenderWeirdGradient(win32_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
    int BytesPerPixel = 4;
    Buffer->Pitch = Buffer->Width * BytesPerPixel;
    uint8 *Row = (uint8 *)Buffer->Memory;
    for (int Y = 0; Y < Buffer->Height; Y++)
    {
        uint32 *Pixel = (uint32 *) Row;
        
        for (int X = 0; X < Buffer->Width; X++)
        {
            uint8 Blue = (X + BlueOffset);
            uint8 Green = (Y + GreenOffset);
            
            *Pixel++ = ((Green << 8) | Blue);
        }
        
        Row += Buffer->Pitch;
    }
}

internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    if (Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }
    
    int BytesPerPixel = 4;
    
    Buffer->Width = Width;
    Buffer->Height = Height;
    
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;
    
    int BitmapMemorySize = Width * Height * BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    // TODO(george): Probably clear this to black
}

internal void Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer, HDC DeviceContext, int WindowWidth, int WindowHeight)
{
    StretchDIBits(DeviceContext,
                  0, 0, WindowWidth, WindowHeight,
                  0, 0, Buffer->Width, Buffer->Height,
                  Buffer->Memory, &Buffer->Info, DIB_RGB_COLORS, SRCCOPY);
}

struct win32_sound_output
{
    int SamplesPerSecond;
    int ToneHz;
    int16 ToneVolume;
    uint32 RunningSampleIndex;
    int WavePeriod;
    int32 BytesPerSample;
    int32 SecondaryBufferSize;
    real32 tSine;
    int LatencySampleCount;
};

internal void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0) == DS_OK)
    {
        DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
        int16 *SampleOut = (int16 *) Region1;
        for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; SampleIndex++)
        {
            //real32 t = 2.0f*Pi32*((real32)SoundOutput->RunningSampleIndex / (real32)SoundOutput->WavePeriod);
            real32 SineValue = sin(SoundOutput->tSine);
            int16 SampleValue = (int16)(SineValue * SoundOutput->ToneVolume);
            *SampleOut++ = SampleValue;
            *SampleOut++ = SampleValue;

			SoundOutput->tSine += 2.0f*Pi32*1.0f / (real32)SoundOutput->WavePeriod;
            SoundOutput->RunningSampleIndex++;
        }
                        
        DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
        SampleOut = (int16 *) Region2;
        for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; SampleIndex++)
        {
            //real32 t = 2.0f*Pi32*((real32)SoundOutput->RunningSampleIndex / (real32)SoundOutput->WavePeriod);
            real32 SineValue = sin(SoundOutput->tSine);
            int16 SampleValue = (int16)(SineValue * SoundOutput->ToneVolume);
            *SampleOut++ = SampleValue;
            *SampleOut++ = SampleValue;

			SoundOutput->tSine += 2.0f*Pi32*1.0f / (real32)SoundOutput->WavePeriod;
			SoundOutput->RunningSampleIndex++;
        }
                        
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}

LRESULT CALLBACK Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;
    
    switch (Message)
    {
        case WM_SIZE:
        {
        } break;
        
        case WM_DESTROY:
        {
            // TODO(george): Handle this as an error - recreate window?
            GlobalRunning = false;
        } break;
        
        case WM_CLOSE:
        {
            // TODO(george): Handle this with a message to the user?
            GlobalRunning = false;
        } break;
        
        case WM_ACTIVATEAPP:
        {
            OutputDebugStringA("WM_ACTIVATEAPP\n");
        } break;
        
        case WM_PAINT:
        {
            PAINTSTRUCT Paint;
            HDC DeviceContext = BeginPaint(Window, &Paint);
            int X = Paint.rcPaint.left;
            int Y = Paint.rcPaint.top;
            int Width = Paint.rcPaint.right - Paint.rcPaint.left;
            int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;
            
            win32_window_dimension Dimension = GetWindowDimenstion(Window);
            
            Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);
            EndPaint(Window, &Paint);
        } break;
        
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            uint32 VKCode = WParam;
            bool WasDown = ((LParam & (1 << 30)) != 0);
            bool IsDown = ((LParam & (1 << 31)) == 0);
            
            if (WasDown != IsDown)
            { 
                if (VKCode == 'W')
                {
                }
                else if (VKCode == 'A')
                {
                }
                else if (VKCode == 'S')
                {
                }
                else if (VKCode == 'D')
                {
                }
                else if (VKCode == 'Q')
                {
                }
                else if (VKCode == 'E')
                {
                }
                else if (VKCode == VK_UP)
                {
                }
                else if (VKCode == VK_LEFT)
                {
                }
                else if (VKCode == VK_DOWN)
                {
                }
                else if (VKCode == VK_RIGHT)
                {
                }
                else if (VKCode == VK_ESCAPE)
                {
                    OutputDebugStringA("ESCAPE: ");
                    if (IsDown)
                    {
                        OutputDebugStringA("IsDown ");
                    }
                    if (WasDown)
                    {
                        OutputDebugStringA("WasDown ");
                    }
                    OutputDebugStringA("\n");
                }
                else if (VKCode == VK_SPACE)
                {
                }
                
                bool32 AltKeyWasDown = (LParam & (1 << 29));
                if ((VKCode == VK_F4) && AltKeyWasDown)
                {
                    GlobalRunning = false;
                }
            }
        } break;
        
        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }
    
    return (Result);
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    Win32LoadInput();
    
    WNDCLASSA WindowClass = {};
    
    Win32ResizeDIBSection(&GlobalBackbuffer, 1280, 720);
    
    WindowClass.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    //	WindowClass.hIcon = ;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";
    
    if (RegisterClassA(&WindowClass))
    {
        HWND Window = CreateWindowExA(
            0,
            WindowClass.lpszClassName,
            "Handmade Hero",
            WS_OVERLAPPEDWINDOW | WS_VISIBLE,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            0,
            0,
            Instance,
            0);
        
        if (Window)
        {
            // NOTE(george): Since we specified CS_OWNDC, we can just
            // get one device context and use it forever because we
            // are not sharing it with anyone
            HDC DeviceContext = GetDC(Window);
            
            // NOTE(george): Graphics output test
            int BlueOffset = 0;
            int GreenOffset = 0;
            
            // NOTE(george): DirestSound output test
            win32_sound_output SoundOutput = {};

            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.ToneHz = 256;
            SoundOutput.ToneVolume = 3000;
            SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
            SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond/SoundOutput.ToneHz;
            SoundOutput.BytesPerSample = sizeof(int16)*2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;
            
            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32FillSoundBuffer(&SoundOutput, 0, SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            GlobalRunning = true;
            while (GlobalRunning)
            {
                MSG Message;
                while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                {
                    if (Message.message == WM_QUIT)
                    {
                        GlobalRunning = false;
                    }
                    
                    TranslateMessage(&Message);
                    DispatchMessageA(&Message);
                }
                
                // TODO(george): Should we poll this more frequently
                for (DWORD ControllerIndex = 0; ControllerIndex < XUSER_MAX_COUNT; ControllerIndex++)
                {
                    XINPUT_STATE ControllerState;
                    if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                    {
                        // NOTE(george): This controller is plugged in
                        // TODO(george): See if ControllerState.dwPacketNumber increments too rapidly
                        XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
                        
                        bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                        bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                        bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                        bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                        bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                        bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                        bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                        bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                        bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
                        bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);
                        bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
                        bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);
                        
                        int16 StickX = Pad->sThumbLX;
                        int16 StrickY = Pad->sThumbLY;
                        
                        if (AButton)
                        {
                            SoundOutput.ToneHz = 512;
                            SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond/SoundOutput.ToneHz;
                        }
                    }
                    else
                    {
                        // NOTE(george): This controller is not available
                    }
                    
                    XINPUT_VIBRATION Vibration;
                    Vibration.wLeftMotorSpeed = 50000;
                    XInputSetState(ControllerIndex, &Vibration);
                }
                
                RenderWeirdGradient(&GlobalBackbuffer, BlueOffset, GreenOffset);
                
                DWORD PlayCursor;
                DWORD WriteCursor;
                if (GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor) == DS_OK)
                {
                    DWORD ByteToLock = (SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) % SoundOutput.SecondaryBufferSize;
                    DWORD BytesToWrite;

                    DWORD TargetCursor = (PlayCursor + (SoundOutput.LatencySampleCount*SoundOutput.BytesPerSample)) % SoundOutput.SecondaryBufferSize;

                    if (ByteToLock > TargetCursor)
                    {
                        BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
                        BytesToWrite += TargetCursor;
                    }
                    else 
                    {
                        BytesToWrite = TargetCursor - ByteToLock;
                    }

                    Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite);
                }
                
                win32_window_dimension Dimension = GetWindowDimenstion(Window);
                Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);
                
                BlueOffset++; 				
            }
        }
        else
        {
            // TODO(george): Logging
        }
    }
    else
    {
        // TODO(george): Logging
    }
    
    return (0);
}