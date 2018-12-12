#include "handmade.h"
#include "handmade_render_group.cpp"
#include "handmade_world.cpp"
#include "handmade_sim_region.cpp"
#include "handmade_entity.cpp"

internal void
GameOutputSound(game_state *GameState, game_sound_output_buffer *SoundBuffer, int ToneHz)
{ 
    int16 ToneVolume = 3000;
    int WavePeriod = SoundBuffer->SamplesPerSecond/ToneHz;

    int16 *SampleOut = SoundBuffer->Samples;
    for(int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; SampleIndex++)
    {
#if 0
        real32 SineValue = sinf(GameState->tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
#else
        int16 SampleValue = 0;
#endif
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

#if 0
		GameState->tSine += 2.0f*Pi32*1.0f / (real32)WavePeriod;

        if (GameState->tSine > 2.0f*Pi32)
        {
            GameState->tSine -= 2.0f*Pi32;
        }
#endif
    }
}

#pragma pack(push, 1)
struct bitmap_header
{
    uint16 FileType;
    uint32 FileSize;     
    uint16 Reserved1;    
    uint16 Reserved2;    
    uint32 BitmapOffset; 
	uint32 Size;          
	int32 Width;         
	int32 Height;        
	uint16 Planes;        
	uint16 BitsPerPixel; 
    uint32 Compression;
    uint32 SizeOfBitmap;
    int32 HorzResolution;
    int32 VertResolution;
    uint32 ColorsUsed;
    uint32 ColorsImportant;

    uint32 RedMask;
    uint32 GreenMask;
    uint32 BlueMask;
};
#pragma pack(pop)

internal loaded_bitmap
DEBUGLoadBMP(thread_context *Thread, debug_platform_read_entire_file *ReadEntireFile, char *Filename)
{
    loaded_bitmap Result = {};

    debug_read_file_result ReadResult = ReadEntireFile(Thread, Filename);
    if(ReadResult.ContentsSize != 0)
    {
        bitmap_header *Header = (bitmap_header *)ReadResult.Contents;
        uint32 *Pixels = (uint32 *)((uint8 *)ReadResult.Contents + Header->BitmapOffset);
        Result.Memory = Pixels;
        Result.Width = Header->Width;
        Result.Height = Header->Height;

        Assert(Header->Compression == 3);
        
        // NOTE(george): If you are using this generically for some reason,
        // please remember that BMP files CAN GO IN EITHER DIRECTION and
        // the height will be negative for top-down.
        // (Also, there can be compression, etc., etc... DON'T think this
        // is complete BMP loading code because it isn't!)

        // NOTE(george): Byte order in memory is determined by the Header itself,
        // so we have to read out the masks and convert the pixels ourselves.
        uint32 RedMask = Header->RedMask;
        uint32 GreenMask = Header->GreenMask;
        uint32 BlueMask = Header->BlueMask;
        uint32 AlphaMask = ~(RedMask | GreenMask | BlueMask);

        bit_scan_result RedScan = FindLeastSignificantSetBit(RedMask);
        bit_scan_result GreenScan = FindLeastSignificantSetBit(GreenMask);
        bit_scan_result BlueScan = FindLeastSignificantSetBit(BlueMask);
        bit_scan_result AlphaScan = FindLeastSignificantSetBit(AlphaMask);
        
        Assert(RedScan.Found);
        Assert(GreenScan.Found);
        Assert(BlueScan.Found);
        Assert(AlphaScan.Found);

        int32 RedShiftDown = (int32)RedScan.Index;
        int32 GreenShiftDown = (int32)GreenScan.Index;
        int32 BlueShiftDown = (int32)BlueScan.Index;
        int32 AlphaShiftDown = (int32)AlphaScan.Index;
        
        uint32 *SourceDest = Pixels;
        for(int32 Y = 0; Y < Header->Height; Y++)
        {
            for(int32 X = 0; X < Header->Width; X++)
            {
                uint32 C = *SourceDest;

                v4 Texel = {(real32)((C & RedMask) >> RedShiftDown),
                            (real32)((C & GreenMask) >> GreenShiftDown),
                            (real32)((C & BlueMask) >> BlueShiftDown),
                            (real32)((C & AlphaMask) >> AlphaShiftDown)};

                Texel = SRGB255ToLinear1(Texel);
#if 1
                Texel.rgb *= Texel.a;
#endif
                Texel = Linear1ToSRGB255(Texel);

                *SourceDest++ = ((uint32)(Texel.a + 0.5f) << 24) |
                                ((uint32)(Texel.r + 0.5f) << 16) |
                                ((uint32)(Texel.g + 0.5f) << 8) |
                                ((uint32)(Texel.b + 0.5f) << 0); 
            }
        }
    }

    Result.Pitch = -Result.Width*BITMAP_BYTES_PER_PIXEL;
    Result.Memory = (uint8*)Result.Memory - Result.Pitch*(Result.Height-1);

    return(Result);
}

struct add_low_entity_result
{   
    low_entity *Low;
    uint32 LowIndex;
};
internal add_low_entity_result
AddLowEntity(game_state *GameState, entity_type Type, world_position P)
{
    Assert(GameState->LowEntityCount < ArrayCount(GameState->LowEntities));    
    uint32 EntityIndex = GameState->LowEntityCount++;

    low_entity *EntityLow = GameState->LowEntities + EntityIndex;
    *EntityLow = {};
    EntityLow->Sim.Type = Type;
    EntityLow->Sim.Collision = GameState->NullCollision;
    EntityLow->P = NullPosition();

    ChangeEntityLocation(&GameState->WorldArena, GameState->World, EntityIndex, EntityLow, P);

    add_low_entity_result Result;
    Result.Low = EntityLow;
    Result.LowIndex = EntityIndex;    

    // TODO(george): Do we need to have a begin/end paradigm for adding
    // entities so that they can be brought into the high set when they
    // are added and are in the camera region?

    return(Result);
}

internal add_low_entity_result
AddGroundedEntity(game_state *GameState, entity_type Type, world_position P, 
                  sim_entity_collision_volume_group *Collision)
{
    add_low_entity_result Entity = AddLowEntity(GameState, Type, P);
    Entity.Low->Sim.Collision = Collision;
    return(Entity);
}

inline world_position
ChunkPositionFromTilePosition(world *World, int32 AbsTileX, int32 AbsTileY, int32 AbsTileZ,
                              v3 AdditionalOffset = V3(0, 0, 0))
{
    world_position BasePos = {};

    real32 TileSideInMeters = 1.4f;
    real32 TileDepthInMeters = 3.0f;

    v3 TileDim = V3(TileSideInMeters, TileSideInMeters, TileDepthInMeters);
    v3 Offset = Hadamard(TileDim, V3((real32)AbsTileX, (real32)AbsTileY, (real32)AbsTileZ));
    world_position Result = MapIntoChunkSpace(World, BasePos, Offset + AdditionalOffset);

    Assert(IsCanonical(World, Result.Offset_));

    return(Result);
} 

internal add_low_entity_result
AddStandardRoom(game_state *GameState, uint32 CenterTileX, uint32 CenterTileY, uint32 AbsTileZ)
{
    world_position P = ChunkPositionFromTilePosition(GameState->World, CenterTileX, CenterTileY, AbsTileZ);
    add_low_entity_result Entity = AddGroundedEntity(GameState, EntityType_Space, P, GameState->StandardRoomCollision); 

    AddFlags(&Entity.Low->Sim, EntityFlag_Traversable);

    return(Entity);
}

internal add_low_entity_result
AddWall(game_state *GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    world_position P = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
    add_low_entity_result Entity = AddGroundedEntity(GameState, EntityType_Wall, P, GameState->WallCollision); 

    AddFlags(&Entity.Low->Sim, EntityFlag_Collides);

    return(Entity);
}

internal add_low_entity_result
AddStair(game_state *GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    
    world_position P = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
    add_low_entity_result Entity = AddGroundedEntity(GameState, EntityType_Stairwell, P, GameState->StairCollision); 

    Entity.Low->Sim.WalkableDim = Entity.Low->Sim.Collision->TotalVolume.Dim.xy;
    Entity.Low->Sim.WalkableHeight = GameState->TypicalFloorHeight;

    AddFlags(&Entity.Low->Sim, EntityFlag_Collides);

    return(Entity);
}

internal void
InitHitPoints(low_entity *EntityLow, uint32 HitPointCount)
{
    Assert(HitPointCount <= ArrayCount(EntityLow->Sim.HitPoint));
    EntityLow->Sim.HitPointMax = 3;
    for(uint32 HitPointIndex = 0; HitPointIndex < EntityLow->Sim.HitPointMax; HitPointIndex++)
    {
        hit_point *HitPoint = EntityLow->Sim.HitPoint + HitPointIndex;
        HitPoint->Flags = 0;
        HitPoint->FilledAmount = HIT_POINT_SUB_COUNT;
    }
}

internal add_low_entity_result
AddSword(game_state *GameState)
{
    add_low_entity_result Entity = AddLowEntity(GameState, EntityType_Sword, NullPosition()); 
    Entity.Low->Sim.Collision = GameState->SwordCollision;

    AddFlags(&Entity.Low->Sim, EntityFlag_Nonspatial | EntityFlag_Moveable);

    return(Entity);
}

internal add_low_entity_result
AddPlayer(game_state *GameState)
{
    world_position P = GameState->CameraP;
    add_low_entity_result Entity = AddGroundedEntity(GameState, EntityType_Hero, P, GameState->PlayerCollision); 

    AddFlags(&Entity.Low->Sim, EntityFlag_Collides | EntityFlag_Moveable);

    InitHitPoints(Entity.Low, 3);

    add_low_entity_result Sword = AddSword(GameState);
    Entity.Low->Sim.Sword.Index = Sword.LowIndex;

    if(GameState->CameraFollowingEntityIndex == 0)
    {
        GameState->CameraFollowingEntityIndex = Entity.LowIndex;
    }
    
    return(Entity);
}

internal add_low_entity_result
AddMonstar(game_state *GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    world_position P = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
    add_low_entity_result Entity = AddGroundedEntity(GameState, EntityType_Monstar, P, GameState->MonstarCollision); 
    
    AddFlags(&Entity.Low->Sim, EntityFlag_Collides | EntityFlag_Moveable);

    InitHitPoints(Entity.Low, 3);    

    return(Entity);
}

internal add_low_entity_result
AddFamiliar(game_state *GameState, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    world_position P = ChunkPositionFromTilePosition(GameState->World, AbsTileX, AbsTileY, AbsTileZ);
    add_low_entity_result Entity = AddGroundedEntity(GameState, EntityType_Familiar, P, GameState->FamiliarCollision); 
    
    AddFlags(&Entity.Low->Sim, EntityFlag_Moveable);

    return(Entity);
}

internal void
DrawHitpoints(sim_entity *Entity, render_group *RenderGroup)
{
    if(Entity->HitPointMax >= 1)
    {
        v2 HealthDim = {0.2f, 0.2f};
        real32 SpacingX = 1.5f*HealthDim.x;
        v2 HitP = {-0.5f*(Entity->HitPointMax - 1)*SpacingX, -0.25f};
        v2 dHitP = {SpacingX, 0.0f};
        for(uint32 HealthIndex = 0; HealthIndex < Entity->HitPointMax; HealthIndex++)
        {
            hit_point *HitPoint = Entity->HitPoint + HealthIndex;
            v4 Color = {1.0f, 0.0f, 0.0f, 1.0f};
            if(HitPoint->FilledAmount == 0)
            {
                Color = V4(0.2f, 0.2f, 0.2f, 1.0f);
            }

            PushRect(RenderGroup, HitP, 0, HealthDim, Color, 0.0f);
            HitP += dHitP;
        }
    }
}

internal void
ClearCollisionRulesFor(game_state *GameState, uint32 StorageIndex)
{
    // TODO(george): Need to make a better data structure that allows
    // removal of collision rules without searching the entire table
    // NOTE(george): One way to make removal easy would be to always
    // add _both_ orders of the pairs of storage indices to the
    // hash table, so no matter which position the entity is in, 
    // you can always find it. Then, when you do your firts pass
    // through for removal, you just remember the original top
    // of the free list, and when you're done, do a pass through all
    // the new things on the free list, and remove the reverse of 
    // those pairs.
    for(uint32 HashBucket = 0;
        HashBucket < ArrayCount(GameState->CollisionRuleHash);
        HashBucket++)
    {
        for(pairwise_collision_rule **Rule = &GameState->CollisionRuleHash[HashBucket];
            *Rule;
           ) 
        {
            if(((*Rule)->StorageIndexA == StorageIndex) ||
               ((*Rule)->StorageIndexB == StorageIndex))
            {
                pairwise_collision_rule *RemovedRule = *Rule;
                *Rule = (*Rule)->NextInHash;

                RemovedRule->NextInHash = GameState->FirstFreeCollisionRule;
                GameState->FirstFreeCollisionRule = RemovedRule;
            }
            else
            {
                Rule = &(*Rule)->NextInHash;
            }
        }    
    }
}

internal void
AddCollisionRule(game_state *GameState, uint32 StorageIndexA, uint32 StorageIndexB, bool32 CanCollide)
{   
    // TODO(george): Collapse this with ShouldCollide
    if(StorageIndexA > StorageIndexB)
    {
        uint32 Temp = StorageIndexA;
        StorageIndexA = StorageIndexB;
        StorageIndexB = Temp;
    }

    // TODO(george): Better hash function
    pairwise_collision_rule *Found = 0;
    uint32 HashBucket = StorageIndexA & (ArrayCount(GameState->CollisionRuleHash) - 1);
    for(pairwise_collision_rule *Rule = GameState->CollisionRuleHash[HashBucket];
        Rule;
        Rule = Rule->NextInHash)
    {
        if((Rule->StorageIndexA == StorageIndexA) &&
           (Rule->StorageIndexB == StorageIndexB))
        {
            Found = Rule;
            break;
        }
    }    

    if(!Found)
    {
        Found = GameState->FirstFreeCollisionRule;
        if(Found)
        {
            GameState->FirstFreeCollisionRule = Found->NextInHash;
        } 
        else
        {
            Found = PushStruct(&GameState->WorldArena, pairwise_collision_rule);
        }
        
        Found->NextInHash = GameState->CollisionRuleHash[HashBucket];
        GameState->CollisionRuleHash[HashBucket] = Found;
    }

    if(Found)
    {
        Found->StorageIndexA = StorageIndexA;
        Found->StorageIndexB = StorageIndexB;
        Found->CanCollide = CanCollide;
    }
}

internal sim_entity_collision_volume_group * 
MakeSimpleGroundedCollision(game_state *GameState, real32 DimX, real32 DimY, real32 DimZ)
{
    // TODO(george): Change to using fundamental types arena, etc.
    sim_entity_collision_volume_group *Group = PushStruct(&GameState->WorldArena, sim_entity_collision_volume_group);
    Group->VolumeCount = 1;
    Group->Volumes = PushArray(&GameState->WorldArena, Group->VolumeCount, sim_entity_collision_volume);
    Group->TotalVolume.OffsetP = V3(0, 0, 0.5f*DimZ);
    Group->TotalVolume.Dim = V3(DimX, DimY, DimZ);
    Group->Volumes[0] = Group->TotalVolume;

    return(Group);
}

internal sim_entity_collision_volume_group * 
MakeNullCollision(game_state *GameState)
{
    // TODO(george): Change to using fundamental types arena, etc.
    sim_entity_collision_volume_group *Group = PushStruct(&GameState->WorldArena, sim_entity_collision_volume_group);
    Group->VolumeCount = 0;
    Group->Volumes = 0;
    Group->TotalVolume.OffsetP = V3(0, 0, 0);
    // TODO(george): Should this be negative?
    Group->TotalVolume.Dim = V3(0, 0, 0);

    return(Group);
}    

internal void
FillGroundChunk(transient_state *TranState, game_state *GameState, ground_buffer *GroundBuffer, world_position *ChunkP)
{
    temporary_memory GroundMemory = BeginTemporaryMemory(&TranState->TranArena);    
    render_group *RenderGroup = AllocateRenderGroup(&TranState->TranArena, Megabytes(4), 1.0f);

    Clear(RenderGroup, V4(1.0f, 1.0f, 0.0f, 1.0f));

    loaded_bitmap *Buffer = &GroundBuffer->Bitmap;

    GroundBuffer->P = *ChunkP;

    // TODO(george): Make random number generation more systematic
    for(int32 ChunkOffsetY = -1;
        ChunkOffsetY <= 1;
        ChunkOffsetY++)
    {
        for(int32 ChunkOffsetX = -1;
        ChunkOffsetX <= 1;
        ChunkOffsetX++)
        {
            int32 ChunkX = ChunkP->ChunkX + ChunkOffsetX;
            int32 ChunkY = ChunkP->ChunkY + ChunkOffsetY;
            int32 ChunkZ = ChunkP->ChunkZ;
            random_series Series = RandomSeed(139*ChunkX + 593*ChunkY + 329*ChunkZ);

            real32 Width = (real32)Buffer->Width;
            real32 Height = (real32)Buffer->Height;
            
            v2 Center = V2(ChunkOffsetX*Width, -ChunkOffsetY*Height);
            for(uint32 GrassIndex = 0;
                GrassIndex < 100;
                GrassIndex++)
            {
                loaded_bitmap *Stamp;
                #if 0
                if(RandomChoice(&Series, 2))
                {
                    Stamp = GameState->Grass + RandomChoice(&Series, ArrayCount(GameState->Grass));
                }
                else
                #endif
                {
                    Stamp = GameState->Stone + RandomChoice(&Series, ArrayCount(GameState->Stone));
                }

                v2 BitmapCenter = 0.5f*V2i(Stamp->Width, Stamp->Height); 
                v2 Offset = {Width*RandomUnilateral(&Series), Height*RandomUnilateral(&Series)};
                v2 P = Center + Offset - BitmapCenter;
                PushBitmap(RenderGroup, Stamp, P, 0.0f, V2(0.0f, 0.0f));    
            }
        }
    }

    RenderGroupToOutput(RenderGroup, Buffer);
    EndTemporaryMemory(GroundMemory);
}

internal void
ClearBitmap(loaded_bitmap *Bitmap)
{
    if(Bitmap->Memory)
    {
        int32 TotalBitmapSize = Bitmap->Width*Bitmap->Height*BITMAP_BYTES_PER_PIXEL;
        ZeroSize(TotalBitmapSize, Bitmap->Memory);
    }
}

internal loaded_bitmap
MakeEmptyBitmap(memory_arena *Arena, int32 Width, int32 Height, bool32 ClearToZero = true)
{
    loaded_bitmap Result = {};

    Result.Width = Width;
    Result.Height = Height;
    Result.Pitch = Result.Width*BITMAP_BYTES_PER_PIXEL;
    int32 TotalBitmapSize = Width*Height*BITMAP_BYTES_PER_PIXEL;
    Result.Memory = PushSize(Arena, TotalBitmapSize);
    if(ClearToZero)
    {
        ClearBitmap(&Result);
    }

    return(Result);
}

internal void
MakeSphereNormalMap(loaded_bitmap *Bitmap, real32 Roughless)
{
    real32 InvWidth = 1.0f / (real32)(Bitmap->Width - 1);
    real32 InvHeight = 1.0f / (real32)(Bitmap->Height - 1);

    uint8 *Row = (uint8 *)Bitmap->Memory;
    for(int32 Y = 0;
        Y < Bitmap->Height;
        Y++)
    {
        uint32 *Pixel = (uint32 *)Row;
        for(int32 X = 0;
            X < Bitmap->Width;
            X++)
        {
            v2 BitmapUV = {InvWidth*(real32)X, InvHeight*(real32)Y};

            real32 Nx = 2.0f*BitmapUV.x - 1.0f;
            real32 Ny = 2.0f*BitmapUV.y - 1.0f;
            
            real32 RootTerm = 1.0f - Nx*Nx - Ny*Ny;
            v3 Normal = {0, 0, 1};            
            real32 Nz = 0.0f;
            if(RootTerm >= 0.0f)
            {   
                Nz = SquareRoot(RootTerm);
                Normal = {Nx, Ny, Nz};
            }

            v4 Color = {255.0f*(0.5f*(Normal.x + 1.0f)), 
                        255.0f*(0.5f*(Normal.y + 1.0f)), 
                        255.0f*(0.5f*(Normal.z + 1.0f)), 
                        255.0f*Roughless};
        
            *Pixel++ = ((uint32)(Color.w + 0.5f) << 24) |
                        ((uint32)(Color.x + 0.5f) << 16) |
                        ((uint32)(Color.y + 0.5f) << 8) |
                        ((uint32)(Color.z + 0.5f) << 0);   

        }

        Row += Bitmap->Pitch;
    }
}

#if 0
internal void
RequestGroundBuffers(world_position CenterP, rectangle3 Bounds)
{
    // We should substract offset, shouldn't we?
    Bound = Offset(Bounds, CenterP.Offset_);
    CenterP.Offset_ = V3(0, 0, 0);

    // TODO(george): This is just a test fill
    FillGroundChunk(TranState, GameState, TranState->GroundBuffers, &GameState->CameraP);
}
#endif

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
	Assert((&Input->Controllers[0].Start - &Input->Controllers[0].Buttons[0]) == 
            (ArrayCount(Input->Controllers[0].Buttons) - 1));
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    uint32 GroundBufferWidth = 256;
    uint32 GroundBufferHeight = 256;
    
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    if (!Memory->IsInitialized)
    {
        uint32 TilesPerWidth = 17;
        uint32 TilesPerHeight = 9;

        GameState->TypicalFloorHeight = 3.0f;
        GameState->MetersToPixels = 42.0f;
        GameState->PixelsToMeters = 1.0f / GameState->MetersToPixels;

        v3 WorldChunkDimInMeters = {GameState->PixelsToMeters*(real32)GroundBufferWidth,
                                    GameState->PixelsToMeters*(real32)GroundBufferHeight,
                                    GameState->TypicalFloorHeight};

        InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
                        (uint8 *)Memory->PermanentStorage + sizeof(game_state));                     

        // NOTE(george): Reserve entity slot 0 for the null entity
        AddLowEntity(GameState, EntityType_Null, NullPosition()); 

        GameState->World = PushStruct(&GameState->WorldArena, world);
        world *World = GameState->World;

        InitializeWorld(World, WorldChunkDimInMeters);

        real32 TileSideInMeters = 1.4f;
        real32 TileDepthInMeters = 3.0f;

        GameState->NullCollision = MakeNullCollision(GameState);
        GameState->SwordCollision = MakeSimpleGroundedCollision(GameState, 1.0f, 0.5f, 0.1f);
        GameState->StairCollision = MakeSimpleGroundedCollision(GameState, 
                                                                TileSideInMeters,
                                                                2.0f*TileSideInMeters,
                                                                1.1f*TileDepthInMeters);
        GameState->PlayerCollision = MakeSimpleGroundedCollision(GameState, 1.0f, 0.5f, 1.2f);
        GameState->MonstarCollision = MakeSimpleGroundedCollision(GameState, 1.0f, 0.5f, 0.5f);
        GameState->FamiliarCollision = MakeSimpleGroundedCollision(GameState, 1.0f, 0.5f, 0.5f);
        GameState->WallCollision = MakeSimpleGroundedCollision(GameState, 
                                                               TileSideInMeters,
                                                               TileSideInMeters,
                                                               TileDepthInMeters);
        GameState->StandardRoomCollision = MakeSimpleGroundedCollision(GameState, 
                                                                       TilesPerWidth*TileSideInMeters,
                                                                       TilesPerHeight*TileSideInMeters,
                                                                       0.9f*TileDepthInMeters);
                                                               

        GameState->Stone[0] = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/stone3.bmp");
        GameState->Grass[0] = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/grass3.bmp");            

        GameState->Backdrop = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/test_background.bmp");
        GameState->Shadow = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/hero_shadow.bmp");
        GameState->Tree = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/tree00.bmp");            
        GameState->Sword = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/sword1.bmp");
        GameState->Stairwell = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/stairwellup.bmp");

        hero_bitmaps *Bitmap;
        Bitmap = GameState->HeroBitmaps;
        Bitmap->Hero = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/hero_right.bmp");
        Bitmap->Align = V2(18, 56);
        Bitmap++;

        Bitmap->Hero = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/hero_back.bmp");
        Bitmap->Align = V2(18, 56);
        Bitmap++;

        Bitmap->Hero = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/hero_left.bmp");
        Bitmap->Align = V2(18, 56);
        Bitmap++;

        Bitmap->Hero = DEBUGLoadBMP(Thread, Memory->DEBUGPlatformReadEntireFile, "test/hero_front.bmp");
        Bitmap->Align = V2(18, 56);

        random_series Series = RandomSeed(1234);

        int32 ScreenBaseX = 0;
        int32 ScreenBaseY = 0;
        int32 ScreenBaseZ = 0;
        int32 ScreenX = ScreenBaseX;
        int32 ScreenY = ScreenBaseY;
        int32 AbsTileZ = ScreenBaseZ;

        // TODO(george): Replace all this with real world generation!s
        bool32 DoorLeft = false;
        bool32 DoorRight = false;
        bool32 DoorTop = false;
        bool32 DoorBottom = false;
        bool32 DoorUp = false;
        bool32 DoorDown = false;
        for (uint32 ScreenIndex = 0; ScreenIndex < 1000; ScreenIndex++)
        {
            // uint32 DoorDirection = RandomChoice(&Series, (DoorUp || DoorDown) ? 2 : 3);
            uint32 DoorDirection = RandomChoice(&Series, 2);

            bool32 CreatedZDoor = false;
            if(DoorDirection == 2)
            {
                CreatedZDoor = true;
                if(AbsTileZ == ScreenBaseZ)
                {
                    DoorUp = true;
                }
                else
                {
                    DoorDown = true;
                }
            }
            else if(DoorDirection == 1)
            {
                DoorRight = true;                
            }
            else
            {
                DoorTop = true;                
            }

            AddStandardRoom(GameState,
                            ScreenX*TilesPerWidth + TilesPerWidth/2, 
                            ScreenY*TilesPerHeight + TilesPerHeight/2,
                            AbsTileZ);

            for(uint32 TileY = 0; TileY < TilesPerHeight; TileY++)
            {
                for(uint32 TileX = 0; TileX < TilesPerWidth; TileX++)
                {
                    uint32 AbsTileX = ScreenX*TilesPerWidth + TileX;
                    uint32 AbsTileY = ScreenY*TilesPerHeight + TileY;

                    bool32 ShouldBeDoor = false;
                    if((TileX == 0) && (!DoorLeft || (TileY != TilesPerHeight/2)))
                    {
                        ShouldBeDoor = true;
                    }

                    if((TileX == TilesPerWidth - 1) && (!DoorRight || (TileY != TilesPerHeight/2)))
                    {
                        ShouldBeDoor = true;
                    }

                    if((TileY == 0) && (!DoorBottom || (TileX != TilesPerWidth/2)))
                    {
                        ShouldBeDoor = true;
                    }

                    if((TileY == TilesPerHeight - 1) && (!DoorTop || (TileX != TilesPerWidth/2)))
                    {
                        ShouldBeDoor = true;
                    }

                    if(ShouldBeDoor)
                    {
                        AddWall(GameState, AbsTileX, AbsTileY, AbsTileZ);
                    }
                    else if(CreatedZDoor)
                    {
                        if((TileX == 10) && (TileY == 5))
                        {
                            AddStair(GameState, AbsTileX, AbsTileY, DoorDown ? AbsTileZ - 1 : AbsTileZ);
                        }
                    }
                }
            }

            DoorLeft = DoorRight;   
            DoorBottom = DoorTop;

            if(CreatedZDoor)
            {
                DoorDown = !DoorDown;
                DoorUp = !DoorUp;
            }
            else
            {
                DoorUp = false;
                DoorDown = false;
            }

            DoorRight = false;
            DoorTop = false;

            if(DoorDirection == 2)
            {
                if(AbsTileZ == ScreenBaseZ)
                {
                    AbsTileZ = ScreenBaseZ + 1;
                }
                else
                {
                    AbsTileZ = ScreenBaseZ;
                }
            }
            else if(DoorDirection == 1)
            {
                ScreenX += 1;
            }
            else
            {
                ScreenY += 1;
            }
        }
    
        world_position NewCameraP = {};
        uint32 CameraTileX = ScreenBaseX*TilesPerWidth + 17/2;
        uint32 CameraTileY = ScreenBaseY*TilesPerHeight + 9/2;
        uint32 CameraTileZ = ScreenBaseZ;
        NewCameraP = ChunkPositionFromTilePosition(World, CameraTileX, CameraTileY, CameraTileZ);   
        GameState->CameraP = NewCameraP;

        AddMonstar(GameState, CameraTileX + 3, CameraTileY + 2, CameraTileZ);
        AddFamiliar(GameState, CameraTileX - 2, CameraTileY + 2, CameraTileZ);   

        Memory->IsInitialized = true; 
    }

    Assert(sizeof(transient_state) <= Memory->TransientStorageSize);
    transient_state *TranState = (transient_state *)Memory->TransientStorage;
    if(!TranState->IsInitialized)
    {
        InitializeArena(&TranState->TranArena, Memory->TransientStorageSize - sizeof(transient_state),
                        (uint8 *)Memory->TransientStorage + sizeof(transient_state));   

        TranState->GroundBufferCount = 64; // 128;
        TranState->GroundBuffers = PushArray(&TranState->TranArena, TranState->GroundBufferCount, ground_buffer);
        for(uint32 GroundBufferIndex = 0;
            GroundBufferIndex < TranState->GroundBufferCount;
            GroundBufferIndex++)
        {
            ground_buffer *GroundBuffer = TranState->GroundBuffers + GroundBufferIndex;
            GroundBuffer->Bitmap = MakeEmptyBitmap(&TranState->TranArena, GroundBufferWidth, GroundBufferHeight, false);
            GroundBuffer->P = NullPosition();
        }

        GameState->TestDiffuse = MakeEmptyBitmap(&TranState->TranArena, 256, 256, false);
        DrawRectangle(&GameState->TestDiffuse, V2(0, 0), V2i(GameState->TestDiffuse.Width, GameState->TestDiffuse.Height), V4(0.5f, 0.5f, 0.5f, 1.0f));
        GameState->TestNormal = MakeEmptyBitmap(&TranState->TranArena, GameState->TestDiffuse.Width, GameState->TestDiffuse.Height, false);
        MakeSphereNormalMap(&GameState->TestNormal, 0.0f);

        TranState->EnvMapWidth = 512;
        TranState->EnvMapHeight = 256;
        for(uint32 MapIndex = 0;
            MapIndex < ArrayCount(TranState->EnvMaps);
            MapIndex++)
        {
            environment_map *Map = TranState->EnvMaps + MapIndex;
            uint32 Width = TranState->EnvMapWidth;
            uint32 Height = TranState->EnvMapHeight;
            for(uint32 LODIndex = 0;
                LODIndex < ArrayCount(Map->LOD);
                LODIndex++)
            {
                Map->LOD[LODIndex] = MakeEmptyBitmap(&TranState->TranArena, Width, Height, false);
                Width >>= 1;
                Height >>= 1;
            }
        }

        TranState->IsInitialized = true;
    }

    if(Input->ExecutableReloaded)
    {
        for(uint32 GroundBufferIndex = 0;
            GroundBufferIndex < TranState->GroundBufferCount;
            GroundBufferIndex++)
        {
            ground_buffer *GroundBuffer = TranState->GroundBuffers + GroundBufferIndex;
            GroundBuffer->P = NullPosition();
        }
    }

    world *World = GameState->World;

    real32 MetersToPixels = GameState->MetersToPixels;
    real32 PixelsToMeters = 1.0f/MetersToPixels;

    for (int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ControllerIndex++)
    {
        game_controller_input *Controller = GetController(Input, ControllerIndex);
        controlled_hero *ConHero = GameState->ControlledHeroes + ControllerIndex;
        if(ConHero->EntityIndex == 0)
        {
            if(Controller->Start.EndedDown)
            {
                *ConHero = {};
                ConHero->EntityIndex = AddPlayer(GameState).LowIndex;
            }
        }
        else
        {
            ConHero->ddP = {};
            ConHero->dZ = 0.0f;

            if(Controller->IsAnalog)
            {
                // NOTE(george): Use analog movement tuning
                ConHero->ddP = v2{Controller->StickAverageX, Controller->StickAverageY};
            }
            else
            {
                // NOTE(george): Use digital movement tuning
                if(Controller->MoveUp.EndedDown)
                {
                    ConHero->ddP.y = 1.0f;
                }
                if(Controller->MoveDown.EndedDown)
                {
                    ConHero->ddP.y = -1.0f;
                }
                if(Controller->MoveLeft.EndedDown)
                {
                    ConHero->ddP.x = -1.0f;
                }
                if(Controller->MoveRight.EndedDown)
                {
                    ConHero->ddP.x = 1.0f;
                }
            }

            if(Controller->Start.EndedDown)
            {
                ConHero->dZ = 3.0f;
            }

            ConHero->dSword = {};
            if(Controller->ActionUp.EndedDown)
            {
                ConHero->dSword = V2(0.0f, 1.0f);
            }  
            if(Controller->ActionDown.EndedDown)
            {
                ConHero->dSword = V2(0.0f, -1.0f);
            }   
            if(Controller->ActionLeft.EndedDown)
            {
                ConHero->dSword = V2(-1.0f, 0.0f);
            }    
            if(Controller->ActionRight.EndedDown)
            {
                ConHero->dSword = V2(1.0f, 0.0f);
            }   
        }
    }

    // 
    // NOTE(george): Render
    // 

    temporary_memory RenderMemory = BeginTemporaryMemory(&TranState->TranArena);    
    render_group *RenderGroup = AllocateRenderGroup(&TranState->TranArena, Megabytes(4), GameState->MetersToPixels);

    loaded_bitmap DrawBuffer_ = {};
    loaded_bitmap *DrawBuffer = &DrawBuffer_;
    DrawBuffer->Width = Buffer->Width;
    DrawBuffer->Height = Buffer->Height;
    DrawBuffer->Pitch = Buffer->Pitch;
    DrawBuffer->Memory = Buffer->Memory;

    Clear(RenderGroup, V4(0.25f, 0.25f, 0.25f, 0.0f));

    v2 ScreenCenter = {0.5f*(real32)DrawBuffer->Width,
                       0.5f*(real32)DrawBuffer->Height};

    real32 ScreenWidthInMeters = DrawBuffer->Width*PixelsToMeters;
    real32 ScreenHeightInMeters = DrawBuffer->Height*PixelsToMeters;
    rectangle3 CameraBoundsInMeters = RectCenterDim(V3(0, 0, 0), 
                                            V3(ScreenWidthInMeters, ScreenHeightInMeters, 0));    

    for(uint32 GroundBufferIndex = 0;
        GroundBufferIndex < TranState->GroundBufferCount;
        GroundBufferIndex++)
    {
        ground_buffer *GroundBuffer = TranState->GroundBuffers + GroundBufferIndex;
        if(IsValid(GroundBuffer->P))
        {
            loaded_bitmap *Bitmap = &GroundBuffer->Bitmap;
            v3 Delta = Substract(GameState->World, &GroundBuffer->P, &GameState->CameraP);
            PushBitmap(RenderGroup, Bitmap, Delta.xy, Delta.z, 0.5f*V2i(Bitmap->Width, Bitmap->Height));
        }
    }            

    {
        world_position MinChunkP = MapIntoChunkSpace(World, GameState->CameraP, GetMinCorner(CameraBoundsInMeters));
        world_position MaxChunkP = MapIntoChunkSpace(World, GameState->CameraP, GetMaxCorner(CameraBoundsInMeters));    

        for(int32 ChunkZ = MinChunkP.ChunkZ; ChunkZ <= MaxChunkP.ChunkZ; ChunkZ++) 
        {
            for(int32 ChunkY = MinChunkP.ChunkY; ChunkY <= MaxChunkP.ChunkY; ChunkY++)
            {
                for(int32 ChunkX = MinChunkP.ChunkX; ChunkX <= MaxChunkP.ChunkX; ChunkX++)
                {
                    world_position ChunkCenterP = CenteredChunkPoint(ChunkX, ChunkY, ChunkZ);
                    v3 RelP = Substract(World, &ChunkCenterP, &GameState->CameraP);

                    // TODO(george): This is super inefficient, fix it!
                    real32 FurthestBufferLengthSq = 0.0f;
                    ground_buffer *FurthestBuffer = 0;
                    for(uint32 GroundBufferIndex = 0;
                        GroundBufferIndex < TranState->GroundBufferCount;
                        GroundBufferIndex++)
                    {
                        ground_buffer *GroundBuffer = TranState->GroundBuffers + GroundBufferIndex;
                        if(AreInTheSameChunk(World, &GroundBuffer->P, &ChunkCenterP))
                        {
                            FurthestBuffer = 0;
                            break;
                        }
                        else if(IsValid(&GroundBuffer->P))
                        {
                            v3 RelP = Substract(World, &GroundBuffer->P, &GameState->CameraP);
                            real32 BufferLengthSq = LengthSq(RelP.xy);
                            if(FurthestBufferLengthSq < BufferLengthSq)
                            {
                                FurthestBufferLengthSq = BufferLengthSq;
                                FurthestBuffer = GroundBuffer;
                            }
                        }
                        else
                        {
                            FurthestBufferLengthSq = Real32Maximum;
                            FurthestBuffer = GroundBuffer;
                        }
                    }

                    if(FurthestBuffer)
                    {
                        FillGroundChunk(TranState, GameState, FurthestBuffer, &ChunkCenterP);
                    }

                    PushRectOutline(RenderGroup, RelP.xy, 0.0f, World->ChunkDimInMeters.xy, V4(1.0f, 1.0f, 0.0f, 1.0f));
                }
            }
        }
    }

    // TODO(george): How big do we actually want to expand here?
    v3 SimBoundExpansion = V3(15.0f, 15.0f, 15.0f);
    rectangle3 SimBounds = AddRadiusTo(CameraBoundsInMeters, SimBoundExpansion);

    temporary_memory SimMemory = BeginTemporaryMemory(&TranState->TranArena);
    sim_region *SimRegion = BeginSim(&TranState->TranArena, GameState, GameState->World, GameState->CameraP, SimBounds, Input->dtForFrame);

    // TODO(george): Move this out to handmade_entity.cpp
    for(uint32 EntityIndex = 0; EntityIndex < SimRegion->EntityCount; EntityIndex++)
    {
        sim_entity *Entity = SimRegion->Entities + EntityIndex;

        if(Entity->Updatable)
        {
            real32 dt = Input->dtForFrame;        

            // TODO(george): This is incorrect, should be computed after update!!!
            real32 ShadowAlpha = 1.0f - 0.5f*Entity->P.z;
            if(ShadowAlpha < 0)
            {
                ShadowAlpha = 0.0f;
            }

            move_spec MoveSpec = DefaultMoveSpec();
            v3 ddP = {};

            render_basis *Basis = PushStruct(&TranState->TranArena, render_basis);
            RenderGroup->DefaultBasis = Basis;

            hero_bitmaps *HeroBitmaps = &GameState->HeroBitmaps[Entity->FacingDirection];
            switch(Entity->Type)
            {
                case EntityType_Hero:
                {
                    for(uint32 ControlIndex = 0;
                        ControlIndex < ArrayCount(GameState->ControlledHeroes);
                        ControlIndex++)
                    {
                        controlled_hero *ConHero = GameState->ControlledHeroes + ControlIndex;

                        if(Entity->StorageIndex == ConHero->EntityIndex)
                        {
                            if(ConHero->dZ != 0.0f)
                            {
                                Entity->dP.z = ConHero->dZ;
                            }

                            MoveSpec.UnitMaxAccelVector = true;
                            MoveSpec.Speed = 50.0f;
                            MoveSpec.Drag = 8.0f;
                            ddP = V3(ConHero->ddP, 0);

                            if((ConHero->dSword.x != 0) || (ConHero->dSword.y != 0))
                            {
                                sim_entity *Sword = Entity->Sword.Ptr;
                                if(Sword && IsSet(Sword, EntityFlag_Nonspatial))
                                {
                                    Sword->DistanceLimit = 5.0f;
                                    MakeEntitySpatial(Sword, Entity->P, Entity->dP + 5.0f*V3(ConHero->dSword, 0.0f));

                                    AddCollisionRule(GameState, Sword->StorageIndex, Entity->StorageIndex, false);        
                                }
                            }
                        }
                    }

                    // TODO(george): Z!!!
                    PushBitmap(RenderGroup, &GameState->Shadow, V2(0, 0), 0, V2(20, 12), ShadowAlpha, 0);                  
                    PushBitmap(RenderGroup, &HeroBitmaps->Hero, V2(0, 0), 0, HeroBitmaps->Align);  

                    DrawHitpoints(Entity, RenderGroup);
                } break;

                case EntityType_Wall:
                {
                    PushBitmap(RenderGroup, &GameState->Tree, V2(0, 0), 0, V2(40, 55));
                } break;

                case EntityType_Stairwell:
                {
                    PushRect(RenderGroup, V2(0, 0), 0, Entity->WalkableDim, V4(1, 0.5f, 0, 1), 0.0f);
                    PushRect(RenderGroup, V2(0, 0), Entity->WalkableHeight, Entity->WalkableDim, V4(1, 1, 0, 1), 0.0f);
                } break;

                case EntityType_Sword:
                {
                    MoveSpec.UnitMaxAccelVector = false;
                    MoveSpec.Speed = 0.0f;
                    MoveSpec.Drag = 0.0f;

                    // TODO(george): IMPORTANT(george): Add the ability in the collision 
                    // routines to understand a movement limit for an entity, and
                    // then update this routine to use to know when to kill the
                    // sword.
                    // TODO(george): Need to handle the fact that DistanceTraveled
                    // might not have enough distance for the total entity move
                    // for the frame!
                    if(Entity->DistanceLimit == 0.0f)
                    {
                        ClearCollisionRulesFor(GameState, Entity->StorageIndex);
                        MakeEntityNonSpatial(Entity);
                    }

                    PushBitmap(RenderGroup, &GameState->Shadow, V2(0, 0), 0, V2(20, 12), ShadowAlpha, 0);                  
                    PushBitmap(RenderGroup, &GameState->Sword, V2(0, 0), 0, V2(7, 19));
                } break;

                case EntityType_Familiar:
                {
                    sim_entity *ClosestHero = 0;
                    real32 ClosestHeroDSq = Square(10.0f); // NOTE(george): Ten meter maximum search!

#if 0
                    // TODO(george): Make spatial queries easy for things!
                    sim_entity *TestEntity = SimRegion->Entities;
                    for(uint32 TestEntityIndex = 0; TestEntityIndex < SimRegion->EntityCount; TestEntityIndex++, TestEntity++)
                    {
                        if(TestEntity->Type == EntityType_Hero)
                        {
                            real32 TestDSq = LengthSq(TestEntity->P - Entity->P);
                            if(ClosestHeroDSq > TestDSq)
                            {
                                ClosestHero = TestEntity;
                                ClosestHeroDSq = TestDSq;
                            }
                        }
                    }
#endif

                    if(ClosestHero && (ClosestHeroDSq > Square(3.0f)))
                    {
                        real32 Acceleration = 0.5f;
                        real32 OneOverLength = Acceleration / SquareRoot(ClosestHeroDSq);
                        ddP = OneOverLength*(ClosestHero->P - Entity->P);
                    }

                    MoveSpec.UnitMaxAccelVector = true;
                    MoveSpec.Speed = 50.0f;
                    MoveSpec.Drag = 8.0f;

                    Entity->tBob += dt;
                    if(Entity->tBob > 2.0f*Pi32)
                    {
                        Entity->tBob -= 2.0f*Pi32;
                    }
                    real32 BobSin = Sin(2.0f*Entity->tBob);
                    PushBitmap(RenderGroup, &GameState->Shadow, V2(0, 0), 0, V2(20, 12), (0.5f*ShadowAlpha) + 0.2f*BobSin, 0);
                    PushBitmap(RenderGroup, &HeroBitmaps->Hero, V2(0, 0), 0.5f*BobSin, HeroBitmaps->Align);
                } break;

                case EntityType_Monstar:
                {
                    PushBitmap(RenderGroup, &GameState->Shadow, V2(0, 0), 0, V2(20, 12), ShadowAlpha, 0);                  
                    PushBitmap(RenderGroup, &HeroBitmaps->Hero, V2(0, 0), 0, HeroBitmaps->Align);      
                    DrawHitpoints(Entity, RenderGroup);            
                } break;

                case EntityType_Space:
                {
#if 1
                    for(uint32 VolumeIndex = 0;
                        VolumeIndex < Entity->Collision->VolumeCount;
                        VolumeIndex++)
                    {
                        sim_entity_collision_volume *Volume = Entity->Collision->Volumes + VolumeIndex;

                        PushRectOutline(RenderGroup, Volume->OffsetP.xy, 0, Volume->Dim.xy, V4(0, 0, 1, 1), 0.0f);
                    }
#endif
                } break;

                default:
                {
                    InvalidCodePath;
                }
            }

            if(!IsSet(Entity, EntityFlag_Nonspatial) && 
                IsSet(Entity, EntityFlag_Moveable))
            {
                MoveEntity(GameState, SimRegion, Entity, Input->dtForFrame, &MoveSpec, ddP);
            }

            Basis->P = GetEntityGroundPoint(Entity);
        }
    }

    GameState->Time += Input->dtForFrame;
    real32 Angle = 0.1f*GameState->Time;

    v3 MapColor[] = 
    {
        {1, 0, 0},
        {0, 1, 0},
        {0, 0, 1}
    };
    for(uint32 MapIndex = 0;
        MapIndex < ArrayCount(TranState->EnvMaps);
        MapIndex++)
    {
        environment_map *Map = TranState->EnvMaps + MapIndex;
        loaded_bitmap *LOD = Map->LOD + 0;
        bool32 RowCheckerOn = false;
        int32 CheckerWidth = 16;
        int32 CheckerHeight = 16;
        for(int32 Y = 0;
            Y < LOD->Height;
            Y += CheckerHeight)
        {
            bool32 CheckerOn = RowCheckerOn;
            for(int32 X = 0;
                X < LOD->Width;
                X += CheckerWidth)
            {
                v4 Color = CheckerOn ? ToV4(MapColor[MapIndex], 1.0f) : V4(0, 0, 0, 1);
                v2 MinP = V2i(X, Y);
                v2 MaxP = MinP + V2i(CheckerWidth, CheckerHeight);
                DrawRectangle(LOD, MinP, MaxP, Color);
                CheckerOn = !CheckerOn;
            }

            RowCheckerOn = !RowCheckerOn;
        }
    }

    Angle = 0.0f;

    v2 Origin = ScreenCenter;
