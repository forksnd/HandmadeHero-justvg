member_definition MembersOf_sim_entity_collision_volume[] = 
{
    {0, MetaType_v3, "OffsetP", (uint32)&((sim_entity_collision_volume *)0)->OffsetP},
    {0, MetaType_v3, "Dim", (uint32)&((sim_entity_collision_volume *)0)->Dim},
};
member_definition MembersOf_sim_entity_collision_volume_group[] = 
{
    {0, MetaType_sim_entity_collision_volume, "TotalVolume", (uint32)&((sim_entity_collision_volume_group *)0)->TotalVolume},
    {0, MetaType_uint32, "VolumeCount", (uint32)&((sim_entity_collision_volume_group *)0)->VolumeCount},
    {MetaMemberFlag_IsPointer, MetaType_sim_entity_collision_volume, "Volumes", (uint32)&((sim_entity_collision_volume_group *)0)->Volumes},
};
member_definition MembersOf_sim_entity[] = 
{
    {MetaMemberFlag_IsPointer, MetaType_world_chunk, "OldChunk", (uint32)&((sim_entity *)0)->OldChunk},
    {0, MetaType_uint32, "StorageIndex", (uint32)&((sim_entity *)0)->StorageIndex},
    {0, MetaType_bool32, "Updatable", (uint32)&((sim_entity *)0)->Updatable},
    {0, MetaType_entity_type, "Type", (uint32)&((sim_entity *)0)->Type},
    {0, MetaType_uint32, "Flags", (uint32)&((sim_entity *)0)->Flags},
    {0, MetaType_v3, "P", (uint32)&((sim_entity *)0)->P},
    {0, MetaType_v3, "dP", (uint32)&((sim_entity *)0)->dP},
    {0, MetaType_real32, "DistanceLimit", (uint32)&((sim_entity *)0)->DistanceLimit},
    {MetaMemberFlag_IsPointer, MetaType_sim_entity_collision_volume_group, "Collision", (uint32)&((sim_entity *)0)->Collision},
    {0, MetaType_real32, "FacingDirection", (uint32)&((sim_entity *)0)->FacingDirection},
    {0, MetaType_real32, "tBob", (uint32)&((sim_entity *)0)->tBob},
    {0, MetaType_int32, "dAbsTileZ", (uint32)&((sim_entity *)0)->dAbsTileZ},
    {0, MetaType_uint32, "HitPointMax", (uint32)&((sim_entity *)0)->HitPointMax},
    {0, MetaType_hit_point, "HitPoint", (uint32)&((sim_entity *)0)->HitPoint},
    {0, MetaType_entity_reference, "Sword", (uint32)&((sim_entity *)0)->Sword},
    {0, MetaType_v2, "WalkableDim", (uint32)&((sim_entity *)0)->WalkableDim},
    {0, MetaType_real32, "WalkableHeight", (uint32)&((sim_entity *)0)->WalkableHeight},
};
member_definition MembersOf_sim_region[] = 
{
    {MetaMemberFlag_IsPointer, MetaType_world, "World", (uint32)&((sim_region *)0)->World},
    {0, MetaType_real32, "MaxEntityRadius", (uint32)&((sim_region *)0)->MaxEntityRadius},
    {0, MetaType_real32, "MaxEntityVelocity", (uint32)&((sim_region *)0)->MaxEntityVelocity},
    {0, MetaType_world_position, "Origin", (uint32)&((sim_region *)0)->Origin},
    {0, MetaType_rectangle3, "Bounds", (uint32)&((sim_region *)0)->Bounds},
    {0, MetaType_rectangle3, "UpdatableBounds", (uint32)&((sim_region *)0)->UpdatableBounds},
    {0, MetaType_uint32, "MaxEntityCount", (uint32)&((sim_region *)0)->MaxEntityCount},
    {0, MetaType_uint32, "EntityCount", (uint32)&((sim_region *)0)->EntityCount},
    {MetaMemberFlag_IsPointer, MetaType_sim_entity, "Entities", (uint32)&((sim_region *)0)->Entities},
    {0, MetaType_sim_entity_hash, "Hash", (uint32)&((sim_region *)0)->Hash},
};
member_definition MembersOf_rectangle2[] = 
{
    {0, MetaType_v2, "Min", (uint32)&((rectangle2 *)0)->Min},
    {0, MetaType_v2, "Max", (uint32)&((rectangle2 *)0)->Max},
};
member_definition MembersOf_rectangle3[] = 
{
    {0, MetaType_v3, "Min", (uint32)&((rectangle3 *)0)->Min},
    {0, MetaType_v3, "Max", (uint32)&((rectangle3 *)0)->Max},
};
member_definition MembersOf_world_position[] = 
{
    {0, MetaType_int32, "ChunkX", (uint32)&((world_position *)0)->ChunkX},
    {0, MetaType_int32, "ChunkY", (uint32)&((world_position *)0)->ChunkY},
    {0, MetaType_int32, "ChunkZ", (uint32)&((world_position *)0)->ChunkZ},
    {0, MetaType_v3, "Offset_", (uint32)&((world_position *)0)->Offset_},
};
#define META_HANDLE_TYPE_DUMP(MemberPtr, NextIndentLevel) \
    case MetaType_world_position: {DEBUGTextLine(Member->Name);DEBUGDumpStruct(ArrayCount(MembersOf_world_position), MembersOf_world_position, MemberPtr, (NextIndentLevel));} break; \
    case MetaType_rectangle3: {DEBUGTextLine(Member->Name);DEBUGDumpStruct(ArrayCount(MembersOf_rectangle3), MembersOf_rectangle3, MemberPtr, (NextIndentLevel));} break; \
    case MetaType_rectangle2: {DEBUGTextLine(Member->Name);DEBUGDumpStruct(ArrayCount(MembersOf_rectangle2), MembersOf_rectangle2, MemberPtr, (NextIndentLevel));} break; \
    case MetaType_sim_region: {DEBUGTextLine(Member->Name);DEBUGDumpStruct(ArrayCount(MembersOf_sim_region), MembersOf_sim_region, MemberPtr, (NextIndentLevel));} break; \
    case MetaType_sim_entity: {DEBUGTextLine(Member->Name);DEBUGDumpStruct(ArrayCount(MembersOf_sim_entity), MembersOf_sim_entity, MemberPtr, (NextIndentLevel));} break; \
    case MetaType_sim_entity_collision_volume_group: {DEBUGTextLine(Member->Name);DEBUGDumpStruct(ArrayCount(MembersOf_sim_entity_collision_volume_group), MembersOf_sim_entity_collision_volume_group, MemberPtr, (NextIndentLevel));} break; \
    case MetaType_sim_entity_collision_volume: {DEBUGTextLine(Member->Name);DEBUGDumpStruct(ArrayCount(MembersOf_sim_entity_collision_volume), MembersOf_sim_entity_collision_volume, MemberPtr, (NextIndentLevel));} break; 
