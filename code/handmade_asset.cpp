struct load_asset_work
{
    task_with_memory *Task;
    asset_slot *Slot;

    platform_file_handle *Handle;
    uint64 Offset;
    uint64 Size;
    void *Destination;

    uint32 FinalState;
};
internal PLATFORM_WORK_QUEUE_CALLBACK(LoadAssetWork)
{
    load_asset_work *Work = (load_asset_work *)Data;

    Platform.ReadDataFromFile(Work->Handle, Work->Offset, Work->Size, Work->Destination);

    CompletePreviousWritesBeforeFutureWrites;

    if(!PlatformNoFileErrors(Work->Handle))
    {
        // TODO(georgy): Should we actually fill in bogus data here and set to final state anyway?
        ZeroSize(Work->Size, Work->Destination);
    }

    Work->Slot->State = Work->FinalState;
    
    EndTaskWithMemory(Work->Task);
}

inline platform_file_handle *
GetFileHandleFor(game_assets *Assets, uint32 FileIndex)
{
    Assert(FileIndex < Assets->FileCount);

    platform_file_handle *Result = Assets->Files[FileIndex].Handle;
    return(Result);
}

inline void *
AcquireAssetMemory(game_assets *Assets, memory_index Size)
{
    void *Result = Platform.AllocateMemory(Size);
    if(Result)
    {
        Assets->TotalMemoryUsed += Size;
    }
    return(Result);
}

inline void 
ReleaseAssetMemory(game_assets *Assets, memory_index Size, void *Memory)
{
    if(Memory)
    {
        Assets->TotalMemoryUsed -= Size;
    }
    Platform.DeallocateMemory(Memory);
}

struct asset_memory_size
{
    uint32 Total;
    uint32 Data;
    uint32 Section;
};
asset_memory_size
GetSizeOfAsset(game_assets *Assets, uint32 Type, uint32 SlotIndex)
{
    asset *Asset = Assets->Assets + SlotIndex;

    asset_memory_size Result = {};
    if(Type == AssetState_Sound)
    {
        hha_sound *Info = &Asset->HHA.Sound;

        Result.Section = Info->SampleCount*sizeof(int16);
        Result.Data = Info->ChannelCount*Result.Section;
    }
    else
    {
        Assert(Type == AssetState_Bitmap);
        
        hha_bitmap *Info = &Asset->HHA.Bitmap;

        uint16 Width = SafeTruncateToUInt16(Info->Dim[0]);
        uint16 Height = SafeTruncateToUInt16(Info->Dim[1]);
        Result.Section = 4*Width;
        Result.Data = Height*Result.Section;
    }

    Result.Total = Result.Data + sizeof(asset_memory_header);

    return(Result);
}

internal void
AddAssetsHeaderToList(game_assets *Assets, uint32 SlotIndex, void *Memory, asset_memory_size Size)
{
    asset_memory_header *Header = (asset_memory_header *)((uint8 *)Memory + Size.Data);

    asset_memory_header *Sentinel = &Assets->LoadedAssetSentinel;
    
    Header->SlotIndex = SlotIndex;
    Header->Prev = Sentinel;
    Header->Next = Sentinel->Next;

    Header->Prev->Next = Header;
    Header->Next->Prev = Header;
}

internal void
RemoveAssetsHeaderFromList(asset_memory_header *Header)
{
    Header->Prev->Next = Header->Next;
    Header->Next->Prev = Header->Prev;

    Header->Next = Header->Prev = 0;
}

internal void
LoadBitmap(game_assets *Assets, bitmap_id ID)
{
    asset_slot *Slot = Assets->Slots + ID.Value;
    if((ID.Value) && (AtomicCompareExchangeUInt32((uint32 *)&Slot->State, AssetState_Queued, AssetState_Unloaded) ==
       AssetState_Unloaded))   
    {
        task_with_memory *Task = BeginTaskWithMemory(Assets->TranState);
        if(Task)
        {
            asset *Asset = Assets->Assets + ID.Value;
            hha_bitmap *Info = &Asset->HHA.Bitmap;
            loaded_bitmap *Bitmap = &Slot->Bitmap;

            Bitmap->AlignPercentage = V2(Info->AlignPercentage[0], Info->AlignPercentage[1]);
            Bitmap->WidthOverHeight = (real32)Info->Dim[0] / (real32)Info->Dim[1];
            Bitmap->Width = SafeTruncateToUInt16(Info->Dim[0]);
            Bitmap->Height = SafeTruncateToUInt16(Info->Dim[1]);

            asset_memory_size Size = GetSizeOfAsset(Assets, AssetState_Bitmap, ID.Value);
            Bitmap->Pitch = SafeTruncateToInt16(Size.Section);
            Bitmap->Memory = AcquireAssetMemory(Assets, Size.Total);// PushSize(&Assets->Arena, MemorySize);

            load_asset_work *Work = PushStruct(&Task->Arena, load_asset_work);
            Work->Task = Task;
            Work->Slot =  Assets->Slots + ID.Value;
            Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
            Work->Offset = Asset->HHA.DataOffset;
            Work->Size = Size.Data;
            Work->Destination = Bitmap->Memory;
            Work->FinalState = (AssetState_Bitmap | AssetState_Loaded);

            AddAssetsHeaderToList(Assets, ID.Value, Bitmap->Memory, Size);

            Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, Work);
        }
        else
        {
            Slot->State = AssetState_Unloaded;
        }
    }    
}

