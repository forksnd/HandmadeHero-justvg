#if 0

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

struct WAVE_header
{
    uint32 RIFFID;
    uint32 Size;
    uint32 WAVEID;
};

#define RIFF_CODE(a, b, c, d) (((uint32)(a) << 0) | ((uint32)(b) << 8) | ((uint32)(c) << 16) | ((uint32)(d) << 24))
enum 
{
    WAVE_ChunkID_fmt = RIFF_CODE('f', 'm', 't', ' '),
    WAVE_ChunkID_data = RIFF_CODE('d', 'a', 't', 'a'),
    WAVE_ChunkID_RIFF = RIFF_CODE('R', 'I', 'F', 'F'),
    WAVE_ChunkID_WAVE = RIFF_CODE('W', 'A', 'V', 'E'),
};
struct WAVE_chunk
{
    uint32 ID;
    uint32 Size;
};

struct WAVE_fmt
{
    uint16 wFormatTag;
    uint16 nChannels;
    uint32 nSamplesPerSec;
    uint32 nAvgBytesPerSec;
    uint16 nBlockAlign;
    uint16 wBitsPerSample;
    uint16 cbSize;
    uint16 wValidBitsPerSample;
    uint32 dwChannelMask;
    uint8 SubFormat[16];
};
#pragma pack(pop)

