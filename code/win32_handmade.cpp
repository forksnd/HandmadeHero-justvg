#include "handmade_platform.h"

#include <Windows.h>
#include <stdio.h>
#include <Xinput.h>
#include <dsound.h>

#include "win32_handmade.h"

// TODO(george): It is global for bow
global_variable bool32 GlobalRunning;
global_variable bool32 GlobalPause;
global_variable win32_offscreen_buffer GlobalBackbuffer;
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;
global_variable int64 GlobalPerfCounterFrequency;
global_variable bool32 DEBUGGlobalShowCursor;
global_variable WINDOWPLACEMENT GlobalWindowPosition = { sizeof(GlobalWindowPosition) };

internal void 
ToggleFullscreen(HWND Window)
{
    // NOTE(george): This follows Raymond Chen's prescription
    // for fullscreen toggling, see:
    // https://blogs.msdn.microsoft.com/oldnewthing/20100412-00/?p=14353

    DWORD Style = GetWindowLong(Window, GWL_STYLE);
    if (Style & WS_OVERLAPPEDWINDOW) 
    {
        MONITORINFO MonitorInfo = { sizeof(MonitorInfo) };
        if (GetWindowPlacement(Window, &GlobalWindowPosition) &&
            GetMonitorInfo(MonitorFromWindow(Window, MONITOR_DEFAULTTOPRIMARY), &MonitorInfo)) 
        {
            SetWindowLong(Window, GWL_STYLE, Style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(Window, HWND_TOP,
                        MonitorInfo.rcMonitor.left, MonitorInfo.rcMonitor.top,
                        MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                        MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                        SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }
    else 
    {
        SetWindowLong(Window, GWL_STYLE, Style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(Window, &GlobalWindowPosition);
        SetWindowPos(Window, NULL, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                    SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

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

inline FILETIME 
Win32GetLastWriteTime(char *Filename)
{
    FILETIME LastWriteTime = {};

    WIN32_FILE_ATTRIBUTE_DATA Data; 
    if(GetFileAttributesEx(Filename, GetFileExInfoStandard, &Data))
    {
        LastWriteTime = Data.ftLastWriteTime;
    }

    return(LastWriteTime);
}

internal win32_game_code
Win32LoadGameCode(char *SourceDLLName, char *TempDLLName, char *LockFilename)
{
    win32_game_code Result = {};

    WIN32_FILE_ATTRIBUTE_DATA Ignored; 
    if(!GetFileAttributesEx(LockFilename, GetFileExInfoStandard, &Ignored))
    {
        Result.DLLLastWriteTime = Win32GetLastWriteTime(SourceDLLName);

        CopyFile(SourceDLLName, TempDLLName, FALSE);

        Result.GameCodeDLL = LoadLibraryA(TempDLLName);    
        if (Result.GameCodeDLL)
        {
            Result.UpdateAndRender = (game_update_and_render *) GetProcAddress(Result.GameCodeDLL, "GameUpdateAndRender");
            Result.GetSoundSamples = (game_get_sound_samples *) GetProcAddress(Result.GameCodeDLL, "GameGetSoundSamples");
            Result.DEBUGFrameEnd = (debug_game_frame_end *) GetProcAddress(Result.GameCodeDLL, "DEBUGFrameEnd");

            Result.IsValid = Result.UpdateAndRender && Result.GetSoundSamples;
        }
    }

    if (!Result.IsValid)
    {
        Result.UpdateAndRender = 0;
        Result.GetSoundSamples = 0;
    }

    return(Result);
}

internal void
Win32UnloadGameCode(win32_game_code *GameCode)
{
    if (GameCode->GameCodeDLL)
    {
        FreeLibrary(GameCode->GameCodeDLL);
        GameCode->GameCodeDLL = 0;
    }

    GameCode->IsValid = false;
    GameCode->UpdateAndRender = 0;
    GameCode->GetSoundSamples = 0;
}

internal void 
Win32LoadXInput(void)
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

internal real32
Win32ProcessXInputStickValue(SHORT Value, SHORT DeadzoneThreshold)
{
    real32 Result = 0;

    if (Value < -DeadzoneThreshold)
    {
        Result = ((real32)(Value + DeadzoneThreshold) / (32768.0f - DeadzoneThreshold));
    }
    else if (Value > DeadzoneThreshold)
    {
        Result = ((real32)(Value - DeadzoneThreshold) / (32767.0f - DeadzoneThreshold));
    }

    return(Result);
}          

#if HANDMADE_INTERNAL
DEBUG_PLATFROM_FREE_FILE_MEMORY(DEBUGPlatformFreeFileMemory)
{
    VirtualFree(Memory, 0, MEM_RELEASE);
}               

DEBUG_PLATFROM_READ_ENTIRE_FILE(DEBUGPlatformReadEntireFile)
{
    debug_read_file_result Result = {};

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize;
        if (GetFileSizeEx(FileHandle, &FileSize))
        {
            uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
            Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            if (Result.Contents)
            {
                DWORD BytesRead;
                if ((ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0)) && (FileSize32 == BytesRead))
                {
                    // NOTE(george): File read succesfully
                    Result.ContentsSize = FileSize32;
                }
                else
                {
                    // TODO(george): Logging
                    DEBUGPlatformFreeFileMemory(Result.Contents);
                    Result.Contents= 0;
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

        CloseHandle(FileHandle);
    }
    else
    {
        // TODO(george): Logging
    }

    return(Result);
}

DEBUG_PLATFROM_EXECUTE_SYSTEM_COMMAND(DEBUGExecuteSystemCommand)
{
    debug_executing_process Result = {};

    STARTUPINFO StartupInfo = {};
    StartupInfo.cb = sizeof(StartupInfo);
    StartupInfo.dwFlags = STARTF_USESHOWWINDOW;
    StartupInfo.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION ProcessInfo = {};
    if(CreateProcess(Command, CommandLine, 0, 0, FALSE, 0, 0, Path, &StartupInfo, &ProcessInfo))
    {
        Assert(sizeof(Result.OSHandle) >= sizeof(ProcessInfo.hProcess));
        *(HANDLE *)&Result.OSHandle = ProcessInfo.hProcess;
    }
	else
	{
		DWORD ErrorCode = GetLastError();
        *(HANDLE *)&Result.OSHandle = INVALID_HANDLE_VALUE;
	}

    return(Result);
}

DEBUG_PLATFROM_GET_PROCESS_STATE(DEBUGGetProcessState)
{
    debug_process_state Result = {};
    HANDLE hProcess = *(HANDLE *)&Process.OSHandle;
    if(hProcess != INVALID_HANDLE_VALUE)
    {   
        Result.StartedSuccessfully = true;

        if(WaitForSingleObject(hProcess, 0) == WAIT_OBJECT_0)
        {
            DWORD ReturnCode = 0;
            GetExitCodeProcess(hProcess, &ReturnCode);
            Result.ReturnCode = ReturnCode;
            CloseHandle(hProcess);
        }
        else
        {
            Result.IsRunning = true;
        }
    }

    return(Result);
}

DEBUG_PLATFROM_WRITE_ENTIRE_FILE(DEBUGPlatformWriteEntireFile)
{
    bool32 Result = false;

    HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (FileHandle != INVALID_HANDLE_VALUE)
    {
        DWORD BytesWritten;
        if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
        {
            // NOTE(george): File written succesfully
            Result = (MemorySize == BytesWritten);
        }
        else
        {
            // TODO(george): Logging
        }

        CloseHandle(FileHandle);   
    }
    else
    {
        // TODO(george): Logging
    }

    return(Result);
}
#endif

internal void 
Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
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
                LPDIRECTSOUNDBUFFER PrimaryBuffer;
                if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
                {
                    HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
                    if (SUCCEEDED(Error))
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
            
            // TODO(george): In release mode, should we not specify DSBCAPS_GLOBALFOCUS?

            // NOTE(george): Create a secondary buffer	
            DSBUFFERDESC BufferDescription = {};
            BufferDescription.dwSize = sizeof(DSBUFFERDESC);
            BufferDescription.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
#if HANDMADE_INTERNAL
            BufferDescription.dwFlags |= DSBCAPS_GLOBALFOCUS;
#endif
            BufferDescription.dwBufferBytes = BufferSize;
            BufferDescription.lpwfxFormat = &WaveFormat;
            HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
            if (SUCCEEDED(Error))
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

internal void 
Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    if (Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }
    
    int BytesPerPixel = 4;
    Buffer->BytesPerPixel = BytesPerPixel;
    
    Buffer->Width = Width;
    Buffer->Height = Height;
    
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    Buffer->Pitch = Align16(Width * BytesPerPixel);
    int BitmapMemorySize = Buffer->Pitch  * Height;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    // TODO(george): Probably clear this to black
}

internal void 
Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer, HDC DeviceContext, int WindowWidth, int WindowHeight)
{
    // NOTE(george): Just a check to see that it works
    if((WindowWidth > 1200))
    {
        StretchDIBits(DeviceContext,
                      0, 0, WindowWidth, WindowHeight,
                      0, 0, Buffer->Width, Buffer->Height,
                      Buffer->Memory, &Buffer->Info, DIB_RGB_COLORS, SRCCOPY);
    }
    else
    {
#if 0
        int OffsetX = 10;
        int OffsetY = 10;

        PatBlt(DeviceContext, 0, 0, WindowWidth, OffsetY, BLACKNESS);
        PatBlt(DeviceContext, 0, OffsetY + Buffer->Height, WindowWidth, WindowHeight, BLACKNESS);
        PatBlt(DeviceContext, 0, 0, OffsetX, WindowHeight, BLACKNESS);
        PatBlt(DeviceContext, OffsetX + Buffer->Width, 0, WindowWidth, WindowHeight, BLACKNESS);
#else
        int OffsetX = 0;
        int OffsetY = 0;
#endif
        // NOTE(george): For prototyping purposes, we're goint to always blit
        // 1-to-1 pixels to make sure we don't introduce artifacts with 
        // stretching while we are learning to code the renderer!
        StretchDIBits(DeviceContext,
                    OffsetX, OffsetY, Buffer->Width, Buffer->Height,
                    0, 0, Buffer->Width, Buffer->Height,
                    Buffer->Memory, &Buffer->Info, DIB_RGB_COLORS, SRCCOPY);
    }
}


internal void
Win32ClearBuffer(win32_sound_output *SoundOutput) 
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize, &Region1, &Region1Size, &Region2, &Region2Size, 0) == DS_OK)
    {
        uint8 *DestByte = (uint8 *)Region1;
        for(DWORD ByteIndex = 0; ByteIndex < Region1Size; ByteIndex++)
        {
            *DestByte++ = 0;
        }
                        
        DestByte = (uint8 *)Region2;
        for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ByteIndex++)
        {
            *DestByte++ = 0;
        }
                        
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}


internal void 
Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite, game_sound_output_buffer *SourceBuffer)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1, &Region1Size, &Region2, &Region2Size, 0) == DS_OK)
    {
        DWORD Region1SampleCount = Region1Size/SoundOutput->BytesPerSample;
        int16 *DestSample = (int16 *) Region1;
        int16 *SourceSample = SourceBuffer->Samples;
        for(DWORD SampleIndex = 0; SampleIndex < Region1SampleCount; SampleIndex++)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;            

            SoundOutput->RunningSampleIndex++;
        }
                        
        DWORD Region2SampleCount = Region2Size/SoundOutput->BytesPerSample;
        DestSample = (int16 *) Region2;
        for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount; SampleIndex++)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;            

            SoundOutput->RunningSampleIndex++;
        }
                        
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
    }
}
 
