#if !defined(HANDMADE_SIM_REGION_H)
#define HANDMADE_SIM_REGION_H

struct move_spec
{
    bool32 UnitMaxAccelVector;
    real32 Speed;
    real32 Drag;
};

enum entity_type
{
    EntityType_Null,

    EntityType_Hero,
    EntityType_Wall,
    EntityType_Familiar,
    EntityType_Monstar,
    EntityType_Sword, 
};

#define HIT_POINT_SUB_COUNT 4
struct hit_point
{
    uint8 Flags;
    uint8 FilledAmount;
};

// TODO(george): Rename sim_entity to entity
struct sim_entity;
union entity_reference
{
	sim_entity *Ptr;
	uint32 Index;	
};

enum sim_entity_flags
{
    EntityFlag_Collides = (1 << 0), 
    EntityFlag_Nonspatial = (1 << 1),

    EntityFlag_Simming = (1 << 30), 
};

struct sim_entity
{
    // NOTE(george): These are only for the sim region
    world_chunk *OldChunk;
	uint32 StorageIndex;
    bool32 Updatable;

    // 

	entity_type Type;
    uint32 Flags;    

	v3 P; 
    v3 dP;

    real32 DistanceLimit;

    v3 Dim;

    uint32 FacingDirection;
    real32 tBob;

    int32 dAbsTileZ;

    // TODO(george): Should hitpoints themselves be entities?
    uint32 HitPointMax;
    hit_point HitPoint[16];

    entity_reference Sword;
};

struct sim_entity_hash
{
    sim_entity *Ptr;
    uint32 Index;
};

struct sim_region
{
	// TODO(george): Need a hash table here to map stored entity indices
	// to sim entities

	world *World;
    real32 MaxEntityRadius;
    real32 MaxEntityVelocity;

	world_position Origin;
	rectangle3 Bounds;
    rectangle3 UpdatableBounds;

	uint32 MaxEntityCount;
	uint32 EntityCount;
    sim_entity *Entities;

    // NOTE(george): Must be a power of two!
    sim_entity_hash Hash[4096];
};

#endif