internal void 
LoadSound(game_assets *Assets, sound_id ID)
{
    asset_slot *Slot = Assets->Slots + ID.Value;    
	if((ID.Value) && (AtomicCompareExchangeUInt32((uint32 *)&Slot->State, AssetState_Queued, AssetState_Unloaded) ==
       AssetState_Unloaded))   
    {
        task_with_memory *Task = BeginTaskWithMemory(Assets->TranState);
        if(Task)
        {
            asset *Asset = Assets->Assets + ID.Value;
            hha_sound *Info = &Asset->HHA.Sound;
            loaded_sound *Sound = &Slot->Sound;
            Sound->SampleCount = Info->SampleCount;
            Sound->ChannelCount = Info->ChannelCount;
            
            asset_memory_size Size = GetSizeOfAsset(Assets, AssetState_Sound, ID.Value);
            uint32 ChannelSize = Size.Section; 
            void *Memory = AcquireAssetMemory(Assets, Size.Total); //PushSize(&Assets->Arena, MemorySize);
            
            int16 *SoundAt = (int16 *)Memory;
            for(uint32 ChannelIndex = 0;
                ChannelIndex < Sound->ChannelCount;
                ChannelIndex++)
            {
                Sound->Samples[ChannelIndex] = SoundAt;
                SoundAt += ChannelSize;
            }

            load_asset_work *Work = PushStruct(&Task->Arena, load_asset_work);
            Work->Task = Task;
            Work->Slot =  Assets->Slots + ID.Value;
            Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
            Work->Offset = Asset->HHA.DataOffset;
            Work->Size = Size.Data;
            Work->Destination = Memory;
            Work->FinalState = (AssetState_Sound | AssetState_Loaded);

            AddAssetsHeaderToList(Assets, ID.Value, Memory, Size);

            Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, Work);
        }
        else
        {
            Assets->Slots[ID.Value].State = AssetState_Unloaded;
        }
    }
}

internal uint32
GetBestMatchAssetFrom(game_assets *Assets, asset_type_id TypeID, 
                      asset_vector *MatchVector, asset_vector *WeightVector)
{
    uint32 Result = 0;
    
    real32 BestDiff = Real32Maximum;

    asset_type *Type = Assets->AssetTypes + TypeID;
    for(uint32 AssetIndex = Type->FirstAssetIndex;
        AssetIndex < Type->OnePastLastAssetIndex;
        AssetIndex++)
    {
        asset *Asset = Assets->Assets + AssetIndex;

        real32 TotalWeightedDiff = 0.0f;
        for(uint32 TagIndex = Asset->HHA.FirstTagIndex;
            TagIndex < Asset->HHA.OnePastLastTagIndex;
            TagIndex++)
        {
            hha_tag *Tag = Assets->Tags + TagIndex;

            real32 A = MatchVector->E[Tag->ID];
            real32 B = Tag->Value;
            real32 D0 = AbsoluteValue(A - B);
            real32 D1 = AbsoluteValue((A - Assets->TagRange[Tag->ID]*SignOf(A)) - B);
            real32 Difference = Minimum(D0, D1);

            real32 Weighted = WeightVector->E[Tag->ID]*Difference;
            TotalWeightedDiff += Weighted;
        }
        
        if(BestDiff > TotalWeightedDiff)
        {
            BestDiff = TotalWeightedDiff;
            Result = AssetIndex;
        }
    }

    return(Result);
}

internal uint32
GetRandomSlotFrom(game_assets *Assets, asset_type_id TypeID, random_series *Series)
{
    uint32 Result = 0;

    asset_type *Type = Assets->AssetTypes + TypeID;
    if(Type->FirstAssetIndex != Type->OnePastLastAssetIndex)
    {
        uint32 Count = Type->OnePastLastAssetIndex - Type->FirstAssetIndex;
        uint32 Choice = RandomChoice(Series, Count);

        Result = Type->FirstAssetIndex + Choice;
    }

    return(Result);
}

