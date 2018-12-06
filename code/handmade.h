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

/*
    TODO(george):
    
    ARCHITECTURE EXPLORATION
    - Collision detection?
        - Transient collision rules! Clear based on flag.
            - Allow non-transient rules to override transient ones.
            - Entry/exit?
        - What's the plan for robustness / shape definition? 
    - Z!
        - Need to make a solid concept of ground levels so the camera can
          be freely placed in Z and have multiple ground levels in one
          sim region
        - How is this rendered?
    - Implement multiple sim regions per frame
        - Per-entity clocking
        - Sim region merging? For multiple players?
    
    
    - Debug code
        - Logging
        - Diagramming
        - Switches / sliders / etc.
    
    - Audio
        - Sound effect triggers
        - Ambient sounds
        - Music
    - Asset streaming

    - Metagame / save game?
        - How do you enter "save slot"?
        - Persistent unlocks/etc.
        - Do we allow saved games? Probably yes, just only for "pausing",
        * Continuous svae for crash recovery?
    - Rudimentary world gen
        - Placement of background things
        - Connectivity?
        - Non-overlapping?
        - Map display
    - AI
        - Rudimentary monstar behavior example
        * Pathfinding
        - AI "storage"

    * Animation should probably lead into rendering
        - Skeletal animation
        - Particle systems

    PRODUCTION
    - Rendering
    -> GAME
        - Entity system
        - World generation
*/

#include "handmade_platform.h"

#define Minimum(A, B) ((A < B) ? (A) : (B))
#define Maximum(A, B) ((A > B) ? (A) : (B))

//
//
//

struct memory_arena
{
    memory_index Size;
    uint8 *Base;
    memory_index Used;

    int32 TempCount;
};

struct temporary_memory
{
    memory_arena *Arena;
    memory_index Used;  
};

inline void
InitializeArena(memory_arena *Arena, memory_index Size, void *Base)
{
    Arena->Size = Size;
    Arena->Base = (uint8 *)Base;
    Arena->Used = 0;
    Arena->TempCount = 0;
}

#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count)*sizeof(type))
#define PushSize(Arena, Size) PushSize_(Arena, Size)
inline void *
PushSize_(memory_arena *Arena, memory_index Size)
{
    Assert((Arena->Used + Size) <= Arena->Size);
    void *Result = Arena->Base + Arena->Used;
    Arena->Used += Size;

    return(Result);
}

inline temporary_memory 
BeginTemporaryMemory(memory_arena *Arena)
{
    temporary_memory Result;

    Result.Arena = Arena;
    Result.Used = Arena->Used;

    Arena->TempCount++;

    return(Result);
}

inline void 
EndTemporaryMemory(temporary_memory TempMem)
{
    memory_arena *Arena = TempMem.Arena;
    Assert(Arena->Used >= TempMem.Used);
    Arena->Used = TempMem.Used;
    Assert(Arena->TempCount > 0);
    Arena->TempCount--;
}

inline void
CheckArena(memory_arena *Arena)
{
    Assert(Arena->TempCount == 0);
}

#define ZeroStruct(Instance) ZeroSize(sizeof(Instance), &(Instance))
inline void
ZeroSize(memory_index Size, void* Ptr)
{
    // TODO(george): Check this guy for performance
    uint8 *Byte = (uint8 *)Ptr;
    while(Size--)
    {
        *Byte++ = 0;
    }
}

#include "handmade_intrinsics.h"
#include "handmade_math.h"
#include "handmade_world.h"
#include "handmade_sim_region.h"
#include "handmade_entity.h"

struct loaded_bitmap
{
    int32 Width;
    int32 Height;
    int32 Pitch;
    void *Memory;
};

struct hero_bitmaps
{
    v2 Align;
    loaded_bitmap Hero;
};

struct low_entity
{
    // TODO(george): It is kind of busted that P's can be invalid here,
    // AND we store whether they would be invalid in the flags field...
    // Can we do something better here?
    world_position P;    
    sim_entity Sim;
};

struct controlled_hero
{
    uint32 EntityIndex;

    // NOTE(george): These are the controller requests for simulation
    v2 ddP;
    v2 dSword;
    real32 dZ;
};

struct pairwise_collision_rule
{
    bool32 CanCollide;
    uint32 StorageIndexA;
    uint32 StorageIndexB;

    pairwise_collision_rule *NextInHash;
};
struct game_state;
internal void AddCollisionRule(game_state *GameState, uint32 StorageIndexA, uint32 StorageIndexB, bool32 CanCollide);
internal void ClearCollisionRulesFor(game_state *GameState, uint32 StorageIndex);

struct ground_buffer
{
    // NOTE(george): An invalid P tells us that this ground has not been filled
    world_position P; // NOTE(george): This is the center of the bitmap
    loaded_bitmap Bitmap;
};

struct game_state
{
    memory_arena WorldArena;
    world *World;

    real32 TypicalFloorHeight;

    uint32 CameraFollowingEntityIndex;
    world_position CameraP;

    controlled_hero ControlledHeroes[ArrayCount(((game_input *)0)->Controllers)];

    // TODO(george): Change the name to "Stored entity"
    uint32 LowEntityCount;
    low_entity LowEntities[100000];    

    loaded_bitmap Grass[1];
    loaded_bitmap Stone[1];

    loaded_bitmap Backdrop;
    loaded_bitmap Shadow;
    hero_bitmaps HeroBitmaps[4];

    loaded_bitmap Tree;
    loaded_bitmap Sword;
    loaded_bitmap Stairwell;
    real32 MetersToPixels;
    real32 PixelsToMeters;

    // TODO(george): Must be power of two
    pairwise_collision_rule *CollisionRuleHash[256];
    pairwise_collision_rule *FirstFreeCollisionRule;

    sim_entity_collision_volume_group *NullCollision;
    sim_entity_collision_volume_group *SwordCollision;
    sim_entity_collision_volume_group *StairCollision;
    sim_entity_collision_volume_group *PlayerCollision;
    sim_entity_collision_volume_group *MonstarCollision;
    sim_entity_collision_volume_group *FamiliarCollision;
    sim_entity_collision_volume_group *WallCollision;
    sim_entity_collision_volume_group *StandardRoomCollision;

    real32 Time;
};

struct transient_state
{
    bool32 IsInitialized;
    memory_arena TranArena;
    uint32 GroundBufferCount;
    ground_buffer *GroundBuffers;
};

// TODO(george): This is dumb, this should just be a part of 
// the renderer pushbuffer - add correction of coordinates
// in there and be done with it.

inline low_entity *
GetLowEntity(game_state *GameState, uint32 Index)
{
    low_entity *Result = 0;

    if((Index > 0) && (Index < GameState->LowEntityCount))
    {
        Result = GameState->LowEntities + Index;
    }

    return(Result);
}

#endif

