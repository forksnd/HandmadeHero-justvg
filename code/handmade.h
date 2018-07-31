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

struct game_state
{
    real32 PlayerX;
    real32 PlayerY;
};

struct tile_map
{
    int32 CountX;
    int32 CountY;
        
    real32 UpperLeftX;
    real32 UpperLeftY;  
    real32 TileWidth;
    real32 TileHeight;

    uint32 *Tiles;
};

struct world
{
    // TODO(george): Beginner's sparseness
    int32 TileMapCountX;
    int32 TileMapCountY;

    tile_map *TileMaps;
};

#endif