internal loaded_bitmap
DEBUGLoadBMP(char *Filename, v2 AlignPercentage = V2(0.5f, 0.5f))
{
    loaded_bitmap Result = {};

    debug_read_file_result ReadResult = DEBUGPlatformReadEntireFile(Filename);
    if(ReadResult.ContentsSize != 0)
    {
        bitmap_header *Header = (bitmap_header *)ReadResult.Contents;
        uint32 *Pixels = (uint32 *)((uint8 *)ReadResult.Contents + Header->BitmapOffset);
        Result.Memory = Pixels;
        Result.Width = Header->Width;
        Result.Height = Header->Height;
        Result.AlignPercentage = AlignPercentage;
        Result.WidthOverHeight = SafeRatio0((real32)Result.Width, (real32)Result.Height);

        Assert(Result.Height >= 0);
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

    Result.Pitch = Result.Width*BITMAP_BYTES_PER_PIXEL;

#if 0
    Result.Pitch = -Result.Width*BITMAP_BYTES_PER_PIXEL;
    Result.Memory = (uint8*)Result.Memory - Result.Pitch*(Result.Height-1);
#endif

    return(Result);
}

struct riff_iterator
{
    uint8 *At;
    uint8 *Stop;
};

inline riff_iterator
ParseChunkAt(void *At, void *Stop)
{
    riff_iterator Iter;

    Iter.At = (uint8 *)At;
    Iter.Stop = (uint8 *)Stop;

    return(Iter);
}

inline riff_iterator
NextChunk(riff_iterator Iter)
{
    WAVE_chunk *Chunk = (WAVE_chunk *)Iter.At;
    uint32 Size = (Chunk->Size + 1) & ~1;
    Iter.At += sizeof(WAVE_chunk) + Size;

    return(Iter);
}

inline bool32
IsValid(riff_iterator Iter)
{
    bool32 Result = (Iter.At < Iter.Stop);

    return(Result);
}

inline void *
GetChunkData(riff_iterator Iter)
{
    void *Result = (Iter.At + sizeof(WAVE_chunk));

    return(Result);
}

inline uint32
GetChunkDataSize(riff_iterator Iter)
{
    WAVE_chunk *Chunk = (WAVE_chunk *)Iter.At;
    uint32 Result = Chunk->Size;

    return(Result);
}

inline uint32
GetType(riff_iterator Iter)
{
    WAVE_chunk *Chunk = (WAVE_chunk *)Iter.At;
    uint32 Result = Chunk->ID;

    return(Result);
}

internal loaded_sound
DEBUGLoadWAV(char *Filename, uint32 SectionFirstSampleIndex, uint32 SectionSampleCount)
{
    loaded_sound Result = {};

    debug_read_file_result ReadResult = DEBUGPlatformReadEntireFile(Filename);
    if(ReadResult.ContentsSize != 0)
    {
        WAVE_header *Header = (WAVE_header *)ReadResult.Contents;
        Assert(Header->RIFFID == WAVE_ChunkID_RIFF);
        Assert(Header->WAVEID == WAVE_ChunkID_WAVE);

        uint32 ChannelCount = 0;
        uint32 SampleDataSize = 0;
        int16 *SampleData = 0;
        for(riff_iterator Iter = ParseChunkAt(Header + 1, (uint8 *)(Header + 1) + Header->Size - 4);
            IsValid(Iter);
            Iter = NextChunk(Iter))
        {
            switch(GetType(Iter))
            {
                case WAVE_ChunkID_fmt:
                {
                    WAVE_fmt *fmt = (WAVE_fmt *)GetChunkData(Iter);
                    Assert(fmt->wFormatTag == 1); // NOTE(georgy): Only support PCM
                    // Assert(fmt->nSamplesPerSec == 48000);
                    // Assert(fmt->nSamplesPerSec == 44100);
                    Assert((fmt->nSamplesPerSec == 48000) || (fmt->nSamplesPerSec == 44100));
                    Assert(fmt->wBitsPerSample == 16);
                    Assert(fmt->nBlockAlign == (sizeof(int16)*fmt->nChannels));
                    ChannelCount = fmt->nChannels;
                } break;

                case WAVE_ChunkID_data:
                {
                    SampleData = (int16 *)GetChunkData(Iter);
                    SampleDataSize = GetChunkDataSize(Iter);
                } break;
            }
        }

        Assert(ChannelCount && SampleData);

        uint32 SampleCount = SampleDataSize / (ChannelCount*sizeof(int16));
        Result.ChannelCount = ChannelCount;
        if(ChannelCount == 1)
        {
            Result.Samples[0] = SampleData;
            Result.Samples[1] = 0;
        }
        else if(ChannelCount == 2)
        {
            Result.Samples[0] = SampleData;
            Result.Samples[1] = SampleData + SampleCount;

            for(uint32 SampleIndex = 0;
                SampleIndex < SampleCount;
                SampleIndex++)
            {
                uint16 Source = SampleData[2*SampleIndex];
                SampleData[2*SampleIndex] = SampleData[SampleIndex];
                SampleData[SampleIndex] = Source;
            }
        }
        else
        {
            Assert(!"Invalid channel count in WAV file");
        }

        // TODO(georgy): Load right channels!
        bool32 AtEnd = true;
        Result.ChannelCount = 1;
        if(SectionSampleCount)
        {
            Assert((SectionFirstSampleIndex + SectionSampleCount) <= SampleCount);
            AtEnd = ((SectionFirstSampleIndex + SectionSampleCount) == SampleCount);
            SampleCount = SectionSampleCount;
            for(uint32 ChannelIndex = 0;
                ChannelIndex < Result.ChannelCount;
                ChannelIndex++)
            {
                Result.Samples[ChannelIndex] += SectionFirstSampleIndex;
            }
        }
        
        if(AtEnd)
        {
            for(uint32 ChannelIndex = 0;
                ChannelIndex < Result.ChannelCount;
                ChannelIndex++)
            {   
                // IMPORTANT(georgy): WE'RE WRITING OUT OF BOUNDS HERE!
                for(uint32 SampleIndex = SampleCount;
                    SampleIndex < (SampleCount + 4);
                    SampleIndex++)
                {
                    Result.Samples[ChannelIndex][SampleIndex] = 0;
                }
            }
        }

        Result.SampleCount = SampleCount;
    }

    return(Result);
}
#endif

struct load_asset_work
{
    task_with_memory *Task;
    asset_slot *Slot;

    platform_file_handle *Handle;
    uint64 Offset;
    uint64 Size;
    void *Destination;

    asset_state FinalState;
};
internal PLATFORM_WORK_QUEUE_CALLBACK(LoadAssetWork)
{
    load_asset_work *Work = (load_asset_work *)Data;

#if 0
    Platform.ReadDataFromFile(Work->Handle, Work->Offset, Work->Size, Work->Destination);
#endif

    CompletePreviousWritesBeforeFutureWrites;

    // TODO(georgy): Should we actually fill in bogus data here and set to final state anyway?
#if 0
    if(PlatformNoFileErrors(Work->Handle))
#endif    
    {
        Work->Slot->State = Work->FinalState;
    }

    EndTaskWithMemory(Work->Task);
}

#if 0
struct load_bitmap_work
{
    game_assets *Assets;
    bitmap_id ID;
    loaded_bitmap *Bitmap;
    task_with_memory *Task;

    asset_state FinalState;
};
internal PLATFORM_WORK_QUEUE_CALLBACK(LoadBitmapWork)
{
    load_bitmap_work *Work = (load_bitmap_work *)Data;

    hha_asset *HHAAsset = &Work->Assets->Assets[Work->ID.Value];
    hha_bitmap *Info = &HHAAsset->Bitmap;
    loaded_bitmap *Bitmap = Work->Bitmap;

    Bitmap->AlignPercentage = V2(Info->AlignPercentage[0], Info->AlignPercentage[1]);
    Bitmap->WidthOverHeight = (real32)Info->Dim[0] / (real32)Info->Dim[1];
    Bitmap->Width = Info->Dim[0];
    Bitmap->Height = Info->Dim[1];
    Bitmap->Pitch = 4*Info->Dim[0];
    Bitmap->Memory = Work->Assets->HHAContents + HHAAsset->DataOffset;


    CompletePreviousWritesBeforeFutureWrites;

    Work->Assets->Slots[Work->ID.Value].Bitmap = Work->Bitmap;
    Work->Assets->Slots[Work->ID.Value].State = Work->FinalState;

    EndTaskWithMemory(Work->Task);
}
#endif

internal void
LoadBitmap(game_assets *Assets, bitmap_id ID)
{
    if((ID.Value) && (AtomicCompareExchangeUInt32((uint32 *)&Assets->Slots[ID.Value].State, AssetState_Queued, AssetState_Unloaded) ==
       AssetState_Unloaded))   
    {
        task_with_memory *Task = BeginTaskWithMemory(Assets->TranState);
        if(Task)
        {
            hha_asset *HHAAsset = Assets->Assets + ID.Value;
            hha_bitmap *Info = &HHAAsset->Bitmap;
            loaded_bitmap *Bitmap = PushStruct(&Assets->Arena, loaded_bitmap);

            Bitmap->AlignPercentage = V2(Info->AlignPercentage[0], Info->AlignPercentage[1]);
            Bitmap->WidthOverHeight = (real32)Info->Dim[0] / (real32)Info->Dim[1];
            Bitmap->Width = Info->Dim[0];
            Bitmap->Height = Info->Dim[1];
            Bitmap->Pitch = 4*Info->Dim[0];
            uint32 MemorySize = Bitmap->Pitch*Bitmap->Height;
            Bitmap->Memory = PushSize(&Assets->Arena, MemorySize);

            load_asset_work *Work = PushStruct(&Task->Arena, load_asset_work);
            Work->Task = Task;
            Work->Slot =  Assets->Slots + ID.Value;
            Work->Handle = 0;
            Work->Offset = HHAAsset->DataOffset;
            Work->Size = MemorySize;
            Work->Destination = Bitmap->Memory;
            Work->FinalState = AssetState_Loaded;

            Work->Slot->Bitmap = Bitmap;

            Bitmap->Memory = Assets->HHAContents + HHAAsset->DataOffset;

            Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadAssetWork, Work);
        }
        else
        {
            Assets->Slots[ID.Value].State = AssetState_Unloaded;
        }
    }    
}

