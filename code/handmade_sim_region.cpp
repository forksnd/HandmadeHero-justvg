inline sim_entity_traversable_point
GetSimSpaceTraversable(sim_entity *Entity, u32 Index)
{
    Assert(Index < Entity->Collision->TraversableCount);

    sim_entity_traversable_point Result = Entity->Collision->Traversables[Index];
    // TODO(georgy): This wants to be rotated eventually!
    Result.P += Entity->P;

    return(Result);
}

internal sim_entity_hash *
GetHashFromStorageIndex(sim_region *SimRegion, uint32 StorageIndex)
{
	Assert(StorageIndex);

	sim_entity_hash *Result = 0;

	uint32 HashValue = StorageIndex;
	for(uint32 Offset = 0;
		Offset < ArrayCount(SimRegion->Hash);
		Offset++)
	{
        uint32 HashMask = (ArrayCount(SimRegion->Hash) - 1);
        uint32 HashIndex = ((HashValue + Offset) & HashMask);
		sim_entity_hash *Entry = SimRegion->Hash + HashIndex;
		if((Entry->Index == 0) || (Entry->Index == StorageIndex))
		{
			Result = Entry;
			break;
		}
	}

	return(Result);
}

inline sim_entity *
GetEntityByStorageIndex(sim_region *SimRegion, uint32 StorageIndex)
{
	sim_entity_hash *Entry = GetHashFromStorageIndex(SimRegion, StorageIndex);
	sim_entity *Result = Entry->Ptr;
	return(Result);
}

inline v3
GetSimSpaceP(sim_region *SimRegion, low_entity *Stored)
{
    // NOTE(george): Map the entity into camera space
    // TODO(george): Do we want to set this to signaling NAN in
    // debug mode to make sure nobody ever uses the position
    // of a nonspatial entity?
    v3 Result = InvalidP;
    if(!IsSet(&Stored->Sim, EntityFlag_Nonspatial))
    {
        Result = Substract(SimRegion->World, &Stored->P, &SimRegion->Origin);
    }

    return(Result);
}

internal sim_entity *
AddEntity(game_mode_world *WorldMode, sim_region *SimRegion, uint32 StorageIndex, low_entity *Source, v3 *SimP);
inline void
LoadEntityReference(game_mode_world *WorldMode, sim_region *SimRegion, entity_reference *Ref)
{
	if(Ref->Index)
	{
		sim_entity_hash *Entry = GetHashFromStorageIndex(SimRegion, Ref->Index);
		if(Entry->Ptr == 0)
		{
            Entry->Index = Ref->Index;
            low_entity *Entity = GetLowEntity(WorldMode, Ref->Index);
            v3 P = GetSimSpaceP(SimRegion, Entity);
			Entry->Ptr = AddEntity(WorldMode, SimRegion, Ref->Index, Entity, &P);
		}

		Ref->Ptr = Entry->Ptr;			
	}
}

inline void
StoreEntityReference(entity_reference *Ref)
{
	if(Ref->Ptr != 0)
	{
		Ref->Index = Ref->Ptr->StorageIndex;
	}
}

internal sim_entity *
AddEntityRaw(game_mode_world *WorldMode, sim_region *SimRegion, uint32 StorageIndex, low_entity *Source)
{
	Assert(StorageIndex);
	sim_entity *Entity = 0;

    sim_entity_hash *Entry = GetHashFromStorageIndex(SimRegion, StorageIndex);
    if(Entry->Ptr == 0)
    {
        if(SimRegion->EntityCount < SimRegion->MaxEntityCount)
        {
            Entity = SimRegion->Entities + SimRegion->EntityCount++;

            Assert((Entry->Index == 0) || (Entry->Index == StorageIndex));
            Entry->Index = StorageIndex;
            Entry->Ptr = Entity;

            if(Source)
            {
                // TODO(george): This Can really be a decompression step, not
                // a copy!
                *Entity = Source->Sim;
                LoadEntityReference(WorldMode, SimRegion, &Entity->Sword);
                LoadEntityReference(WorldMode, SimRegion, &Entity->Head);

                Assert(!IsSet(&Source->Sim, EntityFlag_Simming));
                AddFlags(&Source->Sim, EntityFlag_Simming);
            }

            Entity->StorageIndex = StorageIndex;
            Entity->Updatable = false;
        }
        else
        {
            InvalidCodePath;
        }
    }
	return(Entity);
}

