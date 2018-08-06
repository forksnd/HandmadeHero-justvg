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
struct tile_chunk_position
{
    uint32 TileChunkX;
    uint32 TileChunkY;

    uint32 RelTileX;
    uint32 RelTileY;
};

struct world_position
{   
    /* TODO(george):

        Take the tile map x and y
        and the tile x and y

        and pack them into single 32-bit values for x and y
        where there is some low bits for the tile index
        and the high bits are the tile "page"
    */

    uint32 AbsTileX;
    uint32 AbsTileY;

    // TODO(george): Should these be from the center of a tile?
    // TODO(george): Rename to offset X and Y
    real32 TileRelX;
    real32 TileRelY;
};

struct game_state
{
    world_position PlayerP;
};

struct tile_chunk
{
    uint32 *Tiles;
};

struct world
{
    uint32 ChunkShift;
    uint32 ChunkMask;
    uint32 ChunkDim;

    real32 TileSideInMeters;
    int32 TileSideInPixels;

    // TODO(george): Beginner's sparseness
    int32 TileChunkCountX;
    int32 TileChunkCountY;

    real32 MetersToPixels;

    tile_chunk *TileChunks;
};

#endif