struct load_sound_work
{
    game_assets *Assets;
    sound_id ID;
    loaded_sound *Sound;
    task_with_memory *Task;

    asset_state FinalState;
};
internal PLATFORM_WORK_QUEUE_CALLBACK(LoadSoundWork)
{
    load_sound_work *Work = (load_sound_work *)Data;

    hha_asset *HHAAsset = &Work->Assets->Assets[Work->ID.Value]; 
    hha_sound *Info = &HHAAsset->Sound;
    loaded_sound *Sound = Work->Sound;

    Sound->SampleCount = Info->SampleCount;
    Sound->ChannelCount = Info->ChannelCount;
    Assert(Sound->ChannelCount < ArrayCount(Sound->Samples));
    uint64 SampleDataOffset = HHAAsset->DataOffset;
    for(uint32 ChannelIndex = 0;
        ChannelIndex < Sound->ChannelCount;
        ChannelIndex++)
    {
        Sound->Samples[ChannelIndex] = (int16 *)(Work->Assets->HHAContents + SampleDataOffset);
        SampleDataOffset += Sound->SampleCount*sizeof(int16);
    }

    CompletePreviousWritesBeforeFutureWrites;

    Work->Assets->Slots[Work->ID.Value].Sound = Work->Sound;
    Work->Assets->Slots[Work->ID.Value].State = Work->FinalState;

    EndTaskWithMemory(Work->Task);
}

