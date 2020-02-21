enum finalize_asset_operation
{
    FinalizeAsset_None,
    FinalizeAsset_Font,
};
struct load_asset_work
{
    task_with_memory *Task;
    asset *Asset;

    platform_file_handle *Handle;
    uint64 Offset;
    uint64 Size;
    void *Destination;

    finalize_asset_operation FinalizeOperation;
    uint32 FinalState;
};
internal void
LoadAssetWorkDirectly(load_asset_work *Work)
{
    Platform.ReadDataFromFile(Work->Handle, Work->Offset, Work->Size, Work->Destination);
    if(PlatformNoFileErrors(Work->Handle))
    {
        switch(Work->FinalizeOperation)
        {
            case FinalizeAsset_None:
            {
                // NOTE(georgy): Nothing to do
            } break;

            case FinalizeAsset_Font:
            {
                loaded_font *Font = &Work->Asset->Header->Font;
                hha_font *HHA = &Work->Asset->HHA.Font;
                for(uint32 GlyphIndex = 1;
                    GlyphIndex < HHA->GlyphCount;
                    GlyphIndex++)
                {
                    hha_font_glyph *Glyph = Font->Glyphs + GlyphIndex;
                    
                    Assert(Glyph->UnicodeCodePoint < HHA->OnePastHighestCodepoint);
                    Assert((uint32)(uint16)GlyphIndex == GlyphIndex);
                    Font->UnicodeMap[Glyph->UnicodeCodePoint] = (uint16)GlyphIndex;
                }
            } break;
        }
    }    

    CompletePreviousWritesBeforeFutureWrites;

    if(!PlatformNoFileErrors(Work->Handle))
    {
        // TODO(georgy): Should we actually fill in bogus data here and set to final state anyway?
        ZeroSize(Work->Size, Work->Destination);
    }

    Work->Asset->State = Work->FinalState;
}
internal PLATFORM_WORK_QUEUE_CALLBACK(LoadAssetWork)
{
    load_asset_work *Work = (load_asset_work *)Data;

    LoadAssetWorkDirectly(Work);
    
    EndTaskWithMemory(Work->Task);
}

inline asset_file *
GetFile(game_assets *Assets, uint32 FileIndex)
{
    Assert(FileIndex < Assets->FileCount);
    asset_file *Result = Assets->Files + FileIndex;

    return(Result);
}

inline platform_file_handle *
GetFileHandleFor(game_assets *Assets, uint32 FileIndex)
{
    platform_file_handle *Result = &GetFile(Assets, FileIndex)->Handle;

    return(Result);
}

internal asset_memory_block *
FindBlockForSize(game_assets *Assets, memory_index Size)
{
    asset_memory_block *Result = 0;

    // TODO(georgy): This probably will need to be accelerated in the
    // future as the resident asset count grows

    // TODO(georgy): Best match block!
    for(asset_memory_block *Block = Assets->MemorySentinel.Next;
        Block != &Assets->MemorySentinel;
        Block = Block->Next)
    {
        if(!(Block->Flags & AssetMemory_Used))
        {
            if(Block->Size >= Size)
            {
                Result = Block;
                break;
            }
        }
    }

    return(Result);
}

internal asset_memory_block *
InsertBlock(asset_memory_block *Prev, uint64 Size, void *Memory)
{
    Assert(Size > sizeof(asset_memory_block));
    asset_memory_block *Block = (asset_memory_block *)Memory;
    Block->Flags = 0;
    Block->Size = Size - sizeof(asset_memory_block);
    Block->Prev = Prev;
    Block->Next = Prev->Next;
    Block->Prev->Next = Block;
    Block->Next->Prev = Block;

    return(Block);
}

internal bool32
MergeIfPossible(game_assets *Assets, asset_memory_block *First, asset_memory_block *Second) 
{
    bool32 Result = false;

    if((First != &Assets->MemorySentinel) &&
       (Second != &Assets->MemorySentinel))
    {
        if(!(First->Flags & AssetMemory_Used) && 
           !(Second->Flags & AssetMemory_Used))
        {
            uint8 *ExpectedSecond = (uint8 *)First + sizeof(asset_memory_block) + First->Size;
            if((uint8 *)Second == ExpectedSecond)
            {
                Second->Next->Prev = Second->Prev;
                Second->Prev->Next = Second->Next;

                First->Size += sizeof(asset_memory_block) + Second->Size;

                Result = true;
            }
        }
    }

    return (Result);
}