inline bool32
EntityOverlapsRectangle(v3 P, sim_entity_collision_volume Volume, rectangle3 Rect)
{
    rectangle3 Grown = AddRadiusTo(Rect, 0.5f*Volume.Dim);
    bool32 Result = IsInRectangle(Grown, P + Volume.OffsetP);

    return(Result);
}

internal sim_entity *
AddEntity(game_mode_world *WorldMode, sim_region *SimRegion, uint32 StorageIndex, low_entity *Source, v3 *SimP)
{
	sim_entity *Dest = AddEntityRaw(WorldMode, SimRegion, StorageIndex, Source);
	if(Dest)
	{
		if(SimP)
		{
			Dest->P = *SimP;
            Dest->Updatable = EntityOverlapsRectangle(Dest->P, Dest->Collision->TotalVolume, SimRegion->UpdatableBounds);
		}
		else
		{
			Dest->P = GetSimSpaceP(SimRegion, Source);
		}
	}

    return(Dest);
}

internal sim_region *
BeginSim(memory_arena *SimArena, game_mode_world *WorldMode, world *World, world_position Origin, rectangle3 Bounds, real32 dt)
{
	// TODO(george): If entities were stored in the world, we wouldn't need the game state here!


	sim_region *SimRegion = PushStruct(SimArena, sim_region);

    // TODO(george):Try to make these get enforced more rigorously
    SimRegion->MaxEntityRadius = 5.0f;
    SimRegion->MaxEntityVelocity = 30.0f;
    real32 UpdateSafetyMargin = SimRegion->MaxEntityRadius + dt*SimRegion->MaxEntityVelocity + 1.0f;
    real32 UpdateSafetyMarginZ = 1.0f;

	SimRegion->World = World;
	SimRegion->Origin = Origin;
    SimRegion->UpdatableBounds = AddRadiusTo(Bounds, V3(SimRegion->MaxEntityRadius, SimRegion->MaxEntityRadius, SimRegion->MaxEntityRadius));
	SimRegion->Bounds = AddRadiusTo(SimRegion->UpdatableBounds, V3(UpdateSafetyMargin, UpdateSafetyMargin, UpdateSafetyMarginZ));

	// TODO(george): Need to be more specific about entity counts
	SimRegion->MaxEntityCount = 4096;
	SimRegion->EntityCount = 0;
	SimRegion->Entities = PushArray(SimArena, SimRegion->MaxEntityCount, sim_entity);

	world_position MinChunkP = MapIntoChunkSpace(World, SimRegion->Origin, GetMinCorner(SimRegion->Bounds));
    world_position MaxChunkP = MapIntoChunkSpace(World, SimRegion->Origin, GetMaxCorner(SimRegion->Bounds));

    for(int32 ChunkZ = MinChunkP.ChunkZ; ChunkZ <= MaxChunkP.ChunkZ; ChunkZ++) 
    {
        for(int32 ChunkY = MinChunkP.ChunkY; ChunkY <= MaxChunkP.ChunkY; ChunkY++)
        {
            for(int32 ChunkX = MinChunkP.ChunkX; ChunkX <= MaxChunkP.ChunkX; ChunkX++)
            {
                world_chunk *Chunk = GetWorldChunk(World, ChunkX, ChunkY, ChunkZ);
                if(Chunk)
                {
                    for(world_entity_block *Block = &Chunk->FirstBlock; Block; Block = Block->Next)
                    {
                        for(uint32 EntityIndexInBlock = 0; EntityIndexInBlock < Block->EntityCount; EntityIndexInBlock++)
                        {
                            uint32 LowEntityIndex = Block->LowEntityIndex[EntityIndexInBlock];
                            low_entity *Low = WorldMode->LowEntities + LowEntityIndex;
                            v3 SimSpaceP = GetSimSpaceP(SimRegion, Low);
                            if(!IsSet(&Low->Sim, EntityFlag_Nonspatial))
                            {
                                if(EntityOverlapsRectangle(SimSpaceP, Low->Sim.Collision->TotalVolume, SimRegion->Bounds))
                                {
                                    AddEntity(WorldMode, SimRegion, LowEntityIndex, Low, &SimSpaceP);
                                }
                            }
                        }        
                    }
                }
            }
    }
    }    

    return(SimRegion);
}