internal void 
LoadSound(game_assets *Assets, sound_id ID)
{
	if((ID.Value) && (AtomicCompareExchangeUInt32((uint32 *)&Assets->Slots[ID.Value].State, AssetState_Queued, AssetState_Unloaded) ==
       AssetState_Unloaded))   
    {
        task_with_memory *Task = BeginTaskWithMemory(Assets->TranState);
        if(Task)
        {
            load_sound_work *Work = PushStruct(&Task->Arena, load_sound_work);
            Work->Assets = Assets;
            Work->ID = ID;
            Work->Sound = PushStruct(&Assets->Arena, loaded_sound);
            Work->Task = Task;
            Work->FinalState = AssetState_Loaded;

            Platform.AddEntry(Assets->TranState->LowPriorityQueue, LoadSoundWork, Work);
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
        hha_asset *Asset = Assets->Assets + AssetIndex;

        real32 TotalWeightedDiff = 0.0f;
        for(uint32 TagIndex = Asset->FirstTagIndex;
            TagIndex < Asset->OnePastLastTagIndex;
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

#if 0
internal void
BeginAssetType(game_assets *Assets, asset_type_id TypeID)
{
    Assert(Assets->DEBUGAssetType == 0);
    Assets->DEBUGAssetType = Assets->AssetTypes + TypeID;

    Assets->DEBUGAssetType->FirstAssetIndex = Assets->DEBUGUsedAssetCount;
    Assets->DEBUGAssetType->OnePastLastAssetIndex = Assets->DEBUGAssetType->FirstAssetIndex;
}

internal bitmap_id
AddBitmapAsset(game_assets *Assets, char *Filename, v2 AlignPercentage = V2(0.5f, 0.5f))
{
    Assert(Assets->DEBUGAssetType);
    Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < Assets->AssetCount);

    bitmap_id Result = {Assets->DEBUGAssetType->OnePastLastAssetIndex++};
    asset *Asset = Assets->Assets + Result.Value;
    Asset->FirstTagIndex = Assets->DEBUGUsedTagCount;
    Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
    Asset->Bitmap.Filename = PushString(&Assets->Arena, Filename);
    Asset->Bitmap.AlignPercentage = AlignPercentage;

    Assets->DEBUGAsset = Asset;

    return(Result);
}

internal sound_id
AddSoundAsset(game_assets *Assets, char *Filename, uint32 FirstSampleIndex = 0, uint32 SampleCount = 0)
{
    Assert(Assets->DEBUGAssetType);
    Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < Assets->AssetCount);

    sound_id Result = {Assets->DEBUGAssetType->OnePastLastAssetIndex++};
    asset *Asset = Assets->Assets + Result.Value;
    Asset->FirstTagIndex = Assets->DEBUGUsedTagCount;
    Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
    Asset->Sound.Filename = PushString(&Assets->Arena, Filename);
    Asset->Sound.FirstSampleIndex = FirstSampleIndex;
    Asset->Sound.SampleCount = SampleCount;
    Asset->Sound.NextIDToPlay.Value = 0;

    Assets->DEBUGAsset = Asset;

    return(Result);
}

internal void
AddTag(game_assets *Assets, asset_tag_id ID, real32 Value)
{
    Assert(Assets->DEBUGAsset);

    Assets->DEBUGAsset->OnePastLastTagIndex++;
    hha_tag *Tag = Assets->Tags + Assets->DEBUGUsedTagCount++;

    Tag->ID = ID;
    Tag->Value = Value;
}

internal void
EndAssetType(game_assets *Assets)
{
    Assert(Assets->DEBUGAssetType);
    Assets->DEBUGUsedAssetCount = Assets->DEBUGAssetType->OnePastLastAssetIndex;
    Assets->DEBUGAssetType = 0;
    Assets->DEBUGAsset = 0;
}
#endif

internal game_assets *
AllocateGameAssets(memory_arena *Arena, memory_index Size, transient_state *TranState)
{
	game_assets *Assets = PushStruct(Arena, game_assets);
	SubArena(&Assets->Arena, Arena, Size);
	Assets->TranState = TranState;

    for(uint32 TagType = 0;
        TagType < Tag_Count;
        TagType++)
    {
        Assets->TagRange[TagType] = 1000000.0f;
    }
    Assets->TagRange[Tag_FacingDirection] = 2.0f*Pi32;

    Assets->TagCount = 0;
    Assets->AssetCount = 0;
#if 0
    {
        platform_file_group FileGroup = Platform.GetAllFilesOfTypeBegin("hha");
        Assets->FileCount = FileGroup.FileCount;
        Assets->Files = PushArray(Arena, Assets->FileCount, asset_file);
        for(uint32 FileIndex = 0;
            FileIndex < Assets->FileCount;
            FileIndex++)
        {
            asset_file *File = Assets->Files + FileIndex;

            File->TagBase = Assets->TagCount;
            
            uint32 AssetTypeArraySize = File->Header.AssetTypeCount*sizeof(hha_asset_type);

            ZeroStruct(File->Header);
            File->Handle = Platform.OpenFile(FileGroup, FileIndex);
            Platform.ReadDataFromFile(File->Handle, 0, sizeof(File->Header), &File->Header);
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
                Assets->TagCount += File->Header.TagCount;
                Assets->AssetCount += File->Header.AssetCount;
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
    Assets->Assets = PushArray(Arena, Assets->AssetCount, hha_asset);
    Assets->Slots = PushArray(Arena, Assets->AssetCount, asset_slot);
    Assets->Tags = PushArray(Arena, Assets->TagCount, hha_tag);

    // NOTE(georgy): Load tags
    for(uint32 FileIndex = 0;
        FileIndex < Assets->FileCount;
        FileIndex++)
    {
        asset_file *File = Assets->Files + FileIndex;
        if(PlatformNoFileErrors(File->Handle))
        {
            uint32 TagArraySize = sizeof(hha_tag) * File->Header.TagCount;
            Platform.ReadDataFromFile(File->Handle, File->Header.Tags, TagArraySize, Assets->Tags + File->TagBase);
        }
    }

    uint32 AssetCount = 0;
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
                        Platform.ReadDataFromFile(File->Handle, 
                                                  File->Header.Assets + SourceType->FirstAssetIndex*sizeof(hha_asset), 
                                                  AssetCountForType*sizeof(hha_asset), 
                                                  Assets->Assets + AssetCount);
                        for(uint32 AssetIndex = AssetCount;
                            AssetIndex < (AssetCount + AssetCountForType);
                            AssetIndex++)
                        {
                            hha_asset *Asset = Assets->Assets + AssetIndex;
                            Asset->FirstTagIndex += File->TagBase;
                            Asset->OnePastLastTagIndex += File->TagBase;
                        }
                        AssetCount += AssetCountForType; 
                        Assert(AssetCount <= Assets->AssetCount); 
                    }
                }
            }
        }

        DestType->OnePastLastAssetIndex = AssetCount;
    }

    Assert(AssetCount == Assets->AssetCount);