#if 1
    v2 XAxis = 100.0f*V2(Cos(Angle), Sin(Angle));
    v2 YAxis = Perp(XAxis);
#else
    v2 XAxis = {60.0f, 0};
    v2 YAxis = {0, 60.0f};
#endif
    uint32 PIndex = 0;
    render_entry_coordinate_system *C = CoordinateSystem(RenderGroup, Origin - 0.5f*XAxis - 0.5f*YAxis, XAxis, YAxis, V4(1, 1, 1, 1), &GameState->TestDiffuse, 
                                                         &GameState->TestNormal, 
                                                         TranState->EnvMaps + 2, TranState->EnvMaps + 1, TranState->EnvMaps + 0);

    v2 MapP = {0.0f, 0.0f};
    for(uint32 MapIndex = 0;
        MapIndex < ArrayCount(TranState->EnvMaps);
        MapIndex++)
    {
        environment_map *Map = TranState->EnvMaps + MapIndex;
        loaded_bitmap *LOD = Map->LOD + 0;

        XAxis = 0.5f * V2((real32)LOD->Width, 0.0f);
        YAxis = 0.5f * V2(0.0f, (real32)LOD->Height);

        CoordinateSystem(RenderGroup, MapP, XAxis, YAxis, V4(1, 1, 1, 1), LOD, 0, 0, 0, 0);
        MapP += YAxis + V2(0.0f, 6.0f);
    }

#if 0
    Saturation(RenderGroup, 0.0f);
#endif

    RenderGroupToOutput(RenderGroup, DrawBuffer);

    EndSim(SimRegion, GameState);    
    EndTemporaryMemory(SimMemory);
    EndTemporaryMemory(RenderMemory);

    CheckArena(&GameState->WorldArena);
    CheckArena(&TranState->TranArena);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer, 400);
}