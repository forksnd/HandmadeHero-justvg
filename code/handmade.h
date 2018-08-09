#if !defined(HANDMADE_H)
#define HANDMADE_H

/*
    NOTE(george):

    HANDMADE_INTERNAL:
        0 - Build for public release
        1 - Build for developer only
        
    HANDMADE_SLOW:
        0 - Not slow code allowed!
        1 - Slow code welcome
*/

#include "handmade_platform.h"

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.14159265359f

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

inline uint32
SafeTruncateUInt64(uint64 Value)
{
    Assert(Value <= 0xFFFFFFFF);
    uint32 Result = (uint32) Value;
    return (Result);
}

/*
	NOTE(george): Services that the game provides to the platform layer
	(this may expand in the future - sound on separate thread, etc.)
*/

inline game_controller_input *GetController(game_input *Input, unsigned int ControllerIndex)
{
    Assert(ControllerIndex < ArrayCount(Input->Controllers));

    game_controller_input *Result = &Input->Controllers[ControllerIndex];
    return(Result);
}

//
//
//

struct memory_arena
{
    memory_index Size;
    uint8 *Base;
    memory_index Used;
};

internal void
InitializeArena(memory_arena *Arena, memory_index Size, uint8 *Base)
{
    Arena->Size = Size;
    Arena->Base = Base;
    Arena->Used = 0;
}

#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count)*sizeof(type))
internal void *
PushSize_(memory_arena *Arena, memory_index Size)
{
    Assert((Arena->Used + Size) <= Arena->Size);
    void *Result = Arena->Base + Arena->Used;
    Arena->Used += Size;

    return(Result);
}

#include "handmade_intrinsics.h"
#include "handmade_tile.h"
#include "handmade_random.h"

struct world
{
    tile_map *TileMap;
};

struct game_state
{
    memory_arena WorldArena;
    world *World;

    tile_map_position PlayerP;
};

#endif


