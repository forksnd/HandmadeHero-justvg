
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

internal v2
TopDownAlign(loaded_bitmap *Bitmap, v2 Align)
{
    Align.y = (real32)(Bitmap->Height - 1) - Align.y;

    Align.x = SafeRatio0(Align.x, (real32)Bitmap->Width);
    Align.y = SafeRatio0(Align.y, (real32)Bitmap->Height);

    return (Align);
}

internal loaded_bitmap
DEBUGLoadBMP(char *Filename, int32 AlignX, int32 TopDownAlignY)
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
        Result.AlignPercentage = TopDownAlign(&Result, V2i(AlignX, TopDownAlignY));
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

internal loaded_bitmap
DEBUGLoadBMP(char *Filename)
{
    loaded_bitmap Result = DEBUGLoadBMP(Filename, 0, 0);
    Result.AlignPercentage = V2(0.5f, 0.5f);
    return(Result);
}

struct load_bitmap_work
{
    game_assets *Assets;
    bitmap_id ID;
    char *Filename;
    loaded_bitmap *Bitmap;
    task_with_memory *Task;

    bool32 HasAlignment;
    int32 AlignX;
    int32 TopDownAlignY;

    asset_state FinalState;
};
internal PLATFORM_WORK_QUEUE_CALLBACK(LoadBitmapWork)
{
    load_bitmap_work *Work = (load_bitmap_work *)Data;

    // TODO(georgy): Get rid of this thread thing when I load through a queue instead of the debug call.
    if(Work->HasAlignment)
    {
        *Work->Bitmap = DEBUGLoadBMP(Work->Filename, Work->AlignX, Work->TopDownAlignY);
    }
    else
    {
        *Work->Bitmap = DEBUGLoadBMP(Work->Filename);
    }

    CompletePreviousWritesBeforeFutureWrites;

    Work->Assets->Bitmaps[Work->ID.Value].Bitmap = Work->Bitmap;
    Work->Assets->Bitmaps[Work->ID.Value].State = Work->FinalState;

    EndTaskWithMemory(Work->Task);
}

internal void
LoadBitmap(game_assets *Assets, bitmap_id ID)
{
    if((ID.Value) && (AtomicCompareExchangeUInt32((uint32 *)&Assets->Bitmaps[ID.Value].State, AssetState_Unloaded, AssetState_Queued) ==
       AssetState_Unloaded))   
    {
        task_with_memory *Task = BeginTaskWithMemory(Assets->TranState);
        if(Task)
        {
            load_bitmap_work *Work = PushStruct(&Task->Arena, load_bitmap_work);
            Work->Assets = Assets;
            Work->ID = ID;
            Work->Filename = "";
            Work->Bitmap = PushStruct(&Assets->Arena, loaded_bitmap);
            Work->Task = Task;
            Work->HasAlignment = false;
            Work->FinalState = AssetState_Loaded;

            switch(ID.Value)
            {
                case Asset_Backdrop:
                {
                    Work->Filename = "test/test_background.bmp";
                } break;

                case Asset_Shadow:
                {
                    Work->Filename = "test/hero_shadow.bmp";
                    Work->HasAlignment = true;
                    Work->AlignX = 20;
                    Work->TopDownAlignY = 12;
                } break;

                case Asset_Tree:
                {
                    Work->Filename = "test/tree00.bmp";
                    Work->HasAlignment = true;
                    Work->AlignX = 40;
                    Work->TopDownAlignY = 55;
                } break;

                case Asset_Sword:
                {
                    Work->Filename = "test/sword1.bmp";
                    Work->HasAlignment = true;
                    Work->AlignX = 7;
                    Work->TopDownAlignY = 12;
                } break;

                case Asset_Stairwell:
                {
                    Work->Filename = "test/stairwellup.bmp";
                } break;
            }

            PlatformAddEntry(Assets->TranState->LowPriorityQueue, LoadBitmapWork, Work);
        }
    }    
}

internal void 
LoadSound(game_assets *Assets, uint32 ID)
{
	
}

internal bitmap_id
GetFirstBitmapID(game_assets *Assets, asset_type_id TypeID)
{
    bitmap_id Result = {};

    asset_type *Type = Assets->AssetTypes + TypeID;
    if(Type->FirstAssetIndex != Type->OnePastLastAssetIndex)
    {
        asset *Asset = Assets->Assets + Type->FirstAssetIndex;
        Result.Value = Asset->SlotID;
    }

    return(Result);
}

internal game_assets *
AllocateGameAssets(memory_arena *Arena, memory_index Size, transient_state *TranState)
{
	game_assets *Assets = PushStruct(Arena, game_assets);
	SubArena(&Assets->Arena, Arena, Size);
	Assets->TranState = TranState;

    Assets->BitmapCount = Asset_Count;
    Assets->Bitmaps = PushArray(Arena, Assets->BitmapCount, asset_slot);

    Assets->SoundCount = 1;
    Assets->Sounds = PushArray(Arena, Assets->SoundCount, asset_slot);

    Assets->TagCount = 0;
    Assets->Tags = 0;

    Assets->AssetCount = Assets->BitmapCount;
    Assets->Assets = PushArray(Arena, Assets->AssetCount, asset);

    for(uint32 AssetID = 0;
        AssetID < Asset_Count;
        AssetID++)
    {
        asset_type *Type = Assets->AssetTypes + AssetID;
        Type->FirstAssetIndex = AssetID;
        Type->OnePastLastAssetIndex = AssetID + 1;

        asset *Asset = Assets->Assets + Type->FirstAssetIndex;
        Asset->FirstTagIndex = 0;
        Asset->OnePastLastTagIndex = 0;
        Asset->SlotID = Type->FirstAssetIndex;
    }

    Assets->Stone[0] = DEBUGLoadBMP("test/stone3.bmp");
    Assets->Grass[0] = DEBUGLoadBMP("test/grass3.bmp");            

    hero_bitmaps *Bitmap;
    Bitmap = Assets->HeroBitmaps;
    Bitmap->Hero = DEBUGLoadBMP("test/hero_right.bmp", 18, 56);
    Bitmap++;

    Bitmap->Hero = DEBUGLoadBMP("test/hero_back.bmp", 18, 56);
    Bitmap++;

    Bitmap->Hero = DEBUGLoadBMP("test/hero_left.bmp", 18, 56);
    Bitmap++;

    Bitmap->Hero = DEBUGLoadBMP("test/hero_front.bmp", 18, 56);

    return(Assets);
}