struct asset_memory_size
{
    uint32 Total;
    uint32 Data;
    uint32 Section;
};

internal bool32
GenerationHasCompleted(game_assets *Assets, uint32 CheckID) 
{
    bool32 Result = true;

    for(uint32 Index = 0;
        Index < Assets->InFlightGenerationCount;
        Index++)
    {
        if(Assets->InFlightGenerations[Index] == CheckID)
        {
            Result = false;
            break;
        }
    }

    return(Result);
}

internal asset_memory_header *
AcquireAssetMemory(game_assets *Assets, uint32 Size, uint32 AssetIndex)
{
    asset_memory_header *Result = 0;

    BeginAssetLock(Assets);

    asset_memory_block *Block = FindBlockForSize(Assets, Size);
    for(;;)
    {
        if(Block && (Size <= Block->Size))
        {
            Block->Flags |= AssetMemory_Used;

            Result = (asset_memory_header *)(Block + 1);

            memory_index RemainingSize = Block->Size - Size;
            memory_index BlockSplitThreshold = 4096; // TODO(georgy): Set this on the smallest asset size
            if(RemainingSize > BlockSplitThreshold)
            {
                Block->Size -= RemainingSize;
                InsertBlock(Block, RemainingSize, (uint8 *)Result + Size);
            }
            else
            {
                // TODO(georgy): Actually record the unused portion of the memory
                // in a block so that we can do the merge on blocks when neighbors
                // are freed.
            }

            break;
        }
        else
        {
            for(asset_memory_header *Header = Assets->LoadedAssetSentinel.Prev;
                Header != &Assets->LoadedAssetSentinel;
                Header = Header->Prev)
            {
                asset *Asset = Assets->Assets + Header->AssetIndex;
                if((Asset->State >= AssetState_Loaded) &&
                    GenerationHasCompleted(Assets, Asset->Header->GenerationID)) 
                {
                    Assert(Asset->State == AssetState_Loaded);

                    RemoveAssetsHeaderFromList(Header);

                    Block = (asset_memory_block *)Asset->Header - 1;
                    Block->Flags &= ~AssetMemory_Used;

                    if(MergeIfPossible(Assets, Block->Prev, Block))
                    {
                        Block = Block->Prev;
                    }

                    MergeIfPossible(Assets, Block, Block->Next);

                    Asset->State = AssetState_Unloaded;
                    Asset->Header = 0;  

                    // TODO(georgy): Actually do this!
                    // Block = EvictAsset(Assets, Header);     
                    break;       
                }
            }
        }
    }
    
    if(Result)
    {
        Result->AssetIndex = AssetIndex;
        Result->TotalSize = Size;
        InsertAssetHeaderAtFront(Assets, Result);
    }

    EndAssetLock(Assets);

    return(Result);
}

internal void
LoadBitmap(game_assets *Assets, bitmap_id ID, bool32 Immediate)
{
    asset *Asset = Assets->Assets + ID.Value;
    if(ID.Value)   
    {   
        if((AtomicCompareExchangeUInt32((uint32 *)&Asset->State, AssetState_Queued, AssetState_Unloaded) ==
            AssetState_Unloaded))
        {

            task_with_memory *Task = 0;

            if(!Immediate)
            {
                Task = BeginTaskWithMemory(Assets->TranState, false);
            }

            if(Immediate || Task)
            {
                asset *Asset = Assets->Assets + ID.Value;
                hha_bitmap *Info = &Asset->HHA.Bitmap;

                asset_memory_size Size = {};
                uint32 Width = Info->Dim[0];
                uint32 Height = Info->Dim[1];
                Size.Section = 4*Width;
                Size.Data = Height*Size.Section;
                Size.Total = Size.Data + sizeof(asset_memory_header);

                Asset->Header = AcquireAssetMemory(Assets, Size.Total, ID.Value);

                loaded_bitmap *Bitmap = &Asset->Header->Bitmap;
                Bitmap->AlignPercentage = V2(Info->AlignPercentage[0], Info->AlignPercentage[1]);
                Bitmap->WidthOverHeight = (real32)Info->Dim[0] / (real32)Info->Dim[1];
                Bitmap->Width = Width;
                Bitmap->Height = Height;
                Bitmap->Pitch = Size.Section;
                Bitmap->Handle = 0;
                Bitmap->Memory = (Asset->Header + 1);

                load_asset_work Work;
                Work.Task = Task;
                Work.Asset =  Assets->Assets + ID.Value;
                Work.Handle = GetFileHandleFor(Assets, Asset->FileIndex);
                Work.Offset = Asset->HHA.DataOffset;
                Work.Size = Size.Data;
                Work.Destination = Bitmap->Memory;
                Work.FinalizeOperation = FinalizeAsset_None;
                Work.FinalState = AssetState_Loaded;
                if(Task)
                {
                    load_asset_work *TaskWork = PushStruct(&Task->Arena, load_asset_work, NoClear());
                    *TaskWork = Work;
                    Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, TaskWork);
                }
                else
                {
                    LoadAssetWorkDirectly(&Work);
                }
            }
            else
            {
                Asset->State = AssetState_Unloaded;
            }
        }
        else if(Immediate)
        {
            // TODO(georgy): Do we want to have a more coherent story here
            // for what happens when two force-load people all hit the load
            // at the same time
            asset_state volatile *State = (asset_state volatile *)&Asset->State;
            while(*State == AssetState_Queued) {}
        }
    }    
}