internal void
EndSim(sim_region *Region, game_mode_world *WorldMode)
{
    world *World = WorldMode->World;

	sim_entity *Entity = Region->Entities;
	for(uint32 EntityIndex = 0;
	 	EntityIndex < Region->EntityCount;
	  	Entity++, EntityIndex++)
	{
		low_entity *Stored = WorldMode->LowEntities + Entity->StorageIndex;

        Assert(IsSet(&Stored->Sim, EntityFlag_Simming));
		Stored->Sim = *Entity;
        Assert(!IsSet(&Stored->Sim, EntityFlag_Simming));

		StoreEntityReference(&Stored->Sim.Sword);
		StoreEntityReference(&Stored->Sim.Head);

		// TODO(george): Save state back to the stored entity, once high entities
		// do state decompression, etc.

		world_position NewP = IsSet(Entity, EntityFlag_Nonspatial) ?  
                                NullPosition() : MapIntoChunkSpace(World, Region->Origin, Entity->P);
		ChangeEntityLocation(&World->Arena, World, Entity->StorageIndex, Stored, NewP);

		if(Entity->StorageIndex == WorldMode->CameraFollowingEntityIndex)
		{
			world_position NewCameraP = WorldMode->CameraP;

            NewCameraP.ChunkZ = Stored->P.ChunkZ;

            if(Global_Renderer_Camera_UseRoomBasedCamera)
            {
                if(Entity->P.x > (9.0f))
                {
                    NewCameraP = MapIntoChunkSpace(World, NewCameraP, V3(18.0f, 0.0f, 0.0f));
                }
                else if(Entity->P.x < -(9.0f))
                {
                    NewCameraP = MapIntoChunkSpace(World, NewCameraP, V3(-18.0f, 0.0f, 0.0f));
                }

                if(Entity->P.y > (5.0f))
                {
                    NewCameraP = MapIntoChunkSpace(World, NewCameraP, V3(10.0f, 0.0f, 0.0f));
                }
                else if(Entity->P.y < -(5.0f))
                {
                    NewCameraP = MapIntoChunkSpace(World, NewCameraP, V3(-10.0f, 0.0f, 0.0f));
                }
            }
            else
            {
                // real32 CamOffsetZ = NewCameraP.Offset_.z;
                NewCameraP = Stored->P;
                // NewCameraP.Offset_.z = CamOffsetZ;
            }

            WorldMode->CameraP = NewCameraP;
		}
	}
}

struct test_wall
{
    real32 X;
    real32 RelX;
    real32 RelY;
    real32 DeltaX;
    real32 DeltaY;
    real32 MinY;
    real32 MaxY;
    v3 Normal;
};
internal bool32
TestWall(real32 WallX, real32 RelX, real32 RelY, real32 PlayerDeltaX, real32 PlayerDeltaY, 
         real32 *tMin, real32 MinY, real32 MaxY)
{
    bool32 Hit = false;

    real32 tEpsilon = 0.0001f;
    if(PlayerDeltaX != 0.0f)
    {
        real32 tResult = (WallX - RelX) / PlayerDeltaX;
        real32 Y = RelY + tResult*PlayerDeltaY;
        if((tResult >= 0.0f) && (*tMin > tResult))
        {
            if((Y >= MinY) && (Y <= MaxY))
            {
                *tMin = Maximum(0.0f, tResult - tEpsilon);
                Hit = true;
            }
        }
    }

    return(Hit);
}

