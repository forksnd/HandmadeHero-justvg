#if !defined(HANDMADE_WORLD_CPP)
#define HANDMADE_WORLD_CPP

// TODO(george): Think about what ther real safe margin is
#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX/64)
#define TILE_CHUNK_UNINITIALIZED INT32_MAX

inline world_chunk * 
GetTileChunk(world *World, int32 ChunkX, int32 ChunkY, int32 ChunkZ,
             memory_arena *Arena = 0)
{
    Assert(ChunkX > -TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkY > -TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkZ > -TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkX < TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkY < TILE_CHUNK_SAFE_MARGIN);
    Assert(ChunkZ < TILE_CHUNK_SAFE_MARGIN);

    // TODO(george): Better hash function!
    uint32 HashValue = 19*ChunkX + 7*ChunkY + 3*ChunkZ;
    uint32 HashSlot = HashValue & (ArrayCount(World->ChunkHash) - 1); 
    Assert(HashSlot < ArrayCount(World->ChunkHash));

    world_chunk *TileChunk = World->ChunkHash + HashSlot;
    do
    {
        if((ChunkX == TileChunk->ChunkX) &&
           (ChunkY == TileChunk->ChunkY) && 
           (ChunkZ == TileChunk->ChunkZ))
        {
            break;
        }

        if(Arena && (TileChunk->ChunkX != TILE_CHUNK_UNINITIALIZED) && (!TileChunk->NextInHash))
        {
            TileChunk->NextInHash = PushStruct(Arena, world_chunk);
            TileChunk = TileChunk->NextInHash;
            TileChunk->ChunkX = TILE_CHUNK_UNINITIALIZED;
        }

        if(Arena && (ChunkX == TILE_CHUNK_UNINITIALIZED))
        {
            uint32 TileCount = World->ChunkDim*World->ChunkDim;

            TileChunk->ChunkX = ChunkX;
            TileChunk->ChunkY = ChunkY;
            TileChunk->ChunkZ = ChunkZ;

            TileChunk->NextInHash = 0;

            break;
        }
        
        TileChunk = TileChunk->NextInHash;
    } while(TileChunk);
    
    return(TileChunk);
}

#if 0
inline world_position 
GetChunkPositionFor(world *World, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    world_position Result;

    Result.AbsTileX = AbsTileX >> World->ChunkShift;
    Result.AbsTileY = AbsTileY >> World->ChunkShift;
    Result.AbsTileZ = AbsTileZ;
    Result.RelTileX = AbsTileX & World->ChunkMask;
    Result.RelTileY = AbsTileY & World->ChunkMask;

    return(Result);
}
#endif

internal void
InitializeWorld(world *World, real32 TileSideInMeters)
{
    World->ChunkShift = 4;
    World->ChunkMask = (1 << World->ChunkShift) - 1;
    World->ChunkDim = (1 << World->ChunkShift);
    World->TileSideInMeters = TileSideInMeters;

    for(uint32 TileChunkIndex = 0; TileChunkIndex < ArrayCount(World->ChunkHash); TileChunkIndex++)
    {
        World->ChunkHash[TileChunkIndex].ChunkX = INT32_MAX;
    }
}

//
// TODO(george): Do these really belong in more of a "positioning" or "geometry" file?
// 

inline void
RecanonicalizeCoord(world *World, int32 *Tile, real32 *TileOffset)
{
    // TODO(george): Need t something that doesn't use the div/mul method
    // for recanonicalizing because this can end up rounding back on to the tile
    // you just came from.

    // NOTE(george): World is assumed to be toroidal topology, if you step off one end you
    // come back on the other! 
    int32 Offset = RoundReal32ToInt32(*TileOffset / World->TileSideInMeters); 
    *Tile += Offset;
    *TileOffset -= Offset*World->TileSideInMeters;

    // TODO(george): Fix floating point math so this can be exact?
    Assert(*TileOffset > -0.5f*World->TileSideInMeters);
    Assert(*TileOffset < 0.5f*World->TileSideInMeters);
}

internal world_position
MapIntoTileSpace(world *World, world_position BasePos, v2 Offset)
{
    world_position Result = BasePos;
    
    Result.Offset_ += Offset;
    RecanonicalizeCoord(World, &Result.AbsTileX, &Result.Offset_.X);
    RecanonicalizeCoord(World, &Result.AbsTileY, &Result.Offset_.Y);
    
    return(Result);
}

inline bool32
AreOnTheSameTile(world_position *A, world_position *B)
{
    bool32 Result = ((A->AbsTileX == B->AbsTileX) &&
                     (A->AbsTileY == B->AbsTileY) &&
                     (A->AbsTileZ == B->AbsTileZ));

    return(Result);
}

inline world_difference 
Substract(world *World, world_position *A, world_position *B)
{
    world_difference Result;

    v2 dTileXY = {(real32)A->AbsTileX - (real32)B->AbsTileX,
                  (real32)A->AbsTileY - (real32)B->AbsTileY};
    real32 dTileZ = (real32)A->AbsTileZ - (real32)B->AbsTileZ;

    Result.dXY = World->TileSideInMeters*dTileXY + (A->Offset_ - B->Offset_);

    // TODO(george): Think about what we want t about Z
    Result.dZ = World->TileSideInMeters*dTileZ;

    return(Result);
}

inline world_position
CenteredTilePoint(uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    world_position Result = {};

    Result.AbsTileX = AbsTileX;
    Result.AbsTileY = AbsTileY;
    Result.AbsTileZ = AbsTileZ;    

    return(Result);
}

#endif