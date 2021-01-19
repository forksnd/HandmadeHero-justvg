#include "handmade_world_mode.h"

internal low_entity *
BeginLowEntity(game_mode_world *WorldMode, entity_type Type, world_position P)
{
    Assert(!WorldMode->CreationBufferLocked);
    WorldMode->CreationBufferLocked = true;

    low_entity *EntityLow = &WorldMode->CreationBuffer;
    EntityLow->Sim.StorageIndex.Value = ++WorldMode->LastUsedEntityStorageIndex;

    // TODO(georgy): Worry about this taking awhile once the entities are large (sparse clear?)
    ZeroStruct(*EntityLow);

    EntityLow->Sim.Type = Type;
    EntityLow->Sim.Collision = WorldMode->NullCollision;
    EntityLow->P = P;

    return(EntityLow);
}

internal void
PackEntityIntoChunk(world *World, low_entity *Entity)
{

}

internal void
EndEntity(game_mode_world *WorldMode, low_entity *EntityLow)
{
    Assert(WorldMode->CreationBufferLocked);
    WorldMode->CreationBufferLocked = false;

    PackEntityIntoChunk(WorldMode->World, EntityLow);
}

internal low_entity *
BeginGroundedEntity(game_mode_world *WorldMode, entity_type Type, world_position P, 
                  sim_entity_collision_volume_group *Collision)
{
    low_entity *Entity = BeginLowEntity(WorldMode, Type, P);
    Entity->Sim.Collision = Collision;
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

internal void
AddStandardRoom(game_mode_world *WorldMode, u32 AbsTileX, u32 AbsTileY, u32 AbsTileZ)
{
    for(s32 OffsetY = -4;
        OffsetY <= 4;
        OffsetY++)
    {
        for(s32 OffsetX = -8;
            OffsetX <= 8;
            OffsetX++)
        {
            world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX + OffsetX, AbsTileY + OffsetY, AbsTileZ);
            low_entity *Entity = BeginGroundedEntity(WorldMode, EntityType_Floor, P, WorldMode->FloorCollision); 
            EndEntity(WorldMode, Entity);
        }
    }
}

internal void
AddWall(game_mode_world *WorldMode, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY, AbsTileZ);
    low_entity *Entity = BeginGroundedEntity(WorldMode, EntityType_Wall, P, WorldMode->WallCollision); 
    AddFlags(&Entity->Sim, EntityFlag_Collides);
    EndEntity(WorldMode, Entity);
}

internal void
AddStair(game_mode_world *WorldMode, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY, AbsTileZ);
    low_entity *Entity = BeginGroundedEntity(WorldMode, EntityType_Stairwell, P, WorldMode->StairCollision); 

    Entity->Sim.WalkableDim = Entity->Sim.Collision->TotalVolume.Dim.xy;
    Entity->Sim.WalkableHeight = WorldMode->TypicalFloorHeight;

    AddFlags(&Entity->Sim, EntityFlag_Collides);
    EndEntity(WorldMode, Entity);
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

internal entity_id
AddPlayer(game_mode_world *WorldMode)
{
    world_position P = WorldMode->CameraP;

    low_entity *Body = BeginGroundedEntity(WorldMode, EntityType_HeroBody, P, WorldMode->HeroBodyCollision); 
    AddFlags(&Body->Sim, EntityFlag_Collides | EntityFlag_Moveable);

    low_entity *Head = BeginGroundedEntity(WorldMode, EntityType_HeroHead, P, WorldMode->HeroHeadCollision); 
    AddFlags(&Head->Sim, EntityFlag_Collides | EntityFlag_Moveable);

    InitHitPoints(Body, 3);

    Body->Sim.Head.Index = Head->Sim.StorageIndex;
    Head->Sim.Head.Index = Body->Sim.StorageIndex;

    if(WorldMode->CameraFollowingEntityIndex.Value == 0)
    {
        WorldMode->CameraFollowingEntityIndex = Body->Sim.StorageIndex;
    }

    entity_id Result = Head->Sim.StorageIndex;
    EndEntity(WorldMode, Head);
    EndEntity(WorldMode, Body);
    
    return(Result);
}

internal void
AddMonstar(game_mode_world *WorldMode, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY, AbsTileZ);
    low_entity *Entity = BeginGroundedEntity(WorldMode, EntityType_Monstar, P, WorldMode->MonstarCollision); 
    
    AddFlags(&Entity->Sim, EntityFlag_Collides | EntityFlag_Moveable);
    InitHitPoints(Entity, 3);

    EndEntity(WorldMode, Entity);
}

internal void
AddFamiliar(game_mode_world *WorldMode, uint32 AbsTileX, uint32 AbsTileY, uint32 AbsTileZ)
{
    world_position P = ChunkPositionFromTilePosition(WorldMode->World, AbsTileX, AbsTileY, AbsTileZ);
    low_entity *Entity = BeginGroundedEntity(WorldMode, EntityType_Familiar, P, WorldMode->FamiliarCollision); 
    
    AddFlags(&Entity->Sim, EntityFlag_Moveable);
    EndEntity(WorldMode, Entity);
}

internal void
DrawHitpoints(sim_entity *Entity, render_group *RenderGroup, object_transform ObjectTransform)
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

            PushRect(RenderGroup, ObjectTransform, V3(HitP, 0), HealthDim, Color);
            HitP += dHitP;
        }
    }
}

internal void
ClearCollisionRulesFor(game_mode_world *WorldMode, uint32 StorageIndex)
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
        HashBucket < ArrayCount(WorldMode->CollisionRuleHash);
        HashBucket++)
    {
        for(pairwise_collision_rule **Rule = &WorldMode->CollisionRuleHash[HashBucket];
            *Rule;
           ) 
        {
            if(((*Rule)->StorageIndexA == StorageIndex) ||
               ((*Rule)->StorageIndexB == StorageIndex))
            {
                pairwise_collision_rule *RemovedRule = *Rule;
                *Rule = (*Rule)->NextInHash;

                RemovedRule->NextInHash = WorldMode->FirstFreeCollisionRule;
                WorldMode->FirstFreeCollisionRule = RemovedRule;
            }
            else
            {
                Rule = &(*Rule)->NextInHash;
            }
        }    
    }
}