internal bool32
CanCollide(game_mode_world *WorldMode, sim_entity *A, sim_entity *B)
{
    bool32 Result = false;

    if (A != B)
    {
        if(A->StorageIndex > B->StorageIndex)
        {
            sim_entity *Temp = A;
            A = B;
            B = Temp;
        }

        if(IsSet(A, EntityFlag_Collides) && IsSet(B, EntityFlag_Collides))
        {
            if(!IsSet(A, EntityFlag_Nonspatial) && 
            !IsSet(B, EntityFlag_Nonspatial))
            {
                // TODO(george): Property-based logic goes here
                Result = true;
            }

            uint32 HashBucket = A->StorageIndex & (ArrayCount(WorldMode->CollisionRuleHash) - 1);
            for(pairwise_collision_rule *Rule = WorldMode->CollisionRuleHash[HashBucket];
                Rule;
                Rule = Rule->NextInHash)
            {
                if((Rule->StorageIndexA == A->StorageIndex) &&
                (Rule->StorageIndexB == B->StorageIndex))
                {
                    Result = Rule->CanCollide;
                    break;
                }
            }
        }
    }

    return(Result);
}

internal bool32 
HandleCollision(game_mode_world *WorldMode, sim_entity *A, sim_entity *B)
{
    bool32 StopsOnCollision = false;

    if(A->Type == EntityType_Sword)
    {
        AddCollisionRule(WorldMode, A->StorageIndex, B->StorageIndex, false);
        StopsOnCollision = false;
    }
    else
    {
        StopsOnCollision = true;
    }

    if(A->Type > B->Type)
    {
        sim_entity *Temp = A;
        A = B;
        B = Temp;
    }

    if((A->Type == EntityType_Monstar) &&
       (B->Type == EntityType_Sword))
    {
        if(A->HitPointMax > 0)
        {
            --A->HitPointMax;
        }
    }

    // TODO(george): Stairs
    // Entity->AbsTileZ += HitLow->Sim.dAbsTileZ;   

    return(StopsOnCollision);
}

internal bool32
CanOverlap(game_mode_world *WorldMode, sim_entity *Mover, sim_entity *Region)
{
    bool32 Result = false;

    if(Mover != Region)
    {
        if(Region->Type == EntityType_Stairwell)
        {
            Result = true;
        }
    }

    return(Result);
}

internal bool32
SpeculativeCollide(sim_entity *Mover, sim_entity *Region, v3 TestP)
{
    bool32 Result = true;

    if(Region->Type == EntityType_Stairwell)
    {
        real32 StepHeight = 0.1f;
#if 0
        Result = (AbsoluteValue(MoverGroundPoint.z - Ground) > StepHeight) || 
				 (Bary.y > 0.1f) && (Bary.y < 0.9f);
#endif 
        v3 MoverGroundPoint = GetEntityGroundPoint(Mover, TestP);
        real32 Ground = GetStairGround(Region, MoverGroundPoint);
        Result = (AbsoluteValue(MoverGroundPoint.z - Ground) > StepHeight);
    }

    return(Result);
}

internal bool32
EntitiesOverlap(sim_entity *Entity, sim_entity *TestEntity, v3 Epsilon = V3(0, 0, 0))
{
    bool32 Result = false;

    for(uint32 VolumeIndex = 0; 
        !Result && VolumeIndex < Entity->Collision->VolumeCount; 
        VolumeIndex++)
    {
        sim_entity_collision_volume *Volume = Entity->Collision->Volumes + VolumeIndex;
        for(uint32 TestVolumeIndex = 0;
            !Result && TestVolumeIndex < TestEntity->Collision->VolumeCount; 
            TestVolumeIndex++)
        {
            sim_entity_collision_volume *TestVolume = TestEntity->Collision->Volumes + TestVolumeIndex;

            rectangle3 EntityRect = RectCenterDim(Entity->P + Volume->OffsetP, Volume->Dim + Epsilon);
            rectangle3 TestEntityRect = RectCenterDim(TestEntity->P + TestVolume->OffsetP, TestVolume->Dim);
            Result = RectangleIntersect(EntityRect, TestEntityRect);
        }
    }

    return(Result);
}