internal uint32
GetFirstSlotFrom(game_assets *Assets, asset_type_id TypeID)
{
    uint32 Result = 0;

    asset_type *Type = Assets->AssetTypes + TypeID;
    if(Type->FirstAssetIndex != Type->OnePastLastAssetIndex)
    {
        Result = Type->FirstAssetIndex;
    }

    return(Result);
}

inline bitmap_id
GetFirstBitmapFrom(game_assets *Assets, asset_type_id TypeID)
{
    bitmap_id Result = {GetFirstSlotFrom(Assets, TypeID)};
    return(Result);
}

inline bitmap_id
GetRandomBitmapFrom(game_assets *Assets, asset_type_id TypeID, random_series *Series)
{
    bitmap_id Result = {GetRandomSlotFrom(Assets, TypeID, Series)};
    return(Result);
}

internal bitmap_id
GetBestMatchBitmapFrom(game_assets *Assets, asset_type_id TypeID, 
                       asset_vector *MatchVector, asset_vector *WeightVector)
{
    bitmap_id Result = {GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector)};
    return(Result);
}

inline sound_id
GetFirstSoundFrom(game_assets *Assets, asset_type_id TypeID)
{
    sound_id Result = {GetFirstSlotFrom(Assets, TypeID)};
    return(Result);
}

inline sound_id
GetRandomSoundFrom(game_assets *Assets, asset_type_id TypeID, random_series *Series)
{
    sound_id Result = {GetRandomSlotFrom(Assets, TypeID, Series)};
    return(Result);
}

internal sound_id
GetBestMatchSoundFrom(game_assets *Assets, asset_type_id TypeID, 
                       asset_vector *MatchVector, asset_vector *WeightVector)
{
    sound_id Result = {GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector)};
    return(Result);
}

