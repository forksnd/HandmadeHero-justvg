#if !defined(HANDMADE_WORLD_CPP)
#define HANDMADE_WORLD_CPP

// TODO(george): Think about what ther real safe margin is
#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX/64)
#define TILE_CHUNK_UNINITIALIZED INT32_MAX

#define TILES_PER_CHUNK 16

inline world_position
NullPosition()
{
    world_position Result = {};

    Result.ChunkX = TILE_CHUNK_UNINITIALIZED;

    return(Result);
}

inline bool32
IsValid(world_position *P)
{
    bool32 Result = (P->ChunkX != TILE_CHUNK_UNINITIALIZED);
    return(Result);
}

inline bool32
IsCanonical(world *World, real32 TileRel)
{
    // TODO(george): Fix floating point math so this can be exact?
    bool32 Result = (TileRel >= -0.5f*World->ChunkSideInMeters) && (TileRel <= 0.5f*World->ChunkSideInMeters);

    return(Result);
}

inline bool32
IsCanonical(world *World, v2 Offset)
{
    bool32 Result = (IsCanonical(World, Offset.X) && IsCanonical(World, Offset.Y));

    return(Result);
}

inline world_chunk * 
GetWorldChunk(world *World, int32 ChunkX, int32 ChunkY, int32 ChunkZ,
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

        if(Arena && (TileChunk->ChunkX == TILE_CHUNK_UNINITIALIZED))
        {
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

internal void
InitializeWorld(world *World, real32 TileSideInMeters)
{
    World->TileSideInMeters = TileSideInMeters;
    World->ChunkSideInMeters = (real32)TILES_PER_CHUNK*TileSideInMeters;
    World->FirstFree = 0;

    for(uint32 ChunkIndex = 0; ChunkIndex < ArrayCount(World->ChunkHash); ChunkIndex++)
    {
        World->ChunkHash[ChunkIndex].ChunkX = TILE_CHUNK_UNINITIALIZED;
        World->ChunkHash[ChunkIndex].FirstBlock.EntityCount = 0;
    }
}

inline void
RecanonicalizeCoord(world *World, int32 *Tile, real32 *TileRel)
{
    // TODO(george): Need t something that doesn't use the div/mul method
    // for recanonicalizing because this can end up rounding back on to the tile
    // you just came from.

    // NOTE(george): Wrapping IS NOT ALLOWED, so all coordinates are assumed to be
    // within the safe margin!
    // TODO(george): Assert that we are nowhere near the edges of the world.
    
    int32 Offset = RoundReal32ToInt32(*TileRel / World->ChunkSideInMeters); 
    *Tile += Offset;
    *TileRel -= Offset*World->ChunkSideInMeters;

    Assert(IsCanonical(World, *TileRel));
}

inline world_position
MapIntoChunkSpace(world *World, world_position BasePos, v2 Offset)
{
    world_position Result = BasePos;
    
    Result.Offset_ += Offset;
    RecanonicalizeCoord(World, &Result.ChunkX, &Result.Offset_.X);
    RecanonicalizeCoord(World, &Result.ChunkY, &Result.Offset_.Y);
    
    return(Result);
}

inline world_position
ChunkPositionFromTilePosition(world *World, int32 AbsTileX, int32 AbsTileY, int32 AbsTileZ)
{
    world_position Result = {};

    Result.ChunkX = AbsTileX / TILES_PER_CHUNK;
    Result.ChunkY = AbsTileY / TILES_PER_CHUNK;
    Result.ChunkZ = AbsTileZ / TILES_PER_CHUNK;

    // TODO(george): Think this through and actually work out the math.
    if(AbsTileX < 0)
    {
        --Result.ChunkX;
    }
    if(AbsTileY < 0)
    {
        --Result.ChunkY;
    }
    if(AbsTileZ < 0)
    {
        --Result.ChunkZ;
    }

    // TODO(george): DECIDE ON TILE ALIGNMENT IN CHUNKS!
    Result.Offset_.X = ((AbsTileX - TILES_PER_CHUNK/2) - (Result.ChunkX*TILES_PER_CHUNK))*World->TileSideInMeters;
    Result.Offset_.Y = ((AbsTileY - TILES_PER_CHUNK/2) - (Result.ChunkY*TILES_PER_CHUNK))*World->TileSideInMeters;
    // TODO(george): Move to 3D Z!!!

    Assert(IsCanonical(World, Result.Offset_));

    return(Result);
} 

inline bool32
AreInTheSameChunk(world *World, world_position *A, world_position *B)
{
    Assert(IsCanonical(World, A->Offset_));
    Assert(IsCanonical(World, B->Offset_));
    bool32 Result = ((A->ChunkX == B->ChunkX) &&
                     (A->ChunkY == B->ChunkY) &&
                     (A->ChunkZ == B->ChunkZ));

    return(Result);
}

inline world_difference 
Substract(world *World, world_position *A, world_position *B)
{
    world_difference Result;

    v2 dTileXY = {(real32)A->ChunkX - (real32)B->ChunkX,
                  (real32)A->ChunkY - (real32)B->ChunkY};
    real32 dTileZ = (real32)A->ChunkZ - (real32)B->ChunkZ;

    Result.dXY = World->ChunkSideInMeters*dTileXY + (A->Offset_ - B->Offset_);

    // TODO(george): Think about what we want t about Z
    Result.dZ = World->ChunkSideInMeters*dTileZ;

    return(Result);
}

inline world_position
CenteredChunkPoint(uint32 ChunkX, uint32 ChunkY, uint32 ChunkZ)
{
    world_position Result = {};

    Result.ChunkX = ChunkX;
    Result.ChunkY = ChunkY;
    Result.ChunkZ = ChunkZ;    

    return(Result);
}

internal void
ChangeEntityLocationRaw(memory_arena *Arena, world *World, uint32 LowEntityIndex, 
                     world_position *OldP, world_position *NewP)
{
    // TODO(george): If this moves an entity into the camera bounds, should it automatically
    // go into the high set immediately?

    Assert(!OldP || IsValid(OldP));
    Assert(!NewP || IsValid(NewP));

    if(OldP && AreInTheSameChunk(World, OldP, NewP))
    {
        // NOTE(george): Leave entity where it is
    }
    else
    {
        if(OldP)
        {
            // NOTE(george): Pull the entity out of its current entity block
            world_chunk *Chunk = GetWorldChunk(World, OldP->ChunkX, OldP->ChunkY, OldP->ChunkZ);
            Assert(Chunk);
            if(Chunk)
            {
                bool32 NotFound = true;
                world_entity_block *FirstBlock = &Chunk->FirstBlock;
                for(world_entity_block *Block = FirstBlock; Block && NotFound; Block = Block->Next)
                {
                    for(uint32 Index = 0; (Index < Block->EntityCount) && NotFound; Index++)
                    {
                        if(Block->LowEntityIndex[Index] == LowEntityIndex)
                        {
                            Assert(FirstBlock->EntityCount > 0);
                            Block->LowEntityIndex[Index] = FirstBlock->LowEntityIndex[--FirstBlock->EntityCount];
                            if(FirstBlock->EntityCount == 0)
                            {
                                if(FirstBlock->Next)
                                {
                                    world_entity_block *NextBlock = FirstBlock->Next;
                                    *FirstBlock = *NextBlock;
                                    
                                    NextBlock->Next = World->FirstFree;
                                    World->FirstFree = NextBlock;
                                }
                            }

                            NotFound = false;
                        }
                    }
                }
            }
        }
        
        if(NewP)
        {
            // NOTE(george): Doesn't work if we insert second+ world_entity_block in the chain??

            // NOTE(george): Insert the entity into its new entity block
            world_chunk *Chunk = GetWorldChunk(World, NewP->ChunkX, NewP->ChunkY, NewP->ChunkZ, Arena);
            Assert(Chunk);

            world_entity_block *Block = &Chunk->FirstBlock;
            if(Block->EntityCount == ArrayCount(Block->LowEntityIndex))
            {
                // NOTE(george): We're out of room, get a new block!
                world_entity_block *OldBlock = World->FirstFree;    
                if(OldBlock)
                {
                    World->FirstFree = OldBlock->Next;
                }
                else
                {
                    OldBlock = PushStruct(Arena, world_entity_block);
                }
                
                *OldBlock = *Block;
                Block->Next = OldBlock;
                Block->EntityCount = 0;
            }

            Assert(Block->EntityCount < ArrayCount(Block->LowEntityIndex));
            Block->LowEntityIndex[Block->EntityCount++] = LowEntityIndex;
        }
    }
}

internal void
ChangeEntityLocation(memory_arena *Arena, world *World,
                        uint32 LowEntityIndex, low_entity *LowEntity,  
                        world_position *OldP, world_position *NewP)
{
    ChangeEntityLocationRaw(Arena, World, LowEntityIndex, OldP, NewP);
    if(NewP)
    {
        LowEntity->P = *NewP;
    }
    else
    {
        LowEntity->P = NullPosition();
    }
}

#endif