internal void
Win32ProcessKeyboardMessage(game_button_state *NewState, bool32 IsDown)
{
    if(NewState->EndedDown != IsDown) 
    {
        NewState->EndedDown = IsDown;
        NewState->HalfTransitionCount++;
    }
}

internal void
Win32ProcessXInputDigitalButton(DWORD XInputButtonState, game_button_state OldState, DWORD ButtonBit, game_button_state *NewState)
{
    NewState->EndedDown = ((XInputButtonState & ButtonBit) == ButtonBit); 
    NewState->HalfTransitionCount = (OldState.EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal void 
CatStrings(int64 SourceACount, char *SourceA,
           int64 SourceBCount, char *SourceB,
           int64 DestCount, char *Dest)
{
    for (int Index = 0; Index < SourceACount; Index++)
    {
        *Dest++ = *SourceA++;
    }

    for (int Index = 0; Index < SourceBCount; Index++)
    {
        *Dest++ = *SourceB++;
    }

    *Dest = 0;
}

internal void
Win32BuildEXEPathFilename(win32_state *Win32State, char *Filename, int DestCount, char *Dest)
{
    CatStrings(Win32State->OnePastLastEXEFilenameSlash - Win32State->EXEFilename, Win32State->EXEFilename, 
               StringLength(Filename), Filename,
               DestCount, Dest);
}

internal void
Win32GetInputFileLocation(win32_state *State, bool32 InputStream, int SlotIndex, int DestCount, char *Dest)
{
    char Temp[64];
    wsprintf(Temp, "loop_edit_%d_%s.hmi", SlotIndex, InputStream ? "input" : "state");
    Win32BuildEXEPathFilename(State, Temp, DestCount, Dest);
}

internal win32_replay_buffer *
Win32GetReplayBuffer(win32_state *State, int unsigned Index)
{
    Assert(Index < ArrayCount(State->ReplayBuffers));
    win32_replay_buffer *Result = &State->ReplayBuffers[Index];
    return(Result);
}

internal void
Win32BeginRecordingInput(win32_state *State, int InputRecordingIndex)
{
    win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(State, InputRecordingIndex);
    if(ReplayBuffer->MemoryBlock)
    {
        State->InputRecordingIndex = InputRecordingIndex;

        char Filename[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation(State, true, InputRecordingIndex, sizeof(Filename), Filename);
        State->RecordingHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

#if 0
        LARGE_INTEGER FilePosition;
        FilePosition.QuadPart = State->TotalSize;  
        SetFilePointerEx(State->RecordingHandle, FilePosition, 0, FILE_BEGIN);
#endif

        CopyMemory(ReplayBuffer->MemoryBlock, State->GameMemoryBlock, State->TotalSize);
    }
}

internal void
Win32EndRecordingInput(win32_state *State)
{
	CloseHandle(State->RecordingHandle);
    State->RecordingHandle = 0;
	State->InputRecordingIndex = 0;
}

internal void
Win32BeginInputPlayback(win32_state *State, int InputPlayingIndex)
{
    win32_replay_buffer *ReplayBuffer = Win32GetReplayBuffer(State, InputPlayingIndex);
    if(ReplayBuffer->MemoryBlock)
    {
        State->InputPlayingIndex = InputPlayingIndex;

        char Filename[WIN32_STATE_FILE_NAME_COUNT];
        Win32GetInputFileLocation(State, true, InputPlayingIndex, sizeof(Filename), Filename);
        State->PlaybackHandle = CreateFileA(Filename, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);
#if 0
        LARGE_INTEGER FilePosition;
        FilePosition.QuadPart = State->TotalSize;  
        SetFilePointerEx(State->PlaybackHandle, FilePosition, 0, FILE_BEGIN);
#endif 

        CopyMemory(State->GameMemoryBlock, ReplayBuffer->MemoryBlock, State->TotalSize);
    }
}

internal void
Win32EndInputPlayback(win32_state *State)
{
	CloseHandle(State->PlaybackHandle);
    State->PlaybackHandle = 0;
	State->InputPlayingIndex = 0;
}

internal void
Win32RecordInput(win32_state *State, game_input *NewInput)
{
    DWORD BytesWritten;
    WriteFile(State->RecordingHandle, NewInput, sizeof(*NewInput), &BytesWritten, 0);
}

internal void 
Win32PlaybackInput(win32_state *State, game_input *NewInput)
{
    DWORD BytesRead = 0;
    if(ReadFile(State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0))
    {
        if(BytesRead == 0)
        {
            // NOTE(george): We've hit the end of the stream, go back to the beginning
            int PlayingIndex = State->InputPlayingIndex;
            Win32EndInputPlayback(State);
            Win32BeginInputPlayback(State, PlayingIndex);
            ReadFile(State->PlaybackHandle, NewInput, sizeof(*NewInput), &BytesRead, 0);
        }
    }
}

internal void
Win32ProcessPendingMessages(win32_state *Win32State, game_controller_input *KeyboardController)
{
    MSG Message;
    while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE)) 
    {
        switch(Message.message) 
        {
            case WM_QUIT:
            {
                GlobalRunning = false;
            } break;

            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYDOWN:
            case WM_KEYUP:
            {
                uint32 VKCode = (uint32) Message.wParam;

                // NOTE(george): Since we are comparing WasDown to IsDown,
                // we MUST use == and != to convert these bit tests to actual
                // 0 or 1 values.
                bool WasDown = ((Message.lParam & (1 << 30)) != 0);
                bool IsDown = ((Message.lParam & (1 << 31)) == 0);
                
                if (WasDown != IsDown)
                { 
                    if (VKCode == 'W')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveUp, IsDown);
                    }
                    else if (VKCode == 'A')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveLeft, IsDown);
                    }
                    else if (VKCode == 'S')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveDown, IsDown);
                    }
                    else if (VKCode == 'D')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->MoveRight, IsDown);
                    }
                    else if (VKCode == 'Q')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->LeftShoulder, IsDown);
                    }
                    else if (VKCode == 'E')
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->RightShoulder, IsDown);
                    }
                    else if (VKCode == VK_UP)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionUp, IsDown);
                    }
                    else if (VKCode == VK_LEFT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionLeft, IsDown);
                    }
                    else if (VKCode == VK_DOWN)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionDown, IsDown);
                    }
                    else if (VKCode == VK_RIGHT)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->ActionRight, IsDown);
                    }
                    else if (VKCode == VK_ESCAPE)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->Back, IsDown);
                    }
                    else if (VKCode == VK_SPACE)
                    {
                        Win32ProcessKeyboardMessage(&KeyboardController->Start, IsDown);
                    }
