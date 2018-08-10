#if !defined(HANDMADE_TILE)
#define HANDMADE_TILE

inline tile_chunk * 
GetTileChunk(tile_map *TileMap, uint32 TileChunkX, uint32 TileChunkY, uint32 TileChunkZ)
{
    tile_chunk *TileChunk = 0;

    if((TileChunkX >= 0) && (TileChunkX < TileMap->TileChunkCountX) &&
       (TileChunkY >= 0) && (TileChunkY < TileMap->TileChunkCountY) &&
       (TileChunkZ >= 0) && (TileChunkZ < TileMap->TileChunkCountZ))
    {
        TileChunk = &TileMap->TileChunks[TileChunkZ*TileMap->TileChunkCountY*TileMap->TileChunkCountX
                                         + TileChunkY*TileMap->TileChunkCountX
                                         + TileChunkX];
    }

    return(TileChunk);
}

inline bool32
GetTileValueUnchecked(tile_map *TileMap, tile_chunk *TileChunk, uint32 TileX, uint32 TileY)
{
    Assert(TileChunk);
    Assert(TileX < TileMap->ChunkDim);
    Assert(TileY < TileMap->ChunkDim);

    uint32 TileChunkValue = TileChunk->Tiles[TileY*TileMap->ChunkDim + TileX];
    return(TileChunkValue);
}

inline void
SetTileValueUnchecked(tile_map *TileMap, tile_chunk *TileChunk, uint32 TileX, uint32 TileY, uint32 TileValue)
{
    Assert(TileChunk);
    Assert(TileX < TileMap->ChunkDim);
    Assert(TileY < TileMap->ChunkDim);

    TileChunk->Tiles[TileY*TileMap->ChunkDim + TileX] = TileValue;
}

inline bool32
GetTileValue(tile_map *TileMap, tile_chunk *TileChunk, uint32 TestTileX, uint32 TestTileY)
{
    uint32 TileChunkValue = 0;

    if(TileChunk && TileChunk->Tiles)
    {
        TileChunkValue = GetTileValueUnchecked(TileMap, TileChunk, TestTileX, TestTileY);
    }

    return(TileChunkValue);
}

inline void
SetTileValue(tile_map *TileMap, tile_chunk *TileChunk, uint32 TestTileX, uint32 TestTileY, uint32 TileValue)
{
    if(TileChunk && TileChunk->Tiles)
    {
        SetTileValueUnchecked(TileMap, TileChunk, TestTileX, TestTileY, TileValue);
    }
}

inline tile_chunk_position 
GetChunkPositionFor(tile_map *TileMap, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    tile_chunk_position Result;

    Result.TileChunkX = AbsTileX >> TileMap->ChunkShift;
    Result.TileChunkY = AbsTileY >> TileMap->ChunkShift;
    Result.TileChunkZ = AbsTileZ;
    Result.RelTileX = AbsTileX & TileMap->ChunkMask;
    Result.RelTileY = AbsTileY & TileMap->ChunkMask;

    return(Result);
}

internal uint32
GetTileValue(tile_map *TileMap, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    tile_chunk_position ChunkPos = GetChunkPositionFor(TileMap, AbsTileX, AbsTileY, AbsTileZ);
    tile_chunk *TileChunk = GetTileChunk(TileMap, ChunkPos.TileChunkX, ChunkPos.TileChunkY, ChunkPos.TileChunkZ);
    uint32 TileChunkValue = GetTileValue(TileMap, TileChunk, ChunkPos.RelTileX, ChunkPos.RelTileY);

    return(TileChunkValue);
}

internal uint32
GetTileValue(tile_map *TileMap, tile_map_position Pos)
{
    uint32 TileChunkValue = GetTileValue(TileMap, Pos.AbsTileX, Pos.AbsTileY, Pos.AbsTileZ);

    return(TileChunkValue);
}

internal bool32
IsTileMapPointEmpty(tile_map *TileMap, tile_map_position Pos)
{
    uint32 TileChunkValue = GetTileValue(TileMap, Pos);
    bool32 Empty = ((TileChunkValue == 1) || 
                     (TileChunkValue == 3) ||
                     (TileChunkValue == 4));

    return(Empty);
}

internal void
SetTileValue(memory_arena *Arena, tile_map *TileMap, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ, uint32 TileValue)
{
    tile_chunk_position ChunkPos = GetChunkPositionFor(TileMap, AbsTileX, AbsTileY, AbsTileZ);
    tile_chunk *TileChunk = GetTileChunk(TileMap, ChunkPos.TileChunkX, ChunkPos.TileChunkY, ChunkPos.TileChunkZ);

    Assert(TileChunk);
    if(!TileChunk->Tiles)
    {
        uint32 TileCount = TileMap->ChunkDim*TileMap->ChunkDim;
        TileChunk->Tiles = PushArray(Arena, TileCount, uint32);
        for(uint32 TileIndex = 0; TileIndex < TileCount; TileIndex++)
        {
            TileChunk->Tiles[TileIndex] = 1;
        }
    }

    SetTileValue(TileMap, TileChunk, ChunkPos.RelTileX, ChunkPos.RelTileY, TileValue);
}

//
// TODO(george): Do these really belong in more of a "positioning" or "geometry" file?
// 

inline void
RecanonicalizeCoord(tile_map *TileMap, uint32 *Tile, real32 *TileOffset)
{
    // TODO(george): Need to do something that doesn't use the div/mul method
    // for recanonicalizing because this can end up rounding back on to the tile
    // you just came from.

    // NOTE(george): TileMap is assumed to be toroidal topology, if you step off one end you
    // come back on the other! 
    int32 Offset = RoundReal32ToInt32(*TileOffset / TileMap->TileSideInMeters); 
    *Tile += Offset;
    *TileOffset -= Offset*TileMap->TileSideInMeters;

    // TODO(george): Fix floating point math so this can be < ?
    Assert(*TileOffset >= -0.5f*TileMap->TileSideInMeters);
    Assert(*TileOffset <= 0.5f*TileMap->TileSideInMeters);
}

internal tile_map_position
RecanonicalizePosition(tile_map *TileMap, tile_map_position Pos)
{
    tile_map_position Result = Pos;
    
    RecanonicalizeCoord(TileMap, &Result.AbsTileX, &Result.OffsetX);
    RecanonicalizeCoord(TileMap, &Result.AbsTileY, &Result.OffsetY);
    
    return(Result);
}

inline bool32
AreOnTheSameTile(tile_map_position *A, tile_map_position *B)
{
    bool32 Result = ((A->AbsTileX == B->AbsTileX) &&
                     (A->AbsTileY == B->AbsTileY) &&
                     (A->AbsTileZ == B->AbsTileZ));

    return(Result);
}

#endif