internal void
AddCollisionRule(game_mode_world *WorldMode, uint32 StorageIndexA, uint32 StorageIndexB, bool32 CanCollide)
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
    uint32 HashBucket = StorageIndexA & (ArrayCount(WorldMode->CollisionRuleHash) - 1);
    for(pairwise_collision_rule *Rule = WorldMode->CollisionRuleHash[HashBucket];
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
        Found = WorldMode->FirstFreeCollisionRule;
        if(Found)
        {
            WorldMode->FirstFreeCollisionRule = Found->NextInHash;
        } 
        else
        {
            Found = PushStruct(&WorldMode->World->Arena, pairwise_collision_rule);
        }
        
        Found->NextInHash = WorldMode->CollisionRuleHash[HashBucket];
        WorldMode->CollisionRuleHash[HashBucket] = Found;
    }

    if(Found)
    {
        Found->StorageIndexA = StorageIndexA;
        Found->StorageIndexB = StorageIndexB;
        Found->CanCollide = CanCollide;
    }
}

internal sim_entity_collision_volume_group * 
MakeSimpleGroundedCollision(game_mode_world *WorldMode, real32 DimX, real32 DimY, real32 DimZ,
                            r32 OffsetZ = 0.0f)
{
    // TODO(george): Change to using fundamental types arena, etc.
    sim_entity_collision_volume_group *Group = PushStruct(&WorldMode->World->Arena, sim_entity_collision_volume_group);
    Group->VolumeCount = 1;
    Group->Volumes = PushArray(&WorldMode->World->Arena, Group->VolumeCount, sim_entity_collision_volume);
    Group->TotalVolume.OffsetP = V3(0, 0, 0.5f*DimZ + OffsetZ);
    Group->TotalVolume.Dim = V3(DimX, DimY, DimZ);
    Group->Volumes[0] = Group->TotalVolume;

    return(Group);
}

internal sim_entity_collision_volume_group * 
MakeSimpleFloorCollision(game_mode_world *WorldMode, real32 DimX, real32 DimY, real32 DimZ)
{
    // TODO(george): Change to using fundamental types arena, etc.
    sim_entity_collision_volume_group *Group = PushStruct(&WorldMode->World->Arena, sim_entity_collision_volume_group);
    Group->VolumeCount = 0;
    Group->TraversableCount = 1;
    Group->Traversables = PushArray(&WorldMode->World->Arena, Group->TraversableCount, sim_entity_traversable_point);
    Group->Traversables[0].P = V3(0, 0, 0);
    Group->TotalVolume.OffsetP = V3(0, 0, 0);
    Group->TotalVolume.Dim = V3(DimX, DimY, DimZ);

#if 0
    Group->VolumeCount = 1;
    Group->Volumes = PushArray(&WorldMode->World->Arena, Group->VolumeCount, sim_entity_collision_volume);
    Group->TotalVolume.OffsetP = V3(0, 0, 0.5f*DimZ);
    Group->Volumes[0] = Group->TotalVolume;
#endif

    return(Group);
}

internal sim_entity_collision_volume_group * 
MakeNullCollision(game_mode_world *WorldMode)
{
    // TODO(george): Change to using fundamental types arena, etc.
    sim_entity_collision_volume_group *Group = PushStruct(&WorldMode->World->Arena, sim_entity_collision_volume_group);
    Group->VolumeCount = 0;
    Group->Volumes = 0;
    Group->TotalVolume.OffsetP = V3(0, 0, 0);
    // TODO(george): Should this be negative?
    Group->TotalVolume.Dim = V3(0, 0, 0);

    return(Group);
}    

internal b32
GetClosestTraversable(sim_region *SimRegion, v3 FromP, v3 *Result)
{
    b32 Found = false;

    // TODO(george): Make spatial queries easy for things!
    r32 ClosestDistanceSq = Square(1000.0f);
    sim_entity *TestEntity = SimRegion->Entities;
    for(u32 TestEntityIndex = 0; 
        TestEntityIndex < SimRegion->EntityCount; 
        TestEntityIndex++, TestEntity++)
    {
        sim_entity_collision_volume_group *VolGroup = TestEntity->Collision;
        for(u32 PIndex = 0;
            PIndex < VolGroup->TraversableCount;
            PIndex++)
        {
            sim_entity_traversable_point P = GetSimSpaceTraversable(TestEntity, PIndex);

            v3 HeadToPoint = P.P - FromP; 

            r32 TestDSq = LengthSq(HeadToPoint);
            if(ClosestDistanceSq > TestDSq)
            {
                ClosestDistanceSq = TestDSq;
                *Result = P.P;
                Found = true;
            }
        }
    }

    return(Found);
}

