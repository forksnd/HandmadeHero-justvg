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
#include "handmade_intrinsics.h"
#include "handmade_math.h"
#include "handmade_file_formats.h"
#include "handmade_meta.h"

#define DLIST_INSERT(Sentinel, Element) \
(Element)->Next = (Sentinel)->Next; \
(Element)->Prev = (Sentinel); \
(Element)->Next->Prev = (Element); \
(Element)->Prev->Next = (Element); 

#define DLIST_INIT(Sentinel) (Sentinel)->Next = (Sentinel)->Prev = (Sentinel);

#define FREELIST_ALLOCATE(Result, FreeListPointer, AllocationCode) \
    (Result) = (FreeListPointer); \
    if(Result) {(FreeListPointer) = (FreeListPointer)->NextFree;} \
    else { (Result) = AllocationCode; }
#define FREELIST_DEALLOCATE(Pointer, FreeListPointer) \
    if(Pointer) {(Pointer)->NextFree = (FreeListPointer); (FreeListPointer) = (Pointer);} 

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

#define Minimum(A, B) ((A < B) ? (A) : (B))
#define Maximum(A, B) ((A > B) ? (A) : (B))

inline bool32 
StringsAreEqual(char *A, char *B)
{
    bool32 Result = (A == B);
    
    if(A && B)
    {
        while(*A && *B && (*A == *B))
        {
            A++;
            B++;
        }

        Result = ((*A == 0) && (*B == 0));
    }

    return(Result);
}

//
//
//

inline void
InitializeArena(memory_arena *Arena, memory_index Size, void *Base)
{
    Arena->Size = Size;
    Arena->Base = (uint8 *)Base;
    Arena->Used = 0;
    Arena->TempCount = 0;
}

inline memory_index
GetAlignmentOffset(memory_arena *Arena, memory_index Alignment)
{
    memory_index ResultPointer = (memory_index)Arena->Base + Arena->Used;
    memory_index AlignmentOffset = 0;

    memory_index AlignmentMask = Alignment - 1;
    if(ResultPointer & AlignmentMask)
    {
        AlignmentOffset = Alignment - (ResultPointer & AlignmentMask);
    }

    return(AlignmentOffset);
}

inline memory_index
GetArenaSizeRemaining(memory_arena *Arena, memory_index Alignment = 4)
{
    memory_index Result = Arena->Size - (Arena->Used + GetAlignmentOffset(Arena, Alignment));

    return(Result);
}

#define DEFAULT_MEMORY_ALIGNMENT 4
#define PushStruct(Arena, type, ...) (type *)PushSize_(Arena, sizeof(type), ## __VA_ARGS__)
#define PushArray(Arena, Count, type, ...) (type *)PushSize_(Arena, (Count)*sizeof(type), ## __VA_ARGS__)
#define PushSize(Arena, Size, ...) PushSize_(Arena, Size, ## __VA_ARGS__)
#define PushCopy(Arena, Size, Source, ...) Copy(Size, Source, PushSize_(Arena, Size, ## __VA_ARGS__))
inline memory_index
GetEffectiveSizeFor(memory_arena *Arena, memory_index SizeInit, memory_index Alignment)
{
    memory_index Size = SizeInit;
    
    memory_index AlignmentOffset = GetAlignmentOffset(Arena, Alignment);
    Size += AlignmentOffset;

    return(Size);
}

inline b32
ArenaHasRoomFor(memory_arena *Arena, memory_index SizeInit, memory_index Alignment = DEFAULT_MEMORY_ALIGNMENT)
{
    memory_index Size = GetEffectiveSizeFor(Arena, SizeInit, Alignment);
    b32 Result = (Arena->Used + Size) <= Arena->Size;
    return(Result);
}

inline void *
PushSize_(memory_arena *Arena, memory_index SizeInit, memory_index Alignment = DEFAULT_MEMORY_ALIGNMENT)
{
    memory_index Size = GetEffectiveSizeFor(Arena, SizeInit, Alignment);

    Assert((Arena->Used + Size) <= Arena->Size);
    memory_index AlignmentOffset = GetAlignmentOffset(Arena, Alignment);
    void *Result = Arena->Base + Arena->Used + AlignmentOffset;
    Arena->Used += Size;

    return(Result);
}

