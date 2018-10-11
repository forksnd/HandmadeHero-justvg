internal sim_entity *
AddEntity(sim_region *SimRegion)
{
	sim_entity *Entity = 0;

	if(SimRegion->EntityCount < SimRegion->MaxEntityCount)
	{
		Entity = &SimRegion->Entities[SimRegion->EntityCount++];
		
		// TODO(george): See what we want to do about clearing policy when
		// the entity system is more fleshed out.
		Entity = {};
	}
	else
	{
		InvalidCodePath;
	}

	return(Entity);
}

inline v2
GetSimSpaceP(sim_region *SimRegion, low_entity *Stored)
{
    world_difference Diff = Substract(SimRegion->World, &Stored->P, &SimRegion->Origin);
    v2 Result = Diff.dXY;

    return(Result);
}

internal sim_entity *
AddEntity(sim_region *SimRegion, low_entity *Source, v2 *SimP)
{
	sim_entity *Dest = AddEntity(SimRegion);
	if(Dest)
	{
		// TODO(george): Convert the stored entity to a simulation entity
		if(SimP)
		{
			Dest->P = *SimP;
		}
		else
		{
			Dest->P = GetSimSpaceP(SimRegion, Source);
		}
	}
}

internal sim_region *
BeginSim(memory_arena *SimArena, game_state *GameState, world *World, world_position Origin, rectangle2 Bounds)
{
	// TODO(george): If entities were stored in the world, we wouldn't need the game state here! 

	sim_region *SimRegion = PushStruct(SimArena, sim_region);

	SimRegion->World = World;
	SimRegion->Origin = Origin;
	SimRegion->Bounds = Bounds;

	// TODO(george): Need to be more specific about entity counts
	SimRegion->MaxEntityCount = 4096;
	SimRegion->EntityCount = 0;
	SimRegion->Entities = PushArray(SimArena, SimRegion->MaxEntityCount, sim_entity);

	world_position MinChunkP = MapIntoChunkSpace(World, SimRegion->Origin, GetMinCorner(SimRegion->Bounds));
    world_position MaxChunkP = MapIntoChunkSpace(World, SimRegion->Origin, GetMaxCorner(SimRegion->Bounds));
    for(int32 ChunkY = MinChunkP.ChunkY; ChunkY <= MaxChunkP.ChunkY; ChunkY++)
    {
        for(int32 ChunkX = MinChunkP.ChunkX; ChunkX <= MaxChunkP.ChunkX; ChunkX++)
        {
            world_chunk *Chunk = GetWorldChunk(World, ChunkX, ChunkY, SimRegion->Origin.ChunkZ);
            if(Chunk)
            {
                for(world_entity_block *Block = &Chunk->FirstBlock; Block; Block = Block->Next)
                {
                    for(uint32 EntityIndexInBlock = 0; EntityIndexInBlock < Block->EntityCount; EntityIndexInBlock++)
                    {
                        uint32 LowEntityIndex = Block->LowEntityIndex[EntityIndexInBlock];
                        low_entity *Low = GameState->LowEntities + LowEntityIndex;
						v2 SimSpaceP = GetSimSpaceP(SimRegion, Low);
						if(IsInRectangle(SimRegionBounds, &SimSpaceP))
						{
							AddEntity(SimRegion, Low, SimSpaceP);
						}
                    }        
                }
            }
        }
    }
}

internal void
EndSim(sim_region *Region, game_state *GameState)
{
	entity *Entity = Region->Entities;
	for(uint32 EntityIndex = 0;
	 	EntityIndex < Region->EntityCount;
	  	Entity++, EntityIndex++)
	{
		low_entity *Stored = GameState->LowEntities + Entity->StorageIndex;

		// TODO(george): Save state back to the stored entity, once high entities
		// do state decompression, etc.

		world_position NewP = MapIntoChunkSpace(GameState->World, Region->Origin, Entity->P);
		ChangeEntityLocation(&GameState->WorldArena, GameState->World, Entity->StorageIndex,
		   					 Stored, &Stored->P, &NewP);

		// TODO(george): Entity mapping hash table
		entity CameraFollowingEntity = ForceEntityIntoHigh(GameState, GameState->CameraFollowingEntityIndex);
		if(CameraFollowingEntity.High)
		{
			world_position NewCameraP = GameState->CameraP;

			NewCameraP.ChunkZ = CameraFollowingEntity.Low->P.ChunkZ;

#if 0
			if(CameraFollowingEntity.High->P.X > (9.0f*World->TileSideInMeters))
			{
				NewCameraP.AbsTileX += 17;
			}
			else if(CameraFollowingEntity.High->P.X < -(9.0f*World->TileSideInMeters))
			{
				NewCameraP.AbsTileX -= 17;
			}

			if(CameraFollowingEntity.High->P.Y > (5.0f*World->TileSideInMeters))
			{
				NewCameraP.AbsTileY += 9;
			}
			else if(CameraFollowingEntity.High->P.Y < -(5.0f*World->TileSideInMeters))
			{
				NewCameraP.AbsTileY -= 9;
			}
#else
			NewCameraP = CameraFollowingEntity.Low->P;
#endif
			// TODO(george): Map new entities in and old entities out!
			// TODO(george): Mapping tiles and stairs into the entity set!

			SetCamera(GameState, NewCameraP);
		}
	}
}