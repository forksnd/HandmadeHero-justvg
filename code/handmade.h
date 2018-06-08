#if !defined(HANDMADE_H)

/*
    NOTE(george):
        HANDMADE_INTERNAL:
            0 - Build for public release
            1 - Build for developer only
        
        HANDMADE_SLOW:
            0 - Not slow code allowed!
            1 - Slow code welcome
*/

#if HANDMADE_SLOW
#define Assert(Expresion) if (!(Expresion)) { *(int *) 0 = 0; }
#else
#define Assert(Expression)
#endif

#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

#define ArrayCount(Array) (sizeof(Array)/sizeof((Array)[0]))

// TODO(george): swap, min, max... Macros?

/*
	TODO(george): Services that the platform layer provides to the game
*/

/*
	TODO(george): Services that the game provides to the platform layer
	(this may expand in the future - sound on separate thread, etc.)
*/

// FOUR THINGS - timing, controller/keyboard input, bitmap buffer to use, sound buffer to use
struct game_offscreen_buffer
{
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

struct game_sound_output_buffer
{
    int16 *Samples;
    int SamplesPerSecond;
    int SampleCount;
};

struct game_button_state
{
    int HalfTransitionCount;
    bool32 EndedDown;
};

struct game_controller_input
{
    bool32 IsAnalog;

    real32 StartX;
    real32 StartY;

    real32 MinX;
    real32 MinY;
    
    real32 MaxX;
    real32 MaxY;

    real32 EndX;
    real32 EndY;

    union 
    {
        game_button_state Buttons[6];
        struct 
        {
            game_button_state Up;
            game_button_state Down;
            game_button_state Left;
            game_button_state Right;
            game_button_state LeftShoulder;
            game_button_state RightShoulder;
        };
    };
};

struct game_input
{
    // TODO(george): Insert clock values here
    game_controller_input Controllers[4];
};

struct game_memory 
{
    bool32 IsInitialized;

    uint64 PermanentStorageSize;
    void *PermanentStorage;
    uint64 TransientStorageSize;
    void *TransientStorage;
};

internal void
GameUpdateAndRender(game_input *Input, game_offscreen_buffer *Buffer, game_sound_output_buffer *SoundBuffer);

//
//
//

struct game_state
{
    int BlueOffset;
    int GreenOffset;
    int ToneHz;
};

#define HANDMADE_H
#endif