#endif
#if 1
    debug_read_file_result ReadResult = Platform.DEBUGReadEntireFile("test.hha");
    if(ReadResult.ContentsSize != 0)
    {
        hha_header *Header = (hha_header *)ReadResult.Contents;

        Assets->AssetCount = Header->AssetCount; 
        Assets->Assets = (hha_asset *)((uint8 *)ReadResult.Contents + Header->Assets);
        Assets->Slots = PushArray(Arena, Assets->AssetCount, asset_slot);

        Assets->TagCount = Header->TagCount;
        Assets->Tags = (hha_tag *)((uint8 *)ReadResult.Contents + Header->Tags);

        hha_asset_type *HHAAssetTypes = (hha_asset_type *)((uint8 *)ReadResult.Contents + Header->AssetTypes);

        for(uint32 Index = 0;
            Index < Header->AssetTypeCount;
            Index++)
        {
            hha_asset_type *Source = HHAAssetTypes + Index;
            
            if(Source->TypeID < Asset_Count)
            {
                asset_type *Dest = Assets->AssetTypes + Source->TypeID;
                // TODO(georgy): Support merging!
                Assert(Dest->FirstAssetIndex == 0);
                Assert(Dest->OnePastLastAssetIndex == 0);
                Dest->FirstAssetIndex = Source->FirstAssetIndex;
                Dest->OnePastLastAssetIndex = Source->OnePastLastAssetIndex;
            }
        }

        Assets->HHAContents = (uint8 *)ReadResult.Contents;
    }