internal game_assets *
AllocateGameAssets(memory_arena *Arena, memory_index Size, transient_state *TranState)
{
	game_assets *Assets = PushStruct(Arena, game_assets);
	SubArena(&Assets->Arena, Arena, Size);
	Assets->TranState = TranState;
    Assets->TotalMemoryUsed = 0;
    Assets->TargetMemoryUsed = Size;

    Assets->LoadedAssetSentinel.Next = &Assets->LoadedAssetSentinel;
    Assets->LoadedAssetSentinel.Prev = &Assets->LoadedAssetSentinel;

    for(uint32 TagType = 0;
        TagType < Tag_Count;
        TagType++)
    {
        Assets->TagRange[TagType] = 1000000.0f;
    }
    Assets->TagRange[Tag_FacingDirection] = 2.0f*Pi32;

    Assets->TagCount = 1;
    Assets->AssetCount = 1;
    {
        platform_file_group *FileGroup = Platform.GetAllFilesOfTypeBegin("hha");
        Assets->FileCount = FileGroup->FileCount;
        Assets->Files = PushArray(Arena, Assets->FileCount, asset_file);
        for(uint32 FileIndex = 0;
            FileIndex < Assets->FileCount;
            FileIndex++)
        {
            asset_file *File = Assets->Files + FileIndex;

            File->TagBase = Assets->TagCount;
            
            ZeroStruct(File->Header);
            File->Handle = Platform.OpenNextFile(FileGroup);
            Platform.ReadDataFromFile(File->Handle, 0, sizeof(File->Header), &File->Header);

            uint32 AssetTypeArraySize = File->Header.AssetTypeCount*sizeof(hha_asset_type);
            File->AssetTypeArray = (hha_asset_type *)PushSize(Arena, AssetTypeArraySize);
            Platform.ReadDataFromFile(File->Handle, File->Header.AssetTypes, AssetTypeArraySize, File->AssetTypeArray);
            
            if(File->Header.MagicValue != HHA_MAGIC_VALUE)
            {
                Platform.FileError(File->Handle, "HHA file has an invalid magic value.");
            }

            if(File->Header.Version > HHA_VERSION)
            {
                Platform.FileError(File->Handle, "HHA file is of a later version.");
            }
            
            if(PlatformNoFileErrors(File->Handle))
            {
                // NOTE(georgy): The first asset and tag slot in every HHA is a null asset (reserved)
                // so we don't count it as something we will need space for!
                Assets->TagCount += (File->Header.TagCount - 1);
                Assets->AssetCount += (File->Header.AssetCount - 1); 
            }
            else
            {
                // TODO(georgy): Eventually, have some way of notifying users of bogus files?
                InvalidCodePath;
            }
        }
        Platform.GetAllFilesOfTypeEnd(FileGroup);
    }

    // NOTE(georgy): Allocate all metadata space
    Assets->Assets = PushArray(Arena, Assets->AssetCount, asset);
    Assets->Slots = PushArray(Arena, Assets->AssetCount, asset_slot);
    Assets->Tags = PushArray(Arena, Assets->TagCount, hha_tag);

    // NOTE(georgy): Reserve one null tag at the beginning
    ZeroStruct(Assets->Tags[0]);

    // NOTE(georgy): Load tags
    for(uint32 FileIndex = 0;
        FileIndex < Assets->FileCount;
        FileIndex++)
    {
        asset_file *File = Assets->Files + FileIndex;
        if(PlatformNoFileErrors(File->Handle))
        {
            // NOTE(georgy): Skip the first tag, since it's null!
            uint32 TagArraySize = sizeof(hha_tag) * (File->Header.TagCount - 1);
            Platform.ReadDataFromFile(File->Handle, File->Header.Tags + sizeof(hha_tag), TagArraySize, Assets->Tags + File->TagBase);
        }
    }

    // NOTE(georgy): Reserve one null asset at the beginning
    uint32 AssetCount = 0;
    ZeroStruct(*(Assets->Assets + AssetCount));
    AssetCount++;

    for(uint32 DestTypeID = 0;
        DestTypeID < Asset_Count;
        DestTypeID++)
    {
        asset_type *DestType = Assets->AssetTypes + DestTypeID;
        DestType->FirstAssetIndex = AssetCount;

        for(uint32 FileIndex = 0;
            FileIndex < Assets->FileCount;
            FileIndex++)
        {
            asset_file *File = Assets->Files + FileIndex;
            if(PlatformNoFileErrors(File->Handle))
            {
                for(uint32 SourceIndex = 0;
                    SourceIndex < File->Header.AssetTypeCount;
                    SourceIndex++)
                {
                    hha_asset_type *SourceType = File->AssetTypeArray + SourceIndex;
                    if(SourceType->TypeID == DestTypeID)
                    {
                        uint32 AssetCountForType = (SourceType->OnePastLastAssetIndex - SourceType->FirstAssetIndex);

                        temporary_memory TempMem = BeginTemporaryMemory(&TranState->TranArena);
                        hha_asset *HHAAssetArray = PushArray(&TranState->TranArena, AssetCountForType, hha_asset);
						
                        Platform.ReadDataFromFile(File->Handle, 
                                                  File->Header.Assets + SourceType->FirstAssetIndex*sizeof(hha_asset), 
                                                  AssetCountForType*sizeof(hha_asset), 
                                                  HHAAssetArray);
                        for(uint32 AssetIndex = 0;
                            AssetIndex < AssetCountForType;
                            AssetIndex++)
                        {
                            hha_asset *HHAAsset = HHAAssetArray + AssetIndex;

                            Assert(AssetCount < Assets->AssetCount); 
                            asset *Asset = Assets->Assets + AssetCount++;

                            Asset->HHA = *HHAAsset;
                            Asset->FileIndex = FileIndex;
                            if(Asset->HHA.FirstTagIndex == 0)
                            {
                                Asset->HHA.FirstTagIndex = 0;
                                Asset->HHA.OnePastLastTagIndex = 0;
                            }
                            else
                            {
                                Asset->HHA.FirstTagIndex += (File->TagBase - 1);
                                Asset->HHA.OnePastLastTagIndex += (File->TagBase - 1);
                            }
                        }

                        EndTemporaryMemory(TempMem);
                    }
                }
            }
        }

        DestType->OnePastLastAssetIndex = AssetCount;
    }

    Assert(AssetCount == Assets->AssetCount);

    return(Assets);
}

internal void
EvictAsset(game_assets *Assets, asset_memory_header *Header)
{
    uint32 SlotIndex = Header->SlotIndex;
    asset_slot *Slot = Assets->Slots + SlotIndex;
    Assert(Slot->State == AssetState_Loaded);

    asset_memory_size Size = GetSizeOfAsset(Assets, GetType(Slot), SlotIndex);
    void *Memory = 0;
    if(GetType(Slot) == AssetState_Sound)
    {
        Memory = Slot->Sound.Samples[0];
    }
    else
    {
        Assert(GetType(Slot) == AssetState_Bitmap);
        Memory = Slot->Bitmap.Memory;
    }

    RemoveAssetsHeaderFromList(Header);
    ReleaseAssetMemory(Assets, Size.Total, Memory);

    Slot->State = AssetState_Unloaded;
}

internal void 
EvictAssetsAsNecessary(game_assets *Assets)
{
    while(Assets->TotalMemoryUsed > Assets->TargetMemoryUsed)
    {
        asset_memory_header *Asset = Assets->LoadedAssetSentinel.Prev;
        if(Asset != &Assets->LoadedAssetSentinel)
        {
            EvictAsset(Assets, Asset);            
        }
        else
        {
            InvalidCodePath;
            break;
        }
    }
}