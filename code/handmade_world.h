#if !defined(HANDMADE_WORLD_H)
#define HANDMADE_WORLD_H

// TODO(george): Replace this with a v3 once we get to v3
struct world_difference
{
    v2 dXY;
    real32 dZ;
};

struct world_position
{   
	// TODO(george): Puzzler! How can we get rid of abstile here,
	// and still allow references to entities to be able to figure
	// out _where they are_? (or rather, which world_chunk they are
	// in?)

    // NOTE(george): These are fixed point locations.
    // The high bits are the tile chunk index, and the low bits are the tile index 
    // in the chunk.
    int32 AbsTileX;
    int32 AbsTileY;
    int32 AbsTileZ;

    // NOTE(george): These are the offsets from tile center
    v2 Offset_;
};

// TODO(george): Could make this just tile_chunk and then allow multiple tile chunks per X/Y/Z
struct world_entity_block
{
    uint32 EntityCount;
    uint32 LowEntityIndex[16];
    world_entity_block *Next;
};

struct world_chunk
{
    int32 ChunkX;
	int32 ChunkY;
	int32 ChunkZ;

    world_entity_block FirstBlock;

    world_chunk *NextInHash;
};

struct world
{
    real32 TileSideInMeters;

    // TODO(george): TileChunkHash should probably switch to pointers IF
    // tile entity blocks continue to be stored on masse directily in the tile chunk.
    // NOTE(george): At the moment, this must be a power of two!
    int32 ChunkShift;
    int32 ChunkMask;
    int32 ChunkDim;
	world_chunk ChunkHash[4096];
};

#endif