internal void 
LoadSound(game_assets *Assets, sound_id ID)
{
    asset *Asset = Assets->Assets + ID.Value;    
	if((ID.Value) && (AtomicCompareExchangeUInt32((uint32 *)&Asset->State, AssetState_Queued, AssetState_Unloaded) ==
       AssetState_Unloaded))   
    {
        task_with_memory *Task = BeginTaskWithMemory(Assets->TranState, false);
        if(Task)
        {
            asset *Asset = Assets->Assets + ID.Value;
            hha_sound *Info = &Asset->HHA.Sound;

            asset_memory_size Size = {};
            Size.Section = Info->SampleCount*sizeof(int16);
            Size.Data = Info->ChannelCount*Size.Section;
            Size.Total = Size.Data + sizeof(asset_memory_header);

            Asset->Header = AcquireAssetMemory(Assets, Size.Total, ID.Value);
            loaded_sound *Sound = &Asset->Header->Sound;

            Sound->SampleCount = Info->SampleCount;
            Sound->ChannelCount = Info->ChannelCount;
            uint32 ChannelSize = Size.Section; 

            void *Memory = (Asset->Header + 1);            
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
            Work->Asset =  Assets->Assets + ID.Value;
            Work->Handle = GetFileHandleFor(Assets, Asset->FileIndex);
            Work->Offset = Asset->HHA.DataOffset;
            Work->Size = Size.Data;
            Work->Destination = Memory;
            Work->FinalizeOperation = FinalizeAsset_None;
            Work->FinalState = AssetState_Loaded;

            Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, Work);
        }
        else
        {
            Assets->Assets[ID.Value].State = AssetState_Unloaded;
        }
    }
}