#if HANDMADE_INTERNAL
                    else if (VKCode == 'P')
                    {
                        if (IsDown)
                        {
                            GlobalPause = !GlobalPause;
                        }
                    }
                    else if (VKCode == 'L')
                    {
                        if (IsDown)
                        {
                            if(Win32State->InputPlayingIndex == 0)
                            {
                                if (Win32State->InputRecordingIndex == 0)
                                {
                                    Win32BeginRecordingInput(Win32State, 1);
                                }
                                else 
                                {
                                    Win32EndRecordingInput(Win32State);
                                    Win32BeginInputPlayback(Win32State, 1);
                                }
                            }
                            else
                            {
                                Win32EndInputPlayback(Win32State);
                            }
                        }
                    }
#endif
                    if(IsDown)
                    {
                        bool32 AltKeyWasDown = (Message.lParam & (1 << 29));
                        if ((VKCode == VK_F4) && AltKeyWasDown)
                        {
                            GlobalRunning = false;
                        }
                        if((VKCode == VK_RETURN) && AltKeyWasDown)
                        {
                            if(Message.hwnd)
                            {
                                ToggleFullscreen(Message.hwnd);
                            }
                        }
                    }
                }
            } break;

            default:
            {
                TranslateMessage(&Message);
                DispatchMessageA(&Message);
            } break;
        }
    }
}

LRESULT CALLBACK 
Win32MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;
    
    switch (Message)
    {
        case WM_SIZE:
        {
        } break;

        case WM_SETCURSOR:
        {
            if(DEBUGGlobalShowCursor)
            {
                Result = DefWindowProc(Window, Message, WParam, LParam);
            }
            else
            {
                SetCursor(0);
            }
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
#if 0
            if (WParam == TRUE)
            {
                SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 255, LWA_ALPHA);
            }
            else
            {
                SetLayeredWindowAttributes(Window, RGB(0, 0, 0), 64, LWA_ALPHA);
            }
#endif
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
            Assert(!"Keyboard input came in through a non-dispatch message!");
        } break;
        
        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }
    
    return(Result);
}

inline LARGE_INTEGER
Win32GetWallClock(void)
{
    LARGE_INTEGER Result;
    QueryPerformanceCounter(&Result);
    return(Result);
}

inline real32
Win32GetSecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End)
{
    real32 Result = (End.QuadPart - Start.QuadPart) / (real32)GlobalPerfCounterFrequency;
    return(Result);
}

#if 0
internal void
HandleDebugCycleCounters(game_memory *Memory)
{
#if HANDMADE_INTERNAL
    OutputDebugString("DEBUG CYCLE COUNTS:\n");
    for(int CounterIndex = 0;
        CounterIndex < ArrayCount(Memory->Counters);
        ++CounterIndex)
    {
        debug_cycle_counter *Counter = Memory->Counters + CounterIndex;

        if(Counter->HitCount)
        {
            char TextBuffer[256];
            _snprintf_s(TextBuffer, sizeof(TextBuffer), 
                        " %d: %I64ucy %uh %I64ucy/h\n", 
                        CounterIndex, 
                        Counter->CycleCount,
                        Counter->HitCount, 
                        Counter->CycleCount/Counter->HitCount);
            OutputDebugString(TextBuffer);
            Counter->CycleCount = 0;
            Counter->HitCount = 0;  
        }
    }
#endif
}
#endif

#if 0

internal void 
Win32DebugDrawVertical(win32_offscreen_buffer *Backbuffer, int X, int Top, int Bottom, uint32 Color)
{
    Top = Top <= 0 ? 0 : Top;
    Bottom = Bottom > Backbuffer->Height ? Backbuffer->Height : Bottom;     

    if ((X >= 0) && (X < Backbuffer->Width))
    {
        uint8 *Pixel = (uint8 *)Backbuffer->Memory + X*Backbuffer->BytesPerPixel + Top*Backbuffer->Pitch;
        for (int Y = Top; Y < Bottom; Y++)
        {
            *(uint32 *)Pixel = Color;
            Pixel += Backbuffer->Pitch;
        }
    }
}

inline void
Win32DrawSoundBufferMarker(win32_offscreen_buffer *Backbuffer, win32_sound_output *SoundOutput,
                           real32 C, int PadX, int Top, int Bottom, DWORD Value, uint32 Color)
{
    int X = PadX + (int)(C * (real32)Value);
    Win32DebugDrawVertical(Backbuffer, X, Top, Bottom, Color);
}

