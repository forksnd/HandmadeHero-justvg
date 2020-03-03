#if !defined(WIN32_HANDMADE_H)
#define WIN32_HANDMADE_H

struct win32_sound_output
{
    int SamplesPerSecond;
    uint32 RunningSampleIndex;
    int32 BytesPerSample;
    DWORD SecondaryBufferSize;
    int LatencySampleCount;
    DWORD SafetyBytes;
    // TODO(george): Should running sample index be in bytes as well
    // TODO(george): Math gets simpler if we add a "bytes per second" field?
};

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct win32_window_dimension
{
    int Width;
    int Height;
};

struct win32_game_code
{
    HMODULE GameCodeDLL;
    FILETIME DLLLastWriteTime;

    // IMPORTANT(george): Either of the callbacks can be 0!
    // You must check before calling.
    game_update_and_render *UpdateAndRender;
    game_get_sound_samples *GetSoundSamples;
    debug_game_frame_end *DEBUGFrameEnd;

    bool32 IsValid;
};

struct win32_debug_time_marker 
{
    DWORD OutputPlayCursor;
    DWORD OutputWriteCursor;
    DWORD OutputLocation;
    DWORD OutputByteCount;
    DWORD ExpectedFlipCursor;

    DWORD FlipPlayCursor;
    DWORD FlipWriteCursor;
};

enum win32_fader_state
{
    Win32Fade_FadingIn,
    Win32Fade_WaitingForShow,
    Win32Fade_Inactive,
    Win32Fade_FadingGame,
    Win32Fade_FadingOut,
    Win32Fade_WaitingForClose
};
struct win32_fader
{
    HWND Window;

    r32 Alpha;
    win32_fader_state State;
};

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
struct win32_replay_buffer
{
    HANDLE FileHandle;
    HANDLE MemoryMap;
    char Filename[WIN32_STATE_FILE_NAME_COUNT];
    void *MemoryBlock;
};
struct win32_state
{
    uint64 TotalSize;
    void *GameMemoryBlock;
    win32_replay_buffer ReplayBuffers[4];

    HANDLE RecordingHandle;
    int InputRecordingIndex;

    HANDLE PlaybackHandle;
    int InputPlayingIndex;

    char EXEFilename[WIN32_STATE_FILE_NAME_COUNT];
    char *OnePastLastEXEFilenameSlash;
};

struct win32_thread_startup
{
    HWND Window;
    HGLRC OpenGLRC;
    platform_work_queue *Queue;
};

#endif