internal void
LoadFont(game_assets *Assets, font_id ID, bool32 Immediate)
{
    // TODO(georgy): Merge all the same between LoadBitmap, LoadSound, LoadFont
    asset *Asset = Assets->Assets + ID.Value;
    if(ID.Value)   
    {   
        if((AtomicCompareExchangeUInt32((uint32 *)&Asset->State, AssetState_Queued, AssetState_Unloaded) ==
            AssetState_Unloaded))
        {

            task_with_memory *Task = 0;

            if(!Immediate)
            {
                Task = BeginTaskWithMemory(Assets->TranState, false);
            }

            if(Immediate || Task)
            {
                asset *Asset = Assets->Assets + ID.Value;
                hha_font *Info = &Asset->HHA.Font;

                uint32 HorizontalAdvanceSize = sizeof(real32)*Info->GlyphCount*Info->GlyphCount;
                uint32 GlyphsSize = Info->GlyphCount*sizeof(hha_font_glyph);
                uint32 UnicodeMapSize = sizeof(uint16)*Info->OnePastHighestCodepoint;
                uint32 SizeData = GlyphsSize + HorizontalAdvanceSize;
                uint32 SizeTotal = SizeData + sizeof(asset_memory_header) + UnicodeMapSize;

                Asset->Header = AcquireAssetMemory(Assets, SizeTotal, ID.Value);

                loaded_font *Font = &Asset->Header->Font;
                Font->BitmapIDOffset = GetFile(Assets, Asset->FileIndex)->FontBitmapIDOffset;
                Font->Glyphs = (hha_font_glyph *)(Asset->Header + 1);
                Font->HorizontalAdvance = (real32 *)((uint8 *)Font->Glyphs + GlyphsSize);
                Font->UnicodeMap = (uint16*)((uint8 *)Font->HorizontalAdvance + HorizontalAdvanceSize);

                ZeroSize(UnicodeMapSize, Font->UnicodeMap);

                load_asset_work Work;
                Work.Task = Task;
                Work.Asset =  Assets->Assets + ID.Value;
                Work.Handle = GetFileHandleFor(Assets, Asset->FileIndex);
                Work.Offset = Asset->HHA.DataOffset;
                Work.Size = SizeData;
                Work.Destination = Font->Glyphs;
                Work.FinalizeOperation = FinalizeAsset_Font;
                Work.FinalState = AssetState_Loaded;
                if(Task)
                {
                    load_asset_work *TaskWork = PushStruct(&Task->Arena, load_asset_work, NoClear());
                    *TaskWork = Work;
                    Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, TaskWork);
                }
                else
                {
                    LoadAssetWorkDirectly(&Work);
                }
            }
            else
            {
                Asset->State = AssetState_Unloaded;
            }
        }
        else if(Immediate)
        {
            // TODO(georgy): Do we want to have a more coherent story here
            // for what happens when two force-load people all hit the load
            // at the same time
            asset_state volatile *State = (asset_state volatile *)&Asset->State;
            while(*State == AssetState_Queued) {}
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
GetRandomAssetFrom(game_assets *Assets, asset_type_id TypeID, random_series *Series)
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
GetFirstAssetFrom(game_assets *Assets, asset_type_id TypeID)
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
    bitmap_id Result = {GetFirstAssetFrom(Assets, TypeID)};
    return(Result);
}

inline bitmap_id
GetRandomBitmapFrom(game_assets *Assets, asset_type_id TypeID, random_series *Series)
{
    bitmap_id Result = {GetRandomAssetFrom(Assets, TypeID, Series)};
    return(Result);
}

internal bitmap_id
GetBestMatchBitmapFrom(game_assets *Assets, asset_type_id TypeID, 
                       asset_vector *MatchVector, asset_vector *WeightVector)
{
    bitmap_id Result = {GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector)};
    return(Result);
}

internal font_id 
GetBestMatchFontFrom(game_assets *Assets, asset_type_id TypeID, asset_vector *MatchVector, asset_vector *WeightVector)
{
    font_id Result = {GetBestMatchAssetFrom(Assets, TypeID, MatchVector, WeightVector)};
    return(Result);
}

inline sound_id
GetFirstSoundFrom(game_assets *Assets, asset_type_id TypeID)
{
    sound_id Result = {GetFirstAssetFrom(Assets, TypeID)};
    return(Result);
}

