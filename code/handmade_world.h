#if !defined(HANDMADE_WORLD_H)
#define HANDMADE_WORLD_H

introspect(category:"world") struct world_position
{   
	// TODO(george): It seems like we have to store ChunkX/Y/Z with each
    // entity because even though the sim region gather doesn't need it
    // at first, and we could get by without it, entity references pull
    // in entities WITHOUT going through their world_chunk, and thus
    // still need to know the ChunkX/Y/Z

    // NOTE(george): These are fixed point locations.
    int32 ChunkX;
    int32 ChunkY;
    int32 ChunkZ;

    // NOTE(george): These are the offsets from chunk center
    v3 Offset_;
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

    // TODO(george): Profile this and determine if a pointer would be better here!
    world_entity_block FirstBlock;

    world_chunk *NextInHash;
};

struct world
{
    v3 ChunkDimInMeters;

    world_entity_block *FirstFree;

    // TODO(george): ChunkHash should probably switch to pointers IF
    // tile entity blocks continue to be stored on masse directily in the tile chunk.
    // NOTE(george): At the moment, this must be a power of two!
	world_chunk ChunkHash[4096];
};

#endif