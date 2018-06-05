#if !defined(HANDMADE_H)

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

internal void
GameUpdateAndRender(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset);

#define HANDMADE_H
#endif