inline sound_id
GetRandomSoundFrom(game_assets *Assets, asset_type_id TypeID, random_series *Series)
{
    sound_id Result = {GetRandomAssetFrom(Assets, TypeID, Series)};
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

    Assets->NextGenerationID = 0;
    Assets->InFlightGenerationCount = 0;

    Assets->MemorySentinel.Flags = 0;
    Assets->MemorySentinel.Size = 0;
    Assets->MemorySentinel.Prev = &Assets->MemorySentinel;
    Assets->MemorySentinel.Next = &Assets->MemorySentinel;

    InsertBlock(&Assets->MemorySentinel, Size, PushSize(Arena, Size, NoClear()));

	Assets->TranState = TranState;

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
        platform_file_group FileGroup = Platform.GetAllFilesOfTypeBegin(PlatformFileType_AssetFile);
        Assets->FileCount = FileGroup.FileCount;
        Assets->Files = PushArray(Arena, Assets->FileCount, asset_file);
        for(uint32 FileIndex = 0;
            FileIndex < Assets->FileCount;
            FileIndex++)
        {
            asset_file *File = Assets->Files + FileIndex;

            File->TagBase = Assets->TagCount;
            File->FontBitmapIDOffset = 0;

            ZeroStruct(File->Header);
            File->Handle = Platform.OpenNextFile(&FileGroup);
            Platform.ReadDataFromFile(&File->Handle, 0, sizeof(File->Header), &File->Header);

            uint32 AssetTypeArraySize = File->Header.AssetTypeCount*sizeof(hha_asset_type);
            File->AssetTypeArray = (hha_asset_type *)PushSize(Arena, AssetTypeArraySize);
            Platform.ReadDataFromFile(&File->Handle, File->Header.AssetTypes, AssetTypeArraySize, File->AssetTypeArray);
            
            if(File->Header.MagicValue != HHA_MAGIC_VALUE)
            {
                Platform.FileError(&File->Handle, "HHA file has an invalid magic value.");
            }

            if(File->Header.Version > HHA_VERSION)
            {
                Platform.FileError(&File->Handle, "HHA file is of a later version.");
            }
            
            if(PlatformNoFileErrors(&File->Handle))
            {
                // NOTE(georgy): The first asset and tag Asset in every HHA is a null asset (reserved)
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
        Platform.GetAllFilesOfTypeEnd(&FileGroup);
    }

    // NOTE(georgy): Allocate all metadata space
    Assets->Assets = PushArray(Arena, Assets->AssetCount, asset);
    Assets->Tags = PushArray(Arena, Assets->TagCount, hha_tag);

    // NOTE(georgy): Reserve one null tag at the beginning
    ZeroStruct(Assets->Tags[0]);

    // NOTE(georgy): Load tags
    for(uint32 FileIndex = 0;
        FileIndex < Assets->FileCount;
        FileIndex++)
    {
        asset_file *File = Assets->Files + FileIndex;
        if(PlatformNoFileErrors(&File->Handle))
        {
            // NOTE(georgy): Skip the first tag, since it's null!
            uint32 TagArraySize = sizeof(hha_tag) * (File->Header.TagCount - 1);
            Platform.ReadDataFromFile(&File->Handle, File->Header.Tags + sizeof(hha_tag), TagArraySize, Assets->Tags + File->TagBase);
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
            if(PlatformNoFileErrors(&File->Handle))
            {
                for(uint32 SourceIndex = 0;
                    SourceIndex < File->Header.AssetTypeCount;
                    SourceIndex++)
                {
                    hha_asset_type *SourceType = File->AssetTypeArray + SourceIndex;

                    if(SourceType->TypeID == DestTypeID)
                    {
                        if(SourceType->TypeID == Asset_FontGlyph)
                        {
                            File->FontBitmapIDOffset = AssetCount - SourceType->FirstAssetIndex;
                        }
                        uint32 AssetCountForType = (SourceType->OnePastLastAssetIndex - SourceType->FirstAssetIndex);

                        temporary_memory TempMem = BeginTemporaryMemory(&TranState->TranArena);
                        hha_asset *HHAAssetArray = PushArray(&TranState->TranArena, AssetCountForType, hha_asset);
						
                        Platform.ReadDataFromFile(&File->Handle, 
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

inline uint32
GetGlyphFromCodePoint(hha_font *Info, loaded_font *Font, uint32 CodePoint)
{
    uint32 Result = 0;
    if(CodePoint < Info->OnePastHighestCodepoint)
    {
        Result = Font->UnicodeMap[CodePoint];
        Assert(Result < Info->GlyphCount);
    }

    return(Result);
}

internal real32
GetHorizontalAdvanceForPair(hha_font *Info, loaded_font *Font, uint32 DesiredPrevCodePoint, uint32 DesiredCodePoint)
{
    uint32 PrevGlyph = GetGlyphFromCodePoint(Info, Font, DesiredPrevCodePoint);
    uint32 Glyph = GetGlyphFromCodePoint(Info, Font, DesiredCodePoint);
    real32 Result = Font->HorizontalAdvance[PrevGlyph*Info->GlyphCount + Glyph];

    return(Result);
}

internal bitmap_id
GetBitmapForGlyph(game_assets *Assets, hha_font *Info, loaded_font *Font, uint32 DesiredCodePoint)
{
    uint32 Glyph = GetGlyphFromCodePoint(Info, Font, DesiredCodePoint);
    bitmap_id Result = Font->Glyphs[Glyph].BitmapID;
    Result.Value += Font->BitmapIDOffset;

    return(Result);
}

inline real32
GetLineAdvanceFor(hha_font *Info)
{
    real32 Result = Info->AscenderHeight + Info->DescenderHeight + Info->ExternalLeading;
    return(Result);
}

inline real32
GetStartingBaselineY(hha_font *Info)
{
    real32 Result = Info->AscenderHeight;
    return(Result);
}