internal void
Win32DebugSyncDisplay(win32_offscreen_buffer *Backbuffer, 
                      int MarkerCount, win32_debug_time_marker *Markers, int CurrentMarkerIndex,
                      win32_sound_output *SoundOutput, real32 TargetSecondsPerFrame)
{
    int PadX = 16;
    int PadY = 16;

    int LineHeight = 64;

    real32 C = (real32)(Backbuffer->Width - 2*PadX) / (real32)SoundOutput->SecondaryBufferSize;
    for(int MarkerIndex = 0; MarkerIndex < MarkerCount; MarkerIndex++)
    {
        win32_debug_time_marker *ThisMarker = &Markers[MarkerIndex];        
        Assert(ThisMarker->OutputPlayCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputWriteCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputLocation < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->OutputByteCount < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->FlipPlayCursor < SoundOutput->SecondaryBufferSize);
        Assert(ThisMarker->FlipWriteCursor < SoundOutput->SecondaryBufferSize);

        DWORD PlayColor = 0xFFFFFFFF;
        DWORD WriteColor = 0xFFFF0000;
        DWORD ExpectedFlipColor = 0xFFFFFF00;
        DWORD PlayWindowColor = 0xFFFF00FF;

        int Top = PadY;
        int Bottom = PadY + LineHeight;
        if (MarkerIndex == CurrentMarkerIndex)
        {
            Top += LineHeight;
            Bottom += LineHeight;
            
            int FirstTop = Top;

            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputPlayCursor, PlayColor);
            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputWriteCursor, WriteColor);

            Top += LineHeight;
            Bottom += LineHeight;
            
            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation, PlayColor);
            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation + ThisMarker->OutputByteCount, WriteColor);
            
            Top += LineHeight;
            Bottom += LineHeight;

            Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, FirstTop, Bottom, ThisMarker->ExpectedFlipCursor, ExpectedFlipColor);            
        }

        Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor, PlayColor);
        Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor + 480*SoundOutput->BytesPerSample, PlayWindowColor);
        Win32DrawSoundBufferMarker(Backbuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipWriteCursor, WriteColor);
    }
}

#endif

internal void
Win32GetEXEFilename(win32_state *Win32State)
{
    DWORD SizeOfFilename = GetModuleFileName(0, Win32State->EXEFilename, sizeof(Win32State->EXEFilename));
    Win32State->OnePastLastEXEFilenameSlash = Win32State->EXEFilename;
    for(char *Scan = Win32State->EXEFilename; *Scan; Scan++)
    {
        if (*Scan == '\\')
        {
            Win32State->OnePastLastEXEFilenameSlash = Scan + 1;
        }
    }
}

struct platform_work_queue_entry
{
    platform_work_queue_callback *Callback;
    void *Data;
};

struct platform_work_queue
{
    uint32 volatile CompletionGoal;
    uint32 volatile CompletionCount;

    uint32 volatile NextEntryToWrite;
    uint32 volatile NextEntryToRead;
    HANDLE SemaphoreHandle;

    platform_work_queue_entry Entries[256];    
};

internal void 
Win32AddEntry(platform_work_queue *Queue, platform_work_queue_callback *Callback, void *Data)
{
    // TODO(george): Switch to InterlockedCompareExchange eventually
    // so that any thread can add?
    uint32 NewNextEntryToWrite = (Queue->NextEntryToWrite + 1) % ArrayCount(Queue->Entries);
    Assert(NewNextEntryToWrite != Queue->NextEntryToRead);
    platform_work_queue_entry *Entry = Queue->Entries + Queue->NextEntryToWrite;
    Entry->Callback = Callback; 
    Entry->Data = Data;
    Queue->CompletionGoal++;
    _WriteBarrier(); 
    Queue->NextEntryToWrite = NewNextEntryToWrite;
    ReleaseSemaphore(Queue->SemaphoreHandle, 1, 0);
}

internal bool32
Win32DoNextWorkQueueEntry(platform_work_queue *Queue)
{
    bool32 WeShouldSleep = false;

    uint32 OriginalNextEntryToRead = Queue->NextEntryToRead;
    uint32 NewNextEntryToRead = (OriginalNextEntryToRead + 1) % ArrayCount(Queue->Entries);;
    if(Queue->NextEntryToRead != Queue->NextEntryToWrite)
    {
        uint32 Index = InterlockedCompareExchange((LONG volatile *)&Queue->NextEntryToRead,
                                                  NewNextEntryToRead,
                                                  OriginalNextEntryToRead);
        if(Index == OriginalNextEntryToRead)
        {
            platform_work_queue_entry *Entry = Queue->Entries + Index;
            Entry->Callback(Queue, Entry->Data);
            InterlockedIncrement((LONG volatile *)&Queue->CompletionCount);
        }
    }
    else
    {
        WeShouldSleep = true;
    }

    return (WeShouldSleep);
}

inline void
Win32CompleteAllWork(platform_work_queue *Queue)
{
    while(Queue->CompletionGoal != Queue->CompletionCount) 
    { 
        Win32DoNextWorkQueueEntry(Queue);
    }

    Queue->CompletionGoal = 0;
    Queue->CompletionCount = 0;
}

struct win32_thread_info
{
    int32 LogicalThreadIndex;
    platform_work_queue *Queue;
};

DWORD WINAPI 
ThreadProc(LPVOID lpParameter)
{
    platform_work_queue *Queue = (platform_work_queue *)lpParameter;

    platform_work_queue_entry Entry = {};
    for(;;)
    {
        if(Win32DoNextWorkQueueEntry(Queue))
        {
            WaitForSingleObjectEx(Queue->SemaphoreHandle, INFINITE, FALSE);            
        }
    }

    // return (0);
}

internal PLATFORM_WORK_QUEUE_CALLBACK(DoWorkerWork)
{
    char Buffer[256];
    wsprintf(Buffer, "Thread %u: %s\n", GetCurrentThreadId(), (char *)Data);
    OutputDebugStringA(Buffer);
}

internal void
Win32MakeQueue(platform_work_queue *Queue, uint32 ThreadCount)
{
    Queue->CompletionCount = 0;
    Queue->CompletionGoal = 0;

    Queue->NextEntryToWrite = 0;
    Queue->NextEntryToRead = 0;

    uint32 InitialCount = 0;
    Queue->SemaphoreHandle = CreateSemaphoreEx(0, InitialCount, ThreadCount, 0, 0, SEMAPHORE_ALL_ACCESS);

    for(uint32 ThreadIndex = 0;
        ThreadIndex < ThreadCount;
        ThreadIndex++)
    {
        DWORD ThreadID;
        HANDLE ThreadHandle =  CreateThread(0, 0, ThreadProc, Queue, 0, &ThreadID);
        CloseHandle(ThreadHandle);
    }
}

struct win32_platform_file_handle
{
    HANDLE Win32Handle;
};

struct win32_platform_file_group
{
    HANDLE FindHandle;
    WIN32_FIND_DATAW FindData;
};