#endif

#if 0
    Assets->DEBUGUsedAssetCount = 1;
        
    BeginAssetType(Assets, Asset_Shadow);
    AddBitmapAsset(Assets, "test/hero_shadow.bmp", V2(0.5f, 0.409090906f));
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Tree);
    AddBitmapAsset(Assets, "test/tree00.bmp", V2(0.506329119f, 0.282051295f));
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Sword);
    AddBitmapAsset(Assets, "test/sword1.bmp", V2(0.333333343f, 0.566666663f));
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Stone);
    AddBitmapAsset(Assets, "test/stone3.bmp");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Grass);
    AddBitmapAsset(Assets, "test/grass3.bmp");
    EndAssetType(Assets);

    real32 AngleRight = 0.0f*Pi32;
    real32 AngleBack = 0.5f*Pi32;
    real32 AngleLeft = 1.0f*Pi32;
    real32 AngleFront = 1.5f*Pi32;

    v2 HeroAlign = {0.461538464f , 0.0f};

    BeginAssetType(Assets, Asset_Head);
    AddBitmapAsset(Assets, "test/hero_right_head.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleRight);
    AddBitmapAsset(Assets, "test/hero_back_head.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleBack);
    AddBitmapAsset(Assets, "test/hero_left_head.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleLeft);
    AddBitmapAsset(Assets, "test/hero_front_head.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleFront);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Torso);
    AddBitmapAsset(Assets, "test/hero_right_torso.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleRight);
    AddBitmapAsset(Assets, "test/hero_back_torso.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleBack);
    AddBitmapAsset(Assets, "test/hero_left_torso.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleLeft);
    AddBitmapAsset(Assets, "test/hero_front_torso.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleFront);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Legs);
    AddBitmapAsset(Assets, "test/hero_right_legs.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleRight);
    AddBitmapAsset(Assets, "test/hero_back_legs.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleBack);
    AddBitmapAsset(Assets, "test/hero_left_legs.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleLeft);
    AddBitmapAsset(Assets, "test/hero_front_legs.bmp", HeroAlign);
    AddTag(Assets, Tag_FacingDirection, AngleFront);
    EndAssetType(Assets);

    // 
    // 
    // 

    uint32 OneMusicChunk = 10*48000;
    uint32 TotalMusicSampleCount = 11236160;
    BeginAssetType(Assets, Asset_Music);
    sound_id LastMusic = {0};
    for(uint32 FirstSampleIndex = 0;
        FirstSampleIndex < TotalMusicSampleCount;
        FirstSampleIndex += OneMusicChunk)
    {
        uint32 SampleCount = TotalMusicSampleCount - FirstSampleIndex;
        if(SampleCount > OneMusicChunk)
        {
            SampleCount = OneMusicChunk;
        }
        sound_id ThisMusic = AddSoundAsset(Assets, "test2/Music.wav", FirstSampleIndex, SampleCount);
        if(IsValid(LastMusic))
        {
            Assets->Assets[LastMusic.Value].Sound.NextIDToPlay = ThisMusic;
        }
        LastMusic = ThisMusic;
    }
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Hit);
    AddSoundAsset(Assets, "test2/Hit_00.wav");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Jump);
    AddSoundAsset(Assets, "test2/Jump_00.wav");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Pickup);
    AddSoundAsset(Assets, "test2/Pickup_00.wav");
    EndAssetType(Assets);
#endif

    return(Assets);
}