internal void
MoveEntity(game_mode_world *WorldMode, sim_region *SimRegion, sim_entity *Entity, real32 dt, move_spec *MoveSpec, v3 ddP) 
{
    Assert(!IsSet(Entity, EntityFlag_Nonspatial));

    world *World = SimRegion->World;

    if(MoveSpec->UnitMaxAccelVector)
    {
        real32 ddPLength = LengthSq(ddP);
        if(ddPLength > 1.0f)
        {
            ddP *= 1.0f / SquareRoot(ddPLength);
        }
    }

    ddP *= MoveSpec->Speed;

    // TODO(george): ODE here!
    v3 Drag = -MoveSpec->Drag*Entity->dP;
    Drag.z = 0.0f;
    ddP += Drag;

    v3 PlayerDelta = (0.5f*ddP*Square(dt)) + Entity->dP*dt;
    Entity->dP = ddP*dt + Entity->dP;
    // TODO(george): Upgrade physical motion routines to handle capping the
    // maximum velocity?
    Assert(LengthSq(Entity->dP) <= Square(SimRegion->MaxEntityVelocity));

    real32 DistanceRemaining = Entity->DistanceLimit;    
    if(DistanceRemaining == 0)
    {
        // TODO(george): Do we want to formalize this number?
        DistanceRemaining = 10000.0f;
    }

    for(uint32 Iteration = 0; 
        Iteration < 4; 
        Iteration++)
    {
        real32 tMin = 1.0f;
        real32 tMax = 1.0f;

        real32 PlayerDeltaLength = Length(PlayerDelta);
        // TODO(george): What do we want to do for epsilons here?
        if(PlayerDeltaLength > 0.0f)
        {
            if(PlayerDeltaLength > DistanceRemaining)
            {
                tMin = DistanceRemaining / PlayerDeltaLength;
            }

            v3 WallNormalMin = {};
            v3 WallNormalMax = {};
            sim_entity *HitEntityMin = 0;
            sim_entity *HitEntityMax = 0;

            v3 DesiredPosition = Entity->P + PlayerDelta;

            // NOTE(george): This is just an optimization to avoid enterring the
            // loop in the case where the test entity is non-spatial!
            if(!IsSet(Entity, EntityFlag_Nonspatial))
            {
                // TODO(george): Spatial partion here!
                for(uint32 TestHighEntityIndex = 0; TestHighEntityIndex < SimRegion->EntityCount; TestHighEntityIndex++)
                {
                    sim_entity *TestEntity = SimRegion->Entities + TestHighEntityIndex;

                    real32 OverlapEpsilon = 0.001f;
                    if(CanCollide(WorldMode, Entity, TestEntity))
                    {
                        for(uint32 VolumeIndex = 0; 
                            VolumeIndex < Entity->Collision->VolumeCount; 
                            VolumeIndex++)
                        {
                            sim_entity_collision_volume *Volume = Entity->Collision->Volumes + VolumeIndex;
                            for(uint32 TestVolumeIndex = 0;
                                TestVolumeIndex < TestEntity->Collision->VolumeCount; 
                                TestVolumeIndex++)
                            {
                                sim_entity_collision_volume *TestVolume = TestEntity->Collision->Volumes + TestVolumeIndex;
                                
                                v3 MinkowskiDiameter = {TestVolume->Dim.x + Volume->Dim.x, 
                                                        TestVolume->Dim.y + Volume->Dim.y,  
                                                        TestVolume->Dim.z + Volume->Dim.z};

                                v3 MinCorner = -0.5f*MinkowskiDiameter;
                                v3 MaxCorner = 0.5f*MinkowskiDiameter;

                                v3 Rel = (Entity->P + Volume->OffsetP) - (TestEntity->P + TestVolume->OffsetP);

                                if((Rel.z >= MinCorner.z) && (Rel.z < MaxCorner.z))
                                {
                                    test_wall Walls[] = 
                                    {
                                        {MinCorner.x, Rel.x, Rel.y, PlayerDelta.x, PlayerDelta.y, MinCorner.y, MaxCorner.y, V3(-1, 0, 0)},
                                        {MaxCorner.x, Rel.x, Rel.y, PlayerDelta.x, PlayerDelta.y, MinCorner.y, MaxCorner.y, V3(1, 0, 0)},
                                        {MinCorner.y, Rel.y, Rel.x, PlayerDelta.y, PlayerDelta.x, MinCorner.x, MaxCorner.x, V3(0, -1, 0)},
                                        {MaxCorner.y, Rel.y, Rel.x, PlayerDelta.y, PlayerDelta.x, MinCorner.x, MaxCorner.x, V3(0, 1, 0)}
                                    };

                                    real32 tMinTest = tMin;
                                    bool32 HitThis = false;
                                    v3 TestWallNormal = {}; 

                                    for(uint32 WallIndex = 0;
                                        WallIndex < ArrayCount(Walls);
                                        WallIndex++)
                                    {
                                        test_wall *Wall = Walls + WallIndex;

                                        real32 tEpsilon = 0.0001f;
                                        if(Wall->DeltaX != 0.0f)
                                        {
                                            real32 tResult = (Wall->X - Wall->RelX) / Wall->DeltaX;
                                            real32 Y = Wall->RelY + tResult*Wall->DeltaY;
                                            if((tResult >= 0.0f) && (tMinTest > tResult))
                                            {
                                                if((Y >= Wall->MinY) && (Y <= Wall->MaxY))
                                                {
                                                    tMinTest = Maximum(0.0f, tResult - tEpsilon);
                                                    TestWallNormal = Wall->Normal;
                                                    HitThis = true;
                                                }
                                            }
                                        }
                                    }

                                    if(HitThis)
                                    {
                                        v3 TestP = Entity->P + tMinTest*PlayerDelta;
                                        if(SpeculativeCollide(Entity, TestEntity, TestP))
                                        {
                                            tMin = tMinTest;
                                            WallNormalMin = TestWallNormal;
                                            HitEntityMin = TestEntity;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            v3 WallNormal;
            sim_entity *HitEntity;
            real32 tStop;
            if(tMin < tMax)
            {
                tStop = tMin;
                HitEntity = HitEntityMin;
                WallNormal = WallNormalMin;
            }
            else
            {
                tStop = tMax;
                HitEntity = HitEntityMax;
                WallNormal = WallNormalMax;
            }

            Entity->P += tStop*PlayerDelta; 
            DistanceRemaining -= tStop*PlayerDeltaLength;
            if(HitEntity)
            {
                PlayerDelta = DesiredPosition - Entity->P;

                bool32 StopOnCollision = HandleCollision(WorldMode, Entity, HitEntity);
                if(StopOnCollision)
                {
                    Entity->dP = Entity->dP - 1*Inner(Entity->dP, WallNormal)*WallNormal;    
                    PlayerDelta = PlayerDelta - 1*Inner(PlayerDelta, WallNormal)*WallNormal;
                }
            }
            else
            {
                break;
            }
        } 
        else
        {
            break;
        }      
    }

    if(Entity->DistanceLimit != 0)
    {
        Entity->DistanceLimit = DistanceRemaining;
    }

#if 0
    // TODO(george): Change to using the acceleration vector 
    if((Entity->dP.x == 0.0f) && (Entity->dP.y == 0.0f))
    {
        // NOTE(george): Leave FacingDirection whater it was
    }
    else
    {
        Entity->FacingDirection = ATan2(Entity->dP.y, Entity->dP.x);
        if(Entity->FacingDirection < 0.0f)
        {
            Entity->FacingDirection += 2.0f*Pi32;
        }
    }
#endif
}