// NOTE(georgy): This is generally not for production use, this is probably
// only really something we need during testing, but who knows
inline char *
PushString(memory_arena *Arena, char *Source)
{
    uint32 Size = 1;
    for(char *At = Source;
        *At;
        At++)
    {
        Size++;
    }

    char *Dest = (char *)PushSize_(Arena, Size);
    for(uint32 CharIndex = 0;
        CharIndex < Size;
        CharIndex++)
    {
        Dest[CharIndex] = Source[CharIndex];
    }

    return(Dest);
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

inline void
SubArena(memory_arena *Result, memory_arena *Arena, memory_index Size, memory_index Alignment = 16)
{
    Result->Size = Size;
    Result->Base = (uint8 *)PushSize(Arena, Size, Alignment);
    Result->Used = 0;
    Result->TempCount = 0;
}


#define ZeroStruct(Instance) ZeroSize(sizeof(Instance), &(Instance))
#define ZeroArray(Count, Pointer) ZeroSize(Count*sizeof((Pointer)[0]), (Pointer))
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

inline void *
Copy(memory_index Size, void *SourceInit, void *DestInit)
{
    uint8 *Source = (uint8 *)SourceInit;
    uint8 *Dest = (uint8 *)DestInit;

    while(Size--)
    {
        *Dest++ = *Source++;
    }

    return(DestInit);
}

#include "handmade_world.h"
#include "handmade_sim_region.h"
#include "handmade_entity.h"
#include "handmade_render_group.h"
#include "handmade_random.h"
#include "handmade_asset.h"
#include "handmade_audio.h"

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
    float X, Y;
};

struct hero_bitmap_ids
{
    bitmap_id Head;
    bitmap_id Torso;
    bitmap_id Legs;
};

struct particle_cel
{
    real32 Density;
    v3 VelocityTimesDensity;
};
struct particle
{
    bitmap_id BitmapID;
    v3 P;
    v3 dP;
    v3 ddP;
    v4 Color;
    v4 dColor;
};

struct game_state
{
    bool32 IsInitialized;

    memory_arena WorldArena;
    world *World;

    real32 TypicalFloorHeight;

    uint32 CameraFollowingEntityIndex;
    world_position CameraP;

    controlled_hero ControlledHeroes[ArrayCount(((game_input *)0)->Controllers)];

    // TODO(george): Change the name to "Stored entity"
    uint32 LowEntityCount;
    low_entity LowEntities[100000];    

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

    loaded_bitmap TestDiffuse; // TODO(george): Re-fill this guy with gray
    loaded_bitmap TestNormal;  

    random_series EffectsEntropy; // NOTE(georgy): This is entropy that doesn't affect the gameplay

    audio_state AudioState;
    playing_sound *Music;

#define PARTICLE_CEL_DIM 16
    uint32 NextParticle;
    particle Particles[256];
    particle_cel ParticleCels[PARTICLE_CEL_DIM][PARTICLE_CEL_DIM];
};

struct task_with_memory 
{
    bool32 BeingUsed;
    memory_arena Arena;

    temporary_memory MemoryFlush;
};

struct transient_state
{
    bool32 IsInitialized;
    memory_arena TranArena;

    task_with_memory Tasks[4];

    game_assets *Assets;

    uint32 GroundBufferCount;
    ground_buffer *GroundBuffers;
    platform_work_queue *HighPriorityQueue;
    platform_work_queue *LowPriorityQueue;

    uint32 EnvMapWidth;
    uint32 EnvMapHeight;
    // NOTE(george): 0 is bottom, 1 is middle, 2 is top
    environment_map EnvMaps[3];
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

global_variable platform_api Platform;

internal task_with_memory *BeginTaskWithMemory(transient_state *TranState);
internal void EndTaskWithMemory(task_with_memory *Task);

#endif

