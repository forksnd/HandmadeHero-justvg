#if !defined(WIN32_HANDMADE_H)

struct win32_sound_output
{
    int SamplesPerSecond;
    uint32 RunningSampleIndex;
    int32 BytesPerSample;
    int32 SecondaryBufferSize;
    int LatencySampleCount;
};

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

#define WIN32_HANDMADE_H
#endif