internal PLATFORM_GET_ALL_FILES_OF_TYPE_BEGIN(Win32GetAllFilesOfTypeBegin)
{
    platform_file_group Result = {};

    win32_platform_file_group *Win32FileGroup = (win32_platform_file_group *)VirtualAlloc(0, sizeof(win32_platform_file_group), MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
    Result.Platform = Win32FileGroup;

    wchar_t *WildCard = L"*.*";
    switch(Type)
    {
        case PlatformFileType_AssetFile:
        {
            WildCard = L"*.hha";
        } break;

        case PlatformFileType_SavedGameFile:
        {
            WildCard = L"*.hhs";
        } break;

        InvalidDefaultCase;
    }

    Result.FileCount = 0;

    WIN32_FIND_DATAW FindData;
    HANDLE FindHandle = FindFirstFileW(WildCard, &FindData);
    while(FindHandle != INVALID_HANDLE_VALUE)
    {
        Result.FileCount++;

        if(!FindNextFileW(FindHandle, &FindData))
        {
            break;
        }
    }
    FindClose((platform_file_group *)FindHandle);

    Win32FileGroup->FindHandle = FindFirstFileW(WildCard, &Win32FileGroup->FindData);

    return(Result);
}

internal PLATFORM_GET_ALL_FILES_OF_TYPE_END(Win32GetAllFilesOfTypeEnd)
{
    win32_platform_file_group *Win32FileGroup = (win32_platform_file_group *)FileGroup->Platform;
    if(Win32FileGroup)
    {
        FindClose(Win32FileGroup->FindHandle);

        VirtualFree(Win32FileGroup, 0, MEM_RELEASE);
    }
}

internal PLATFORM_OPEN_NEXT_FILE(Win32OpenNextFile)
{
    win32_platform_file_group *Win32FileGroup = (win32_platform_file_group *)FileGroup->Platform;
    platform_file_handle Result = {};

    if(Win32FileGroup->FindHandle != INVALID_HANDLE_VALUE)
    {
        // TODO(georgy): If we want, someday, make an actual arena used by Win32
        win32_platform_file_handle *Win32Handle = (win32_platform_file_handle *)VirtualAlloc(0, sizeof(win32_platform_file_handle), 
                                                                                MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
        Result.Platform = Win32Handle;

        if(Win32Handle)
        {
            wchar_t *Filename = Win32FileGroup->FindData.cFileName;
            Win32Handle->Win32Handle = CreateFileW(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
            Result.NoErrors = (Win32Handle->Win32Handle != INVALID_HANDLE_VALUE);
        }

        if(!FindNextFileW(Win32FileGroup->FindHandle, &Win32FileGroup->FindData))
        {
            FindClose(Win32FileGroup->FindHandle);
            Win32FileGroup->FindHandle = INVALID_HANDLE_VALUE;
        }
    }

    return(Result);
}

internal PLATFORM_FILE_ERROR(Win32FileError)
{
#if HANDMADE_INTERNAL
    OutputDebugString("WIN32 FILE ERROR: ");
    OutputDebugString(Message);
    OutputDebugString("\n");
#endif
    Handle->NoErrors = false;
}

internal PLATFORM_READ_DATA_FROM_FILE(Win32ReadDataFromFile)
{
    if(PlatformNoFileErrors(Source))
    {
        win32_platform_file_handle *Handle = (win32_platform_file_handle *)Source->Platform;

        OVERLAPPED Overlapped = {};
        Overlapped.Offset = (uint32)(Offset & 0xFFFFFFFF);
        Overlapped.OffsetHigh = (uint32)((Offset >> 32) & 0xFFFFFFFF);

        uint32 FileSize32 = SafeTruncateUInt64(Size);

        DWORD BytesRead;
        if ((ReadFile(Handle->Win32Handle, Dest, FileSize32, &BytesRead, &Overlapped)) && 
            (FileSize32 == BytesRead))
        {
            // NOTE(george): File read succeeded!
        }
        else
        {
            Win32FileError(Source, "Read file failed.");
        }
    }
}

/*

internal PLATFORM_FILE_ERROR(Win32CloseFile)
{
    CloseHandle(FileHandle);
}

*/

PLATFORM_ALLOCATE_MEMORY(Win32AllocateMemory)
{
    void *Result = VirtualAlloc(0, Size, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

    return(Result);
}

PLATFORM_DEALLOCATE_MEMORY(Win32DeallocateMemory)
{
    if(Memory)
    {
        VirtualFree(Memory, 0, MEM_RELEASE);
    }
}

#if HANDMADE_INTERNAL
global_variable debug_table GlobalDebugTable_;
debug_table *GlobalDebugTable = &GlobalDebugTable_;
#endif

int CALLBACK 
WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCode)
{
    win32_state Win32State = {};    

    platform_work_queue HighPriorityQueue;
    Win32MakeQueue(&HighPriorityQueue, 2);

    platform_work_queue LowPriorityQueue;
    Win32MakeQueue(&LowPriorityQueue, 2);

#if 0
    Win32AddEntry(&Queue, DoWorkerWork, "String A0");
    Win32AddEntry(&Queue, DoWorkerWork, "String A1");
    Win32AddEntry(&Queue, DoWorkerWork, "String A2");
    Win32AddEntry(&Queue, DoWorkerWork, "String A3");
    Win32AddEntry(&Queue, DoWorkerWork, "String A4");
    Win32AddEntry(&Queue, DoWorkerWork, "String A5");
    Win32AddEntry(&Queue, DoWorkerWork, "String A6");
    Win32AddEntry(&Queue, DoWorkerWork, "String A7");
    Win32AddEntry(&Queue, DoWorkerWork, "String A8");
    Win32AddEntry(&Queue, DoWorkerWork, "String A9");

    Win32AddEntry(&Queue, DoWorkerWork, "String B0");
    Win32AddEntry(&Queue, DoWorkerWork, "String B1");
    Win32AddEntry(&Queue, DoWorkerWork, "String B2");
    Win32AddEntry(&Queue, DoWorkerWork, "String B3");
    Win32AddEntry(&Queue, DoWorkerWork, "String B4");
    Win32AddEntry(&Queue, DoWorkerWork, "String B5");
    Win32AddEntry(&Queue, DoWorkerWork, "String B6");
    Win32AddEntry(&Queue, DoWorkerWork, "String B7");
    Win32AddEntry(&Queue, DoWorkerWork, "String B8");
    Win32AddEntry(&Queue, DoWorkerWork, "String B9");

    Win32CompleteAllWork(&Queue);
#endif

    LARGE_INTEGER PerfCounterFrequencyResult;
    QueryPerformanceFrequency(&PerfCounterFrequencyResult);
    GlobalPerfCounterFrequency = PerfCounterFrequencyResult.QuadPart;

    Win32GetEXEFilename(&Win32State);

    char SourceGameCodeFullPath[MAX_PATH];
    Win32BuildEXEPathFilename(&Win32State, "handmade.dll", 
                              sizeof(SourceGameCodeFullPath), SourceGameCodeFullPath);

    char TempGameCodeFullPath[MAX_PATH];
    Win32BuildEXEPathFilename(&Win32State, "handmade_temp.dll", 
                              sizeof(TempGameCodeFullPath), TempGameCodeFullPath);   

    char GameCodeLockFullPath[MAX_PATH];
    Win32BuildEXEPathFilename(&Win32State, "lock.tmp", 
                              sizeof(GameCodeLockFullPath), GameCodeLockFullPath);

    // NOTE(george): Set the Windows scheduler granularity to 1 ms
    // so that out Sleep() can be more granular
    UINT DesiredSchedulerMS = 1;
    bool32 SleepIsGranular = (timeBeginPeriod(DesiredSchedulerMS) == TIMERR_NOERROR);

    Win32LoadXInput();

#if HANDMADE_INTERNAL
    DEBUGGlobalShowCursor = true;
#endif
    WNDCLASSA WindowClass = {};
    
    /* NOTE(george): 1080p display mode is 1920x1080 -> Half of that is 960x540
       1920 -> 2048 = 2048-1920 -> 128 pixels
       1080 -> 2048 = 2048-1080 -> 968 pixels
       1024 + 128 = 1152
    */  
    // Win32ResizeDIBSection(&GlobalBackbuffer, 1366, 768);
    Win32ResizeDIBSection(&GlobalBackbuffer, 960, 540);
    
    WindowClass.style = CS_VREDRAW | CS_HREDRAW;
    WindowClass.lpfnWndProc = Win32MainWindowCallback;
    WindowClass.hInstance = Instance;
    //	WindowClass.hIcon = ;
    WindowClass.lpszClassName = "HandmadeHeroWindowClass";

    if (RegisterClassA(&WindowClass))
    {
        HWND Window = CreateWindowExA(
            0, // WS_EX_TOPMOST | WS_EX_LAYERED,
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
            // NOTE(george): DirestSound output test
            win32_sound_output SoundOutput = {};

            // TODO(george): How do we reliably query on this on Windows?
            int MonitorRefreshHz = 60;
            HDC RefreshDC = GetDC(Window);
            int Win32RefreshRate = GetDeviceCaps(RefreshDC, VREFRESH);
            ReleaseDC(Window, RefreshDC);
            if (Win32RefreshRate > 0) 
            {
                MonitorRefreshHz = Win32RefreshRate;
            }
            real32 GameUpdateHz = (MonitorRefreshHz / 2.0f);
            real32 TargetSecondsPerFrame = 1.0f / GameUpdateHz;

            SoundOutput.SamplesPerSecond = 48000;
            SoundOutput.BytesPerSample = sizeof(int16)*2;
            SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample;

            // TODO(george): Actually compute this variance and see 
            // what the lowest reasonable value is.      
            SoundOutput.SafetyBytes = (DWORD)((real32)(SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample) / GameUpdateHz / 3.0f);
            Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
            Win32ClearBuffer(&SoundOutput);

            // TODO(georgy): Remove MaxPossibleOverrun
            uint32 MaxPossibleOverrun = 8*2*sizeof(int16);
            int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize + MaxPossibleOverrun, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
            LPVOID BaseAddress = (LPVOID)Terabytes(2);
#else 
            LPVOID BaseAddress = 0;
#endif

            game_memory GameMemory = {};
            GameMemory.PermanentStorageSize = Megabytes(256);
            GameMemory.TransientStorageSize = Gigabytes(1);
            GameMemory.DebugStorageSize = Megabytes(64);
            GameMemory.HighPriorityQueue = &HighPriorityQueue;
            GameMemory.LowPriorityQueue = &LowPriorityQueue;
            GameMemory.PlatformAPI.AddEntry = Win32AddEntry;
            GameMemory.PlatformAPI.CompleteAllWork = Win32CompleteAllWork;

            GameMemory.PlatformAPI.GetAllFilesOfTypeBegin = Win32GetAllFilesOfTypeBegin;
            GameMemory.PlatformAPI.GetAllFilesOfTypeEnd = Win32GetAllFilesOfTypeEnd;
            GameMemory.PlatformAPI.OpenNextFile = Win32OpenNextFile;
            GameMemory.PlatformAPI.ReadDataFromFile = Win32ReadDataFromFile;
            GameMemory.PlatformAPI.FileError = Win32FileError;

            GameMemory.PlatformAPI.AllocateMemory = Win32AllocateMemory;
            GameMemory.PlatformAPI.DeallocateMemory = Win32DeallocateMemory;

#if HANDMADE_INTERNAL
            GameMemory.PlatformAPI.DEBUGReadEntireFile = DEBUGPlatformReadEntireFile;
            GameMemory.PlatformAPI.DEBUGFreeFileMemory = DEBUGPlatformFreeFileMemory;
            GameMemory.PlatformAPI.DEBUGWriteEntireFile = DEBUGPlatformWriteEntireFile;
            GameMemory.PlatformAPI.DEBUGExecuteSystemCommand = DEBUGExecuteSystemCommand;
            GameMemory.PlatformAPI.DEBUGGetProcessState = DEBUGGetProcessState;
#endif

            // TODO(george): TransientStorage needs to be broken up
            // into game transient and cache transient, and only
            // the former need to be saved for state playback.
            
            // TODO(george): Use MEM_LARGE_PAGES and call adjust token privileges when not on Windows XP?
            Win32State.TotalSize = GameMemory.PermanentStorageSize + 
                                   GameMemory.TransientStorageSize + 
                                   GameMemory.DebugStorageSize;
            Win32State.GameMemoryBlock = VirtualAlloc(BaseAddress, Win32State.TotalSize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            GameMemory.PermanentStorage = Win32State.GameMemoryBlock;
            GameMemory.TransientStorage = ((uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);
            GameMemory.DebugStorage = ((uint8 *)GameMemory.TransientStorage + GameMemory.TransientStorageSize);
            
            for (int ReplayIndex = 0; ReplayIndex < ArrayCount(Win32State.ReplayBuffers); ReplayIndex++)
            {
                win32_replay_buffer *ReplayBuffer = &Win32State.ReplayBuffers[ReplayIndex];

                // TODO(george): Recording system still seems to take too long
                // on record start - find out what Windows is doing and if
                // we can speed up / defer some of that processing.

                Win32GetInputFileLocation(&Win32State, false, ReplayIndex, 
                                            sizeof(ReplayBuffer->Filename), ReplayBuffer->Filename);

                ReplayBuffer->FileHandle = 
                    CreateFileA(ReplayBuffer->Filename, GENERIC_WRITE|GENERIC_READ, 0, 0, CREATE_ALWAYS, 0, 0);           

                LARGE_INTEGER MaxSize;
                MaxSize.QuadPart = Win32State.TotalSize;
                ReplayBuffer->MemoryMap = CreateFileMappingA(
                    ReplayBuffer->FileHandle, 0, PAGE_READWRITE,
                    MaxSize.HighPart, MaxSize.LowPart, 0);

                ReplayBuffer->MemoryBlock = MapViewOfFile(ReplayBuffer->MemoryMap, FILE_MAP_ALL_ACCESS, 0, 0, Win32State.TotalSize);
                if (ReplayBuffer->MemoryBlock)
                {
                }
                else
                {
                    // TODO(george): Diagnostic                    
                }
            }

            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

            GlobalRunning = true;            

#if 0
            // NOTE(george): This tests the PlayCursor/WriteCursor update frequency
            // On my machine, it is 480 samples
            while(GlobalRunning)
            {
                DWORD PlayCursor;
                DWORD WriteCursor;
                GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);
                char TextBuffer[256];
                _snprintf_s(TextBuffer, sizeof(TextBuffer), 
                                    "PC:%u WC:%u\n", PlayCursor, WriteCursor);
                OutputDebugString(TextBuffer);
            }
#endif
            if (Samples && GameMemory.PermanentStorage && GameMemory.TransientStorage)
            {
                game_input Input[2] = {};
                game_input *NewInput = &Input[0];
                game_input *OldInput = &Input[1];

                LARGE_INTEGER LastCounter = Win32GetWallClock();
                LARGE_INTEGER FlipWallClock = Win32GetWallClock();

                int DebugTimeMarkerIndex = 0;
                win32_debug_time_marker DebugTimeMarkers[15] = {};

                bool32 SoundIsValid = false;
                DWORD AudioLatencyBytes = 0;
                real32 AudioLatencySeconds = 0;

                win32_game_code Game = Win32LoadGameCode(SourceGameCodeFullPath, TempGameCodeFullPath, GameCodeLockFullPath);

                while (GlobalRunning)
                {   
                    // 
                    // 
                    // 
                    BEGIN_BLOCK(ExecutableRefresh);
                    NewInput->dtForFrame = TargetSecondsPerFrame;

                    GameMemory.ExecutableReloaded = false;
                    FILETIME NewDLLWriteTime = Win32GetLastWriteTime(SourceGameCodeFullPath);
                    if (CompareFileTime(&NewDLLWriteTime, &Game.DLLLastWriteTime) != 0)
                    {
                        Win32CompleteAllWork(&HighPriorityQueue);
                        Win32CompleteAllWork(&LowPriorityQueue);
#if HANDMADE_INTERNAL
                        GlobalDebugTable = &GlobalDebugTable_;
#endif
                        Win32UnloadGameCode(&Game);
                        Game = Win32LoadGameCode(SourceGameCodeFullPath, TempGameCodeFullPath, GameCodeLockFullPath);
                        GameMemory.ExecutableReloaded = true;
                    }
                    END_BLOCK(ExecutableRefresh);

                    // 
                    // 
                    // 

                    BEGIN_BLOCK(InputProcessing);

                    game_controller_input *OldKeyboardController = GetController(OldInput, 0);
                    game_controller_input *NewKeyboardController = GetController(NewInput, 0);
                    *NewKeyboardController = {};
                    NewKeyboardController->IsConnected = true;
                    for(int ButtonIndex = 0; ButtonIndex < ArrayCount(NewKeyboardController->Buttons); ButtonIndex++)
                    {
                        NewKeyboardController->Buttons[ButtonIndex].EndedDown =
                                                OldKeyboardController->Buttons[ButtonIndex].EndedDown;
                    }
                    
                    Win32ProcessPendingMessages(&Win32State, NewKeyboardController);
                    
                    if (!GlobalPause) 
                    {
                        POINT MouseP;
                        GetCursorPos(&MouseP);
                        ScreenToClient(Window, &MouseP); 
                        NewInput->MouseX = (real32)MouseP.x + (-0.5f*(real32)GlobalBackbuffer.Width + 0.5f);
                        NewInput->MouseY = (0.5f*(real32)GlobalBackbuffer.Height - 0.5f) - (real32)MouseP.y;
                        NewInput->MouseZ = 0; // TODO(george): Support mousewheel?

                        DWORD WinButtonID[PlatformMouseButton_Count] = 
                        {
                            VK_LBUTTON,
                            VK_MBUTTON,
                            VK_RBUTTON,
                            VK_XBUTTON1,
                            VK_XBUTTON2
                        };
                        for(uint32 ButtonIndex = 0;
                            ButtonIndex < PlatformMouseButton_Count;
                            ButtonIndex++)
                        {
                            NewInput->MouseButtons[ButtonIndex] = OldInput->MouseButtons[ButtonIndex];
                            NewInput->MouseButtons[ButtonIndex].HalfTransitionCount = 0;
                            Win32ProcessKeyboardMessage(&NewInput->MouseButtons[ButtonIndex], 
                                                        GetKeyState(WinButtonID[ButtonIndex]) & (1 << 15));
                        }

                        // TODO(george): Need to not poll disconnected controllers to avoid
                        // xinput frame rate hit on older libraries...
                        // TODO(george): Should we poll this more frequently?
                        DWORD MaxControllerCount = XUSER_MAX_COUNT;
                        if (MaxControllerCount > ArrayCount(NewInput->Controllers) - 1)
                        {
                            MaxControllerCount = ArrayCount(NewInput->Controllers) - 1;
                        }
                        for (DWORD ControllerIndex = 0; ControllerIndex < MaxControllerCount; ControllerIndex++)
                        {
                            DWORD OurControllerIndex = ControllerIndex + 1;
                            game_controller_input *OldController = GetController(OldInput, OurControllerIndex);
                            game_controller_input *NewController = GetController(NewInput, OurControllerIndex);

                            XINPUT_STATE ControllerState;
                            if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
                            {
                                // NOTE(george): This controller is plugged in
                                NewController->IsConnected = true;
                                NewController->IsAnalog = OldController->IsAnalog;

                                // TODO(george): See if ControllerState.dwPacketNumber increments too rapidly
                                XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

                                NewController->StickAverageX =  Win32ProcessXInputStickValue(Pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                NewController->StickAverageY = Win32ProcessXInputStickValue(Pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
                                if(NewController->StickAverageX || NewController->StickAverageY)
                                {
                                    NewController->IsAnalog = true;
                                }

                                real32 Threshold = 0.5f;
                                Win32ProcessXInputDigitalButton((NewController->StickAverageX < -Threshold) ? 1 : 0, OldController->MoveLeft, 1, &NewController->MoveLeft);
                                Win32ProcessXInputDigitalButton((NewController->StickAverageX > Threshold) ? 1 : 0, OldController->MoveRight, 1, &NewController->MoveLeft);
                                Win32ProcessXInputDigitalButton((NewController->StickAverageY < -Threshold) ? 1 : 0, OldController->MoveDown, 1, &NewController->MoveLeft);
                                Win32ProcessXInputDigitalButton((NewController->StickAverageY > Threshold) ? 1 : 0, OldController->MoveUp, 1, &NewController->MoveLeft);

                                if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
                                {
                                    NewController->StickAverageY = 1.0f;
                                    NewController->IsAnalog = false;
                                }

                                if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                                {
                                    NewController->StickAverageY = -1.0f;                                
                                    NewController->IsAnalog = false;
                                }

                                if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                                {
                                    NewController->StickAverageX = -1.0f;
                                    NewController->IsAnalog = false;
                                }

                                if(Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                                {
                                    NewController->StickAverageX = 1.0f;                                
                                    NewController->IsAnalog = false;
                                }

                                Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->ActionDown, XINPUT_GAMEPAD_A, &NewController->ActionDown);
                                Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->ActionRight, XINPUT_GAMEPAD_B, &NewController->ActionRight);
                                Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->ActionLeft, XINPUT_GAMEPAD_X, &NewController->ActionLeft);
                                Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->ActionUp, XINPUT_GAMEPAD_Y, &NewController->ActionUp);
                                Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &NewController->LeftShoulder);
                                Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &NewController->RightShoulder);

                                Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->Start, XINPUT_GAMEPAD_START, &NewController->Start);
                                Win32ProcessXInputDigitalButton(Pad->wButtons, OldController->Back, XINPUT_GAMEPAD_BACK, &NewController->Back);
                            }
                            else
                            {
                                // NOTE(george): This controller is not available
                                NewController->IsConnected = false;
                            }
                        }
                        END_BLOCK(InputProcessing);

                        // 
                        // 
                        // 

                        BEGIN_BLOCK(GameUpdate);

                        game_offscreen_buffer Buffer = {};
                        Buffer.Memory = GlobalBackbuffer.Memory;
                        Buffer.Height = GlobalBackbuffer.Height;
                        Buffer.Width = GlobalBackbuffer.Width;
                        Buffer.Pitch = GlobalBackbuffer.Pitch;

                        if(Win32State.InputRecordingIndex)
                        {
                            Win32RecordInput(&Win32State, NewInput);
                        }

                        if(Win32State.InputPlayingIndex)
                        {
                            game_input Temp = *NewInput;
                            Win32PlaybackInput(&Win32State, NewInput);
                            for(uint32 MouseButtonIndex = 0;
                                MouseButtonIndex < PlatformMouseButton_Count;
                                MouseButtonIndex++)
                            {
                                NewInput->MouseButtons[MouseButtonIndex] = Temp.MouseButtons[MouseButtonIndex];
                            }
                            NewInput->MouseX = Temp.MouseX;
                            NewInput->MouseY = Temp.MouseY;
                            NewInput->MouseZ = Temp.MouseZ;
                        }

                        if (Game.UpdateAndRender)
                        {
                            Game.UpdateAndRender(&GameMemory, NewInput, &Buffer);
                            // HandleDebugCycleCounters(&GameMemory);
                        }

                        END_BLOCK(GameUpdate);

                        // 
                        // 
                        // 

                        BEGIN_BLOCK(AudioUpdate);

                        LARGE_INTEGER AudioWallClock = Win32GetWallClock();
                        real32 FromBeginToAudioSeconds = Win32GetSecondsElapsed(FlipWallClock, AudioWallClock);                        

                        DWORD PlayCursor;
                        DWORD WriteCursor;
                        if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
                        {
                            /*  NOTE(george): 

                                Here is how sound output computation works.

                                We define a safety value that is the number
                                of samples we think out game update loop
                                may vary by (let's say up to 2ms).

                                When we wake up to write audio, we will look
                                and see what the play cursor position is and we 
                                will forecast ahead where we think the
                                play cursor will be on the next frame boundary.

                                We will then look to see if the write cursor is
                                before that by at least our safety value. If it is, the 
                                target fill position is that frame boundary
                                plus one frame. This gives us perfect audio
                                sync in the case of a card that has low enough 
                                latency.

                                If the write cursor is after that safety margin,
                                then we assume we can never sync the
                                audio perfectly, so we will write one frame's
                                worth of audio plus some number of guard samples.
                            */

                            if (!SoundIsValid)
                            {
                                SoundOutput.RunningSampleIndex = WriteCursor / SoundOutput.BytesPerSample;
                                SoundIsValid = true;
                            }

                            DWORD ByteToLock = (SoundOutput.RunningSampleIndex*SoundOutput.BytesPerSample) % 
                                                SoundOutput.SecondaryBufferSize;

                            DWORD ExpectedSoundBytesPerFrame = (DWORD)((real32)(SoundOutput.SamplesPerSecond*SoundOutput.BytesPerSample) 
                                                                        / GameUpdateHz);

                            real32 SecondsLeftUntilFlip = TargetSecondsPerFrame - FromBeginToAudioSeconds;
                            DWORD ExpectedBytesUntilFlip = (DWORD)((SecondsLeftUntilFlip/TargetSecondsPerFrame)*(real32)ExpectedSoundBytesPerFrame);

                            DWORD ExpectedFrameBoundaryByte = PlayCursor + ExpectedBytesUntilFlip;

                            DWORD SafeWriteCursor = WriteCursor;
                            if (SafeWriteCursor < PlayCursor)
                            {
                                SafeWriteCursor += SoundOutput.SecondaryBufferSize;
                            }
                            Assert(SafeWriteCursor >= PlayCursor);
                            SafeWriteCursor += SoundOutput.SafetyBytes;

                            bool32 AudioCardIsLowLatency = (SafeWriteCursor < ExpectedFrameBoundaryByte);

                            DWORD TargetCursor = 0;
                            if (AudioCardIsLowLatency)
                            {
                                TargetCursor = (ExpectedFrameBoundaryByte + ExpectedSoundBytesPerFrame);
                            }
                            else
                            {
                                TargetCursor = (WriteCursor + ExpectedSoundBytesPerFrame + SoundOutput.SafetyBytes);
                            }
                            TargetCursor %= SoundOutput.SecondaryBufferSize;

                            DWORD BytesToWrite = 0;
                            if (ByteToLock > TargetCursor)
                            {
                                BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
                                BytesToWrite += TargetCursor;
                            }
                            else 
                            {
                                BytesToWrite = TargetCursor - ByteToLock;   
                            }

                            game_sound_output_buffer SoundBuffer = {};
                            SoundBuffer.Samples = Samples;
                            SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
                            SoundBuffer.SampleCount = Align8(BytesToWrite / SoundOutput.BytesPerSample);
                            BytesToWrite = SoundBuffer.SampleCount*SoundOutput.BytesPerSample;

                            if (Game.GetSoundSamples)
                            {
                                Game.GetSoundSamples(&GameMemory, &SoundBuffer);
                            }
                        
#if HANDMADE_INTERNAL
                            win32_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
                            Marker->OutputPlayCursor = PlayCursor;
                            Marker->OutputWriteCursor = WriteCursor;
                            Marker->OutputLocation = ByteToLock;
                            Marker->OutputByteCount = BytesToWrite;
                            Marker->ExpectedFlipCursor = ExpectedFrameBoundaryByte;

                            DWORD UnwrappedWriteCursor = WriteCursor;
                            if (UnwrappedWriteCursor < PlayCursor)
                            {
                                UnwrappedWriteCursor += SoundOutput.SecondaryBufferSize;
                            }
                            AudioLatencyBytes = UnwrappedWriteCursor - PlayCursor;
                            AudioLatencySeconds = ((real32)AudioLatencyBytes / (real32)SoundOutput.BytesPerSample) / 
                                                    (real32)SoundOutput.SamplesPerSecond;

#if 0
                            char TextBuffer[256];
                            _snprintf_s(TextBuffer, sizeof(TextBuffer), 
                                        "BTL:%u TC:%u BTW:%u - PC:%u WC:%u DELTA:%u (%fs)\n", 
                                        ByteToLock, TargetCursor, BytesToWrite, 
                                        PlayCursor, WriteCursor, AudioLatencyBytes, AudioLatencySeconds);
                            OutputDebugString(TextBuffer);
#endif
#endif
                            Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
                        }
                        else
                        {
                            SoundIsValid = false;
                        }

                        END_BLOCK(AudioUpdate);

                        // 
                        // 
                        // 

#if HANDMADE_INTERNAL
                        BEGIN_BLOCK(CollationTime);

                        if(Game.DEBUGFrameEnd)
                        {
                            GlobalDebugTable = Game.DEBUGFrameEnd(&GameMemory, NewInput, &Buffer);
                        }
                        GlobalDebugTable_.EventArrayIndex_EventIndex = 0;
                        END_BLOCK(CollationTime);
#endif

                        // 
                        // 
                        // 

                        BEGIN_BLOCK(FrameWait);

                        LARGE_INTEGER WorkCounter = Win32GetWallClock();
                        real32 WorkSecondsElapsed = Win32GetSecondsElapsed(LastCounter, WorkCounter);
                        
#if 0
                        // TODO(george): Not tested yet! Probably buggy!
                        real32 SecondsElapsedForFrame = WorkSecondsElapsed;
                        if (SecondsElapsedForFrame < TargetSecondsPerFrame)
                        {
                            if (SleepIsGranular)
                            {
                                DWORD SleepMS = (DWORD)(1000.0f * (TargetSecondsPerFrame - SecondsElapsedForFrame));

                                if(SleepMS > 0)
                                {
                                    Sleep(SleepMS);
                                }
                            }
                    
                            real32 TestSecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());

                            if (TestSecondsElapsedForFrame > TargetSecondsPerFrame)
                            {
                                // TODO(george): LOG MISS HERE
                            }

                            while(SecondsElapsedForFrame < TargetSecondsPerFrame)
                            {
                                SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
                            }
                        }
                        else
                        {
                            // TODO(george): MISSED FRAME RATE
                            // TODO(george): Logging
                        }
                        
#else
                        real32 SecondsElapsedForFrame = WorkSecondsElapsed;
                        while (SecondsElapsedForFrame < TargetSecondsPerFrame)
                        {
                            SecondsElapsedForFrame = Win32GetSecondsElapsed(LastCounter, Win32GetWallClock());
                        }
#endif
                        END_BLOCK(FrameWait);

                        // 
                        // 
                        // 

                        BEGIN_BLOCK(FrameDisplay);

                        win32_window_dimension Dimension = GetWindowDimenstion(Window);

                        HDC DeviceContext = GetDC(Window);
                        Win32DisplayBufferInWindow(&GlobalBackbuffer, DeviceContext, Dimension.Width, Dimension.Height);
                        ReleaseDC(Window, DeviceContext);         

                        FlipWallClock = Win32GetWallClock();              

                        // TODO(george): Should I clean this?
                        game_input *Temp = NewInput;
                        NewInput = OldInput;
                        OldInput = Temp;                        

                        END_BLOCK(FrameDisplay);

                        LARGE_INTEGER EndCounter = Win32GetWallClock();  
                        FRAME_MARKER(Win32GetSecondsElapsed(LastCounter, EndCounter));
                        LastCounter = EndCounter;

#if HANDMADE_INTERNAL
                        if(GlobalDebugTable)
                        {
                            // TODO(georgy): Move this to a global variable so that
                            // there can be timers below this one?
                            GlobalDebugTable->RecordCount[TRANSLATION_UNIT_INDEX] = __COUNTER__;
                        }
#endif
                    }
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
    }
    else
    {
        // TODO(george): Logging
    }
    
    return (0);
}