internal void
PlayWorld(game_state *GameState, transient_state *TranState)
{
    SetGameMode(GameState, TranState, GameMode_World);

    game_mode_world *WorldMode = PushStruct(&GameState->ModeArena, game_mode_world);

    uint32 TilesPerWidth = 17;
    uint32 TilesPerHeight = 9;

    WorldMode->EffectsEntropy = RandomSeed(1234);
    WorldMode->TypicalFloorHeight = 3.0f;

        // TODO(george): Remove this!
    real32 PixelsToMeters = 1.0f / 42.0f;
    v3 WorldChunkDimInMeters = {PixelsToMeters*(real32)GroundBufferWidth,
                                PixelsToMeters*(real32)GroundBufferHeight,
                                WorldMode->TypicalFloorHeight};

    WorldMode->World = CreateWorld(WorldChunkDimInMeters, &GameState->ModeArena);
    world *World = WorldMode->World;

    real32 TileSideInMeters = 1.4f;
    real32 TileDepthInMeters = 3.0f;

    WorldMode->NullCollision = MakeNullCollision(WorldMode);
    WorldMode->StairCollision = MakeSimpleGroundedCollision(WorldMode, 
                                                            TileSideInMeters,
                                                            2.0f*TileSideInMeters,
                                                            1.1f*TileDepthInMeters);
    WorldMode->HeroBodyCollision = MakeSimpleGroundedCollision(WorldMode, 1.0f, 0.5f, 0.6f);
    WorldMode->HeroHeadCollision = MakeSimpleGroundedCollision(WorldMode, 1.0f, 0.5f, 0.5f, 0.7f);
    WorldMode->MonstarCollision = MakeSimpleGroundedCollision(WorldMode, 1.0f, 0.5f, 0.5f);
    WorldMode->FamiliarCollision = MakeSimpleGroundedCollision(WorldMode, 1.0f, 0.5f, 0.5f);
    WorldMode->WallCollision = MakeSimpleGroundedCollision(WorldMode, 
                                                            TileSideInMeters,
                                                            TileSideInMeters,
                                                            TileDepthInMeters);
    WorldMode->FloorCollision = MakeSimpleFloorCollision(WorldMode, 
                                                         TileSideInMeters,
                                                         TileSideInMeters,
                                                         TileDepthInMeters);

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
    for (u32 ScreenIndex = 0; 
         ScreenIndex < 1; 
         ScreenIndex++)
    {
#if 1
        uint32 DoorDirection = RandomChoice(&Series, (DoorUp || DoorDown) ? 2 : 4);
#else
        uint32 DoorDirection = RandomChoice(&Series, 2);
#endif
        // DoorDirection = 3;   

        bool32 CreatedZDoor = false;
        if(DoorDirection == 3)
        {
            CreatedZDoor = true;
            DoorDown = true;
        }
        else if(DoorDirection == 2)
        {
            CreatedZDoor = true;
            DoorUp = true;
        }
        else if(DoorDirection == 1)
        {
            DoorRight = true;                
        }
        else
        {
            DoorTop = true;                
        }

        AddStandardRoom(WorldMode,
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
                    AddWall(WorldMode, AbsTileX, AbsTileY, AbsTileZ);
                }
                else if(CreatedZDoor)
                {
                    if(((AbsTileZ % 2) && (TileX == 10) && (TileY == 5)) ||
                        (!(AbsTileZ % 2) && (TileX == 4) && (TileY == 5)))
                    {
                        AddStair(WorldMode, AbsTileX, AbsTileY, DoorDown ? AbsTileZ - 1 : AbsTileZ);
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

        if(DoorDirection == 3)
        {
            AbsTileZ -= 1;                
        }
        else if(DoorDirection == 2)
        {
            AbsTileZ += 1;
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
    WorldMode->CameraP = NewCameraP;

    AddMonstar(WorldMode, CameraTileX + 3, CameraTileY + 2, CameraTileZ);
    AddFamiliar(WorldMode, CameraTileX - 2, CameraTileY + 2, CameraTileZ);

    GameState->WorldMode = WorldMode;
}

internal b32
UpdateAndRenderWorld(game_state *GameState, game_mode_world *WorldMode, transient_state *TranState, 
                     game_input *Input, render_group *RenderGroup, loaded_bitmap *DrawBuffer)
{
    b32 Result = false;

    world *World = WorldMode->World;

    v2 MouseP = {Input->MouseX, Input->MouseY};

    real32 WidthOfMonitor = 0.635f; // NOTE(george): Horizontal measurment of monitor in meters
    real32 MetersToPixels = (real32)DrawBuffer->Width/WidthOfMonitor;

    Clear(RenderGroup, V4(0.25f, 0.25f, 0.25f, 0.0f));

    real32 FocalLength = 0.6f;
    real32 DistanceAboveGround = 24.0f;
    Perspective(RenderGroup, DrawBuffer->Width, DrawBuffer->Height, MetersToPixels, FocalLength, DistanceAboveGround);

    v2 ScreenCenter = {0.5f*(real32)DrawBuffer->Width,
                       0.5f*(real32)DrawBuffer->Height};

    rectangle2 ScreenBounds = GetCameraRectangleAtTarget(RenderGroup);
    rectangle3 CameraBoundsInMeters = RectMinMax(V3(ScreenBounds.Min, 0.0f), 
                                                 V3(ScreenBounds.Max, 0.0f));   
    CameraBoundsInMeters.Min.z = -3.0f*WorldMode->TypicalFloorHeight;
    CameraBoundsInMeters.Max.z = 1.0f*WorldMode->TypicalFloorHeight;

    PushRectOutline(RenderGroup, DefaultFlatTransform(), V3(0, 0, 0), GetDim(ScreenBounds), V4(1.0f, 1.0f, 0.0f, 1.0f));

    real32 FadeTopEndZ = 0.75f*WorldMode->TypicalFloorHeight;
    real32 FadeTopStartZ = 0.5f*WorldMode->TypicalFloorHeight;
    real32 FadeBottomEndZ = -2.25f*WorldMode->TypicalFloorHeight;
    real32 FadeBottomStartZ = -2.0f*WorldMode->TypicalFloorHeight;

    b32 HeroesExist = false;
    b32 QuitRequested = false;
    for(u32 ControllerIndex = 0; 
        ControllerIndex < ArrayCount(Input->Controllers); 
        ControllerIndex++)
    {
        game_controller_input *Controller = GetController(Input, ControllerIndex);
        controlled_hero *ConHero = GameState->ControlledHeroes + ControllerIndex;
        if(ConHero->EntityIndex.Value == 0)
        {
            if(WasPressed(Controller->Back))
            {
                QuitRequested = true;
            }
            if(WasPressed(Controller->Start))
            {
                *ConHero = {};
                ConHero->EntityIndex = AddPlayer(WorldMode);
            }
        }
        
        if(ConHero->EntityIndex.Value)
        {
            HeroesExist = true;

            ConHero->dZ = 0.0f;

            if(Controller->IsAnalog)
            {
                // NOTE(george): Use analog movement tuning
                ConHero->ddP = v2{Controller->StickAverageX, Controller->StickAverageY};
            }
            else
            {
                // NOTE(george): Use digital movement tuning
                r32 Recenter = 0.5f;
                if(WasPressed(Controller->MoveUp))
                {
                    ConHero->ddP.x = 0.0f;
                    ConHero->ddP.y = 1.0f;
                    ConHero->RecenterTimer = Recenter;
                }
                if(WasPressed(Controller->MoveDown))
                {
                    ConHero->ddP.x = 0.0f;
                    ConHero->ddP.y = -1.0f;
                    ConHero->RecenterTimer = Recenter;
                }
                if(WasPressed(Controller->MoveLeft))
                {
                    ConHero->ddP.x = -1.0f;
                    ConHero->ddP.y = 0.0f;
                    ConHero->RecenterTimer = Recenter;
                }
                if(WasPressed(Controller->MoveRight))
                {
                    ConHero->ddP.x = 1.0f;
                    ConHero->ddP.y = 0.0f;
                    ConHero->RecenterTimer = Recenter;
                }

                if(!IsDown(Controller->MoveLeft) && 
                   !IsDown(Controller->MoveRight))
                {
                    ConHero->ddP.x = 0.0f;
                    if(IsDown(Controller->MoveUp))
                    {
                        ConHero->ddP.y = 1.0f;
                    }
                    if(IsDown(Controller->MoveDown))
                    {
                        ConHero->ddP.y = -1.0f;
                    }
                }

                if(!IsDown(Controller->MoveUp) && 
                   !IsDown(Controller->MoveDown))
                {
                    ConHero->ddP.y = 0.0f;
                    if(IsDown(Controller->MoveRight))
                    {
                        ConHero->ddP.x = 1.0f;
                    }
                    if(IsDown(Controller->MoveLeft))
                    {
                        ConHero->ddP.x = -1.0f;
                    }
                } 
            }

#if 0
            if(Controller->Start.EndedDown)
            {
                ConHero->dZ = 3.0f;
            }
#endif

            ConHero->dSword = {};
            if(Controller->ActionUp.EndedDown)
            {
                ChangeVolume(&GameState->AudioState, GameState->Music, 10.0f, V2(1.0f, 1.0f));
                ConHero->dSword = V2(0.0f, 1.0f);
            }  
            if(Controller->ActionDown.EndedDown)
            {
                ChangeVolume(&GameState->AudioState, GameState->Music, 10.0f, V2(0.0f, 0.0f));
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

            if(WasPressed(Controller->Back))
            {
                Assert(!"Mark this and delete in sim!");
                    // DeleteLowEntity(WorldMode, ConHero->EntityIndex);
                ConHero->EntityIndex.Value = 0;
            }
        }
    }

    // TODO(george): How big do we actually want to expand here?
    v3 SimBoundExpansion = V3(15.0f, 15.0f, 0.0f);
    rectangle3 SimBounds = AddRadiusTo(CameraBoundsInMeters, SimBoundExpansion);

    temporary_memory SimMemory = BeginTemporaryMemory(&TranState->TranArena);
    world_position SimCenterP = WorldMode->CameraP;
    sim_region *SimRegion = BeginSim(&TranState->TranArena, WorldMode, WorldMode->World, SimCenterP, SimBounds, Input->dtForFrame);

    v3 CameraP = Substract(World, &WorldMode->CameraP, &SimCenterP);

    // TODO(george): Move this out to handmade_entity.cpp
    {
        TIMED_BLOCK("EntityRender");

        for(u32 EntityIndex = 0; 
            EntityIndex < SimRegion->EntityCount; 
            EntityIndex++)
        {
            sim_entity *Entity = SimRegion->Entities + EntityIndex;
            Entity->XAxis = V2(1, 0);
            Entity->YAxis = V2(1, 0);

            // TODO(georgy): We don't really have a way to unique-ify these :(
            debug_id EntityDebugID = DEBUG_POINTER_ID((void *)Entity->StorageIndex.Value);
            if(DEBUG_REQUESTED(EntityDebugID))
            {
                DEBUG_BEGIN_DATA_BLOCK("Simulation/Entity");
            }
                
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

                v3 CameraRelativeGroundP = GetEntityGroundPoint(Entity) - CameraP;
                RenderGroup->GlobalAlpha = 1.0f;
                if(CameraRelativeGroundP.z > FadeTopStartZ)
                {
                    RenderGroup->GlobalAlpha = 1.0f - Clamp01MapToRange(FadeTopStartZ, CameraRelativeGroundP.z, FadeTopEndZ);
                }
                else if(CameraRelativeGroundP.z < FadeBottomStartZ)
                {
                    RenderGroup->GlobalAlpha = 1.0f - Clamp01MapToRange(FadeBottomStartZ, CameraRelativeGroundP.z, FadeBottomEndZ);
                }


                // 
                // NOTE(georgy): Pre-physics entity work
                // 
                hero_bitmap_ids HeroBitmaps = {};
                asset_vector MatchVector = {};
                MatchVector.E[Tag_FacingDirection] = Entity->FacingDirection;
                asset_vector WeightVector = {};
                WeightVector.E[Tag_FacingDirection] = 1.0f;
                HeroBitmaps.Head = GetBestMatchBitmapFrom(TranState->Assets, Asset_Head, &MatchVector, &WeightVector);
                HeroBitmaps.Torso = GetBestMatchBitmapFrom(TranState->Assets, Asset_Torso, &MatchVector, &WeightVector);
                HeroBitmaps.Legs = GetBestMatchBitmapFrom(TranState->Assets, Asset_Legs, &MatchVector, &WeightVector);
                switch(Entity->Type)
                {
                    case EntityType_HeroHead:
                    {
                        for(uint32 ControlIndex = 0;
                            ControlIndex < ArrayCount(GameState->ControlledHeroes);
                            ControlIndex++)
                        {
                            controlled_hero *ConHero = GameState->ControlledHeroes + ControlIndex;

                            if(Entity->StorageIndex.Value == ConHero->EntityIndex.Value)
                            {
                                ConHero->RecenterTimer = ClampAboveZero(ConHero->RecenterTimer - dt);

                                if(ConHero->dZ != 0.0f)
                                {
                                    Entity->dP.z = ConHero->dZ;
                                }

                                MoveSpec.UnitMaxAccelVector = true;
                                MoveSpec.Speed = 30.0f;
                                MoveSpec.Drag = 8.0f;

                                ddP = V3(ConHero->ddP, 0);

                                // TODO(george): Change to using the acceleration vector 
                                if((Entity->dP.x == 0.0f) && (Entity->dP.y == 0.0f))
                                {
                                    // NOTE(george): Leave FacingDirection whater it was
                                }
                                else
                                {
                                    Entity->FacingDirection = ATan2(ddP.y, ddP.x);
                                    if(Entity->FacingDirection < 0.0f)
                                    {
                                        Entity->FacingDirection += 2.0f*Pi32;
                                    }
                                }

                                v3 ClosestP = Entity->P;
                                if(GetClosestTraversable(SimRegion, Entity->P, &ClosestP))
                                {
                                    b32 TimerIsUp = (ConHero->RecenterTimer == 0.0f);
                                    b32 NoPush = (LengthSq(ddP) < 0.1f);
                                    r32 Cp = NoPush ? 300.0f : 25.0f;
                                    v3 ddP2 = {};
                                    for(u32 E = 0;
                                        E < 3;
                                        E++)
                                    {
                                        if(NoPush || (TimerIsUp && (Square(ddP.E[E]) < 0.01f)))
                                        // if(NoPush)
                                        {
                                            ddP2.E[E] = Cp*(ClosestP.E[E] - Entity->P.E[E]) - 30.0f*Entity->dP.E[E];
                                        }
                                    }

                                    Entity->dP += dt*ddP2;
                                }
                            }
                        }
                    } break;

                    case EntityType_HeroBody:
                    {
                        sim_entity *Head = Entity->Head.Ptr;
                        if(Head)
                        {
                            v3 ClosestP = Entity->P;
                            b32 Found = GetClosestTraversable(SimRegion, Head->P, &ClosestP);
                            v3 BodyDelta = ClosestP - Entity->P;
                            r32 BodyDistance = LengthSq(BodyDelta);

                            Entity->FacingDirection = Head->FacingDirection;
                            Entity->dP = V3(0, 0, 0);
                            r32 ddtBob = 0.0f;

                            v3 HeadDelta = Head->P - Entity->P;
                            r32 HeadDistance = Length(HeadDelta);
                            r32 MaxHeadDistance = 0.5f;
                            r32 tHeadDistance = Clamp01MapToRange(0.0f, HeadDistance, MaxHeadDistance);

                            Entity->FloorDisplace = 0.25f*HeadDelta.xy;

                            switch(Entity->MovementMode)
                            {
                                case MovementMode_Planted:
                                {
                                    if(Found && (BodyDistance > Square(0.01f)))
                                    {
                                        Entity->tMovement = 0.0f;
                                        Entity->MovementFrom = Entity->P;
                                        Entity->MovementTo = ClosestP;
                                        Entity->MovementMode = MovementMode_Hopping;
                                    }

                                    ddtBob = -20.0f*tHeadDistance;
                                } break;

                                case MovementMode_Hopping:
                                {
                                    r32 tJump = 0.1f;
                                    r32 tThrust = 0.2f;
                                    r32 tLand = 0.9f;

                                    if(Entity->tMovement < tThrust)
                                    {
                                        ddtBob = 30.0f;
                                    }

                                    if(Entity->tMovement < tLand)
                                    {
                                        r32 t = Clamp01MapToRange(tJump, Entity->tMovement, tLand);
                                        v3 a = V3(0, -2.0f, 0);
                                        v3 b = (Entity->MovementTo - Entity->MovementFrom) - a;
                                        Entity->P = a*t*t + b*t + Entity->MovementFrom;
                                    }

                                    if(Entity->tMovement >= 1.0f)
                                    {
                                        Entity->P = Entity->MovementTo;
                                        Entity->MovementMode = MovementMode_Planted;
                                        Entity->dtBob = -2.0f;
                                    }

                                    Entity->tMovement += 4.0f*dt;
                                    if(Entity->tMovement > 1.0f)
                                    {
                                        Entity->tMovement = 1.0f;
                                    }
                                } break;
                            }   

                            r32 Cp = 100.0f;
                            r32 Cv = 10.0f;
                            ddtBob += -Cp*Entity->tBob + -Cv*Entity->dtBob;
                            Entity->tBob += ddtBob*dt*dt + Entity->dtBob*dt;
                            Entity->dtBob += ddtBob*dt;

                            Entity->YAxis = V2(0, 1) + 0.5f*HeadDelta.xy;
                            // Entity->XAxis = Perp(Entity->YAxis);
                        }
                    } break;

                    case EntityType_Sword:
                    {
                        MoveSpec.UnitMaxAccelVector = false;
                        MoveSpec.Speed = 0.0f;
                        MoveSpec.Drag = 0.0f;

                        if(Entity->DistanceLimit == 0.0f)
                        {
                            ClearCollisionRulesFor(WorldMode, Entity->StorageIndex.Value);
                            MakeEntityNonSpatial(Entity);
                        }
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
                            if(TestEntity->Type == EntityType_HeroBody)
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
                    } break;
                }

                if(!IsSet(Entity, EntityFlag_Nonspatial) && 
                    IsSet(Entity, EntityFlag_Moveable))
                {
                    MoveEntity(WorldMode, SimRegion, Entity, Input->dtForFrame, &MoveSpec, ddP);
                }

                object_transform EntityTransform = DefaultUprightTransform();
                EntityTransform.OffsetP = GetEntityGroundPoint(Entity);

                // 
                // NOTE(georgy): Post-physics entity work
                // 
                real32 HeroSizeC = 1.15f;
                switch(Entity->Type)
                {
                    case EntityType_HeroBody:
                    {
                        v4 Color = V4(1, 1, 1, 1);
                        v2 XAxis = Entity->XAxis;
                        v2 YAxis = Entity->YAxis;
                        v3 Offset = V3(Entity->FloorDisplace, 0.0f);
                        PushBitmap(RenderGroup, EntityTransform, GetFirstBitmapFrom(TranState->Assets, Asset_Shadow), HeroSizeC*0.25f, V3(0, 0, 0), V4(1, 1, 1, ShadowAlpha));                  
                        PushBitmap(RenderGroup, EntityTransform, HeroBitmaps.Torso, HeroSizeC*1.2f, V3(0, Entity->tBob, -0.0001f), Color, 1.0f, XAxis, YAxis);  
                        PushBitmap(RenderGroup, EntityTransform, HeroBitmaps.Legs, HeroSizeC*1.2f, V3(0, 0, 0), Color, 1.0f, XAxis, YAxis);  
                        DrawHitpoints(Entity, RenderGroup, EntityTransform);
                    } break;

                    case EntityType_HeroHead:
                    {
                        PushBitmap(RenderGroup, EntityTransform, HeroBitmaps.Head, HeroSizeC*1.2f, V3(0, -0.1f, 0));  
                    } break;
    #if 1
                    case EntityType_Wall:
                    {
                        bitmap_id BID = GetFirstBitmapFrom(TranState->Assets, Asset_Tree);
                        if(DEBUG_REQUESTED(EntityDebugID))
                        {
                            DEBUG_NAMED_VALUE(BID);
                        }

                        PushBitmap(RenderGroup, EntityTransform, BID, 2.5f, V3(0, 0, 0));
                    } break;

                    case EntityType_Stairwell:
                    {
                        PushRect(RenderGroup, EntityTransform, V3(0, 0, 0), Entity->WalkableDim, V4(1, 0.5f, 0, 1));
                        PushRect(RenderGroup,EntityTransform,  V3(0, 0, Entity->WalkableHeight), Entity->WalkableDim, V4(1, 1, 0, 1));
                    } break;

                    case EntityType_Sword:
                    {
                        PushBitmap(RenderGroup, EntityTransform, GetFirstBitmapFrom(TranState->Assets, Asset_Shadow), 0.5f, V3(0, 0, 0), V4(1, 1, 1, ShadowAlpha));                  
                        PushBitmap(RenderGroup, EntityTransform, GetFirstBitmapFrom(TranState->Assets, Asset_Sword), 0.5f, V3(0, 0, 0));
                    } break;

                    case EntityType_Familiar:
                    {
                        bitmap_id BID = HeroBitmaps.Head;
                        if(DEBUG_REQUESTED(EntityDebugID))
                        {
                            DEBUG_NAMED_VALUE(BID);
                        }

                        Entity->tBob += dt;
                        if(Entity->tBob > 2.0f*Pi32)
                        {
                            Entity->tBob -= 2.0f*Pi32;
                        }
                        real32 BobSin = Sin(2.0f*Entity->tBob);
                        PushBitmap(RenderGroup, EntityTransform, GetFirstBitmapFrom(TranState->Assets, Asset_Shadow), 2.5f*0.25f, V3(0, 0, 0), V4(1, 1, 1, (0.5f*ShadowAlpha) + 0.2f*BobSin));
                        PushBitmap(RenderGroup, EntityTransform, BID, 2.5f, V3(0, 0, 0.5f*BobSin));  
                    } break;

                    case EntityType_Monstar:
                    {
                        PushBitmap(RenderGroup, EntityTransform, GetFirstBitmapFrom(TranState->Assets, Asset_Shadow), 4.5f*0.25f, V3(0, 0, 0), V4(1, 1, 1, ShadowAlpha));                  
                        PushBitmap(RenderGroup, EntityTransform, HeroBitmaps.Torso, 4.5f, V3(0, 0, 0));  
                        DrawHitpoints(Entity, RenderGroup, EntityTransform);            
                    } break;

                    case EntityType_Floor:
                    {
                        for(u32 VolumeIndex = 0;
                            VolumeIndex < Entity->Collision->VolumeCount;
                            VolumeIndex++)
                        {
                            sim_entity_collision_volume *Volume = Entity->Collision->Volumes + VolumeIndex;
                            PushRectOutline(RenderGroup, EntityTransform, Volume->OffsetP - V3(0, 0, 0.5f*Volume->Dim.z), Volume->Dim.xy, V4(0, 0, 1, 1));
                        }

                        for(u32 TraversableIndex = 0;
                            TraversableIndex < Entity->Collision->TraversableCount;
                            TraversableIndex++)
                        {
                            sim_entity_traversable_point *Traversable = Entity->Collision->Traversables + TraversableIndex;
                            PushRectOutline(RenderGroup, EntityTransform, Traversable->P, V2(0.1f, 0.1f), V4(1.0f, 0.5f, 0, 1));
                        }
                    } break;
    #endif
                    default:
                    {
                        // InvalidCodePath;
                    }
                }

                if(DEBUG_UI_ENABLED)
                {
                    debug_id EntityDebugID = DEBUG_POINTER_ID((void *)Entity->StorageIndex.Value);

                    for(uint32 VolumeIndex = 0;
                        VolumeIndex < Entity->Collision->VolumeCount;
                        VolumeIndex++)
                    {
                        sim_entity_collision_volume *Volume = Entity->Collision->Volumes + VolumeIndex;

                        v3 LocalMouseP = Unproject(RenderGroup, EntityTransform, MouseP);

                        if((LocalMouseP.x > -0.5f*Volume->Dim.x) && (LocalMouseP.x < 0.5f*Volume->Dim.x) && 
                        (LocalMouseP.y > -0.5f*Volume->Dim.y) && (LocalMouseP.y < 0.5f*Volume->Dim.y))
                        {
                            DEBUG_HIT(EntityDebugID, LocalMouseP.z);
                        }               

                        v4 OutlineColor;
                        if(DEBUG_HIGHLIGHTED(EntityDebugID, &OutlineColor))
                        {
                            PushRectOutline(RenderGroup, EntityTransform, Volume->OffsetP - V3(0, 0, 0.5f*Volume->Dim.z), 
                                            Volume->Dim.xy, OutlineColor, 0.05f);
                        }
                    }
                }

                if(DEBUG_REQUESTED(EntityDebugID))
                {
                    // TODO(georgy): Do we want this? DEBUG_VALUE(EntityDebugID);
                    DEBUG_VALUE(Entity->StorageIndex.Value);               
                    DEBUG_VALUE(Entity->Updatable);               
                    DEBUG_VALUE(Entity->Type);               
                    DEBUG_VALUE(Entity->P);
                    DEBUG_VALUE(Entity->dP);
                    DEBUG_VALUE(Entity->DistanceLimit);
                    DEBUG_VALUE(Entity->FacingDirection);
                    DEBUG_VALUE(Entity->tBob);
                    DEBUG_VALUE(Entity->dAbsTileZ);
                    DEBUG_VALUE(Entity->HitPointMax);

    #if 0
                    DEBUG_BEGIN_ARRAY(Entity->HitPoint);
                    for(uint32 HitPointIndex = 0;
                        HitPointIndex < Entity->HitPointMax;
                        HitPointIndex++)
                    {
                        DEBUG_VALUE(Entity->HitPoint[HitPointIndex]);
                    }
                    DEBUG_END_ARRAY();
                    DEBUG_VALUE(Entity->Sword);
    #endif
                    DEBUG_VALUE(Entity->WalkableDim);
                    DEBUG_VALUE(Entity->WalkableHeight);
                    
                    DEBUG_END_DATA_BLOCK();
                }
            }
        }
    }

    RenderGroup->GlobalAlpha = 1.0f;

#if 0
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
                v4 Color = CheckerOn ? V4(MapColor[MapIndex], 1.0f) : V4(0, 0, 0, 1);
                v2 MinP = V2i(X, Y);
                v2 MaxP = MinP + V2i(CheckerWidth, CheckerHeight);
                DrawRectangle(LOD, MinP, MaxP, Color);
                CheckerOn = !CheckerOn;
            }

            RowCheckerOn = !RowCheckerOn;
        }
    }
    TranState->EnvMaps[0].Pz = -1.5f;
    TranState->EnvMaps[1].Pz = 0.0f;
    TranState->EnvMaps[2].Pz = 1.5f;

    WorldMode->Time += Input->dtForFrame;
    real32 Angle = 0.1f*WorldMode->Time;
    v2 Origin = ScreenCenter;

#if 1
    v2 Disp = {100.0f*Cos(5.0f*Angle),
               100.0f*Sin(3.0f*Angle)};
#else
    v2 Disp = {};
#endif

#if 1
    v2 XAxis = 100.0f*V2(Cos(5.0f*Angle), Sin(5.0f*Angle));
    v2 YAxis = Perp(XAxis);
#else
    v2 XAxis = {60.0f, 0};
    v2 YAxis = {0, 60.0f};
#endif

    CoordinateSystem(RenderGroup, Origin - 0.5f*XAxis - 0.5f*YAxis + Disp, XAxis, YAxis, V4(1, 1, 1, 1), &WorldMode->TestDiffuse, 
                     &WorldMode->TestNormal, 
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
#endif

#if 0
    Saturation(RenderGroup, 0.0f);
#endif

    Orthographic(RenderGroup, DrawBuffer->Width, DrawBuffer->Height, 1.0f);

    PushRectOutline(RenderGroup, DefaultFlatTransform(), V3(MouseP, 0.0f), V2(2.0f, 2.0f));  

    EndSim(SimRegion, WorldMode);    
    EndTemporaryMemory(SimMemory);

    if(!HeroesExist)
    {
        PlayTitleScreen(GameState, TranState);
    }

    return(Result);
}

#if 0
if(Global_Particles_Test)
{
    for(uint32 ParticleSpawnIndex = 0;
        ParticleSpawnIndex < 2;
        ParticleSpawnIndex++)
    {
        particle *Particle = WorldMode->Particles + WorldMode->NextParticle++;
        if(WorldMode->NextParticle >= ArrayCount(WorldMode->Particles))
        {
            WorldMode->NextParticle = 0;
        }

        Particle->P = V3(RandomBetween(&WorldMode->EffectsEntropy, -0.05f, 0.05f), 0.0f, 0.0f);
        Particle->dP = V3(RandomBetween(&WorldMode->EffectsEntropy, -0.01f, 0.01f), 7.0f*RandomBetween(&WorldMode->EffectsEntropy, 0.7f, 1.0f), 0.0f);
        Particle->ddP = V3(0.0f, -9.8f, 0.0f);
        Particle->Color = V4(RandomBetween(&WorldMode->EffectsEntropy, 0.75f, 1.0f), 
                            RandomBetween(&WorldMode->EffectsEntropy, 0.75f, 1.0f), 
                            RandomBetween(&WorldMode->EffectsEntropy, 0.75f, 1.0f), 
                            1.0f);
        Particle->dColor = V4(0.0f, 0.0f, 0.0f, -0.2f);

        char Nothings[] = "NOTHINGS";
        asset_vector MatchVector = {};
        asset_vector WeightVector = {};
        MatchVector.E[Tag_UnicodeCodepoint] = (real32)Nothings[RandomChoice(&WorldMode->EffectsEntropy, ArrayCount(Nothings) - 1)];
        WeightVector.E[Tag_UnicodeCodepoint] = 1.0f;
        Particle->BitmapID = HeroBitmaps.Head; // GetBestMatchBitmapFrom(TranState->Assets, Asset_Font, &MatchVector, &WeightVector);
        // Particle->BitmapID = GetRandomBitmapFrom(TranState->Assets, Asset_Font, &WorldMode->EffectsEntropy);
    }

    // NOTE(georgy): Particle system test
    ZeroStruct(WorldMode->ParticleCels);

    real32 GridScale = 0.2f;
    real32 InvGridScale = 1.0f / GridScale;
    v3 GridOrigin = {-0.5f*GridScale*PARTICLE_CEL_DIM, 0.0f, 0.0f};
    for(uint32 ParticleIndex = 0;
        ParticleIndex < ArrayCount(WorldMode->Particles);
        ParticleIndex++)
    {
        particle *Particle = WorldMode->Particles + ParticleIndex;

        v3 P = InvGridScale*(Particle->P - GridOrigin);

        int32 X = TruncateReal32ToInt32(P.x);
        int32 Y = TruncateReal32ToInt32(P.y);

        if(X < 0) { X = 0; }
        if(X > (PARTICLE_CEL_DIM - 1)) { X = (PARTICLE_CEL_DIM - 1); }
        if(Y < 0) { Y = 0; }
        if(Y > (PARTICLE_CEL_DIM - 1)) { Y = (PARTICLE_CEL_DIM - 1); }

        particle_cel *Cel = &WorldMode->ParticleCels[Y][X];
        real32 Density = Particle->Color.a;
        Cel->Density += Density;
        Cel->VelocityTimesDensity += Density*Particle->dP;
    }

#if 0
    for(uint32 Y = 0;
        Y < PARTICLE_CEL_DIM;
        Y++)
    {
        for(uint32 X = 0;
            X < PARTICLE_CEL_DIM;
            X++)
        {
            particle_cel *Cel = &WorldMode->ParticleCels[Y][X];
            real32 Alpha = Clamp01(0.1f*Cel->Density);
            PushRect(RenderGroup, EntityTransform, GridScale*V3((real32)X, (real32)Y, 0) + GridOrigin, GridScale*V2(1.0f, 1.0f), V4(Alpha, Alpha, Alpha, 1.0f));
        }
    }
#endif

    for(uint32 ParticleIndex = 0;
        ParticleIndex < ArrayCount(WorldMode->Particles);
        ParticleIndex++)
    {
        particle *Particle = WorldMode->Particles + ParticleIndex;

        v3 P = InvGridScale*(Particle->P - GridOrigin);

        int32 X = TruncateReal32ToInt32(P.x);
        int32 Y = TruncateReal32ToInt32(P.y);

        if(X < 1) { X = 1; }
        if(X > (PARTICLE_CEL_DIM - 2)) { X = (PARTICLE_CEL_DIM - 2); }
        if(Y < 1) { Y = 1; }
        if(Y > (PARTICLE_CEL_DIM - 2)) { Y = (PARTICLE_CEL_DIM - 2); }

        particle_cel *CelCenter = &WorldMode->ParticleCels[Y][X];
        particle_cel *CelLeft = &WorldMode->ParticleCels[Y][X - 1];
        particle_cel *CelRight = &WorldMode->ParticleCels[Y][X + 1];
        particle_cel *CelUp = &WorldMode->ParticleCels[Y + 1][X];
        particle_cel *CelDown = &WorldMode->ParticleCels[Y - 1][X];

        v3 Dispersion = {};
        real32 Dc = 1.0f;
        Dispersion += Dc*(CelCenter->Density - CelLeft->Density)*V3(-1.0f, 0.0f, 0.0f);
        Dispersion += Dc*(CelCenter->Density - CelRight->Density)*V3(1.0f, 0.0f, 0.0f);
        Dispersion += Dc*(CelCenter->Density - CelDown->Density)*V3(0.0f, -1.0f, 0.0f);
        Dispersion += Dc*(CelCenter->Density - CelUp->Density)*V3(0.0f, 1.0f, 0.0f);

        v3 ddP = Particle->ddP + Dispersion;

        // NOTE(georgy): Simulate the particle forward in time
        Particle->P += 0.5f*Square(Input->dtForFrame)*ddP + Particle->dP*Input->dtForFrame;
        Particle->dP += Input->dtForFrame*ddP;
        Particle->Color += Particle->dColor*Input->dtForFrame;

        // TODO(georgy): Shouldn't we just clamp colors in the renderer?
        v4 Color;
        Color.r = Clamp01(Particle->Color.r);
        Color.g = Clamp01(Particle->Color.g);
        Color.b = Clamp01(Particle->Color.b);
        Color.a = Clamp01(Particle->Color.a);

        if(Particle->P.y < 0.0f)
        {
            real32 CoefficientOfRestitution = 0.3f;
            real32 CoefficientOfFriction = 0.7f;
            Particle->P.y =  -Particle->P.y;
            Particle->dP.y = -CoefficientOfRestitution*Particle->dP.y;
            Particle->dP.y = CoefficientOfFriction*Particle->dP.x;
        } 

        if(Color.a > 0.9f)
        {
            Color.a = 0.9f*Clamp01MapToRange(1.0f, Color.a, 0.9f);
        }

        // NOTE(georgy): Render the particle
        PushBitmap(RenderGroup, EntityTransform, Particle->BitmapID, 1.0f, Particle->P, Color);//Particle->Color);
    }
}
#endif