#include "test_asset_builder.h"

#define USE_FONTS_FROM_WINDOWS 1

#if USE_FONTS_FROM_WINDOWS
#include <Windows.h>
#else
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#endif

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

struct loaded_bitmap
{
    int32 Width;
    int32 Height;
    int32 Pitch;
    void *Memory;

    void *Free;
};

struct entire_file
{
    uint32 ContentsSize;
    void *Contents;
};
internal entire_file
ReadEntireFile(char *Filename)
{
    entire_file Result = {};

    FILE *In = fopen(Filename, "rb");
    if(In)
    {
        fseek(In, 0, SEEK_END);
        Result.ContentsSize = ftell(In);
        fseek(In, 0, SEEK_SET);

        Result.Contents = malloc(Result.ContentsSize);
        fread(Result.Contents, Result.ContentsSize, 1, In);
        fclose(In);
    }
    else
    {
        printf("ERROR: Cannot open file %s.\n", Filename);
    }

    return(Result);
}

internal loaded_bitmap
LoadBMP(char *Filename)
{
    loaded_bitmap Result = {};

    entire_file ReadResult = ReadEntireFile(Filename);
    if(ReadResult.ContentsSize != 0)
    {
        Result.Free = ReadResult.Contents;

        bitmap_header *Header = (bitmap_header *)ReadResult.Contents;
        uint32 *Pixels = (uint32 *)((uint8 *)ReadResult.Contents + Header->BitmapOffset);
        Result.Memory = Pixels;
        Result.Width = Header->Width;
        Result.Height = Header->Height;

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

struct loaded_sound
{
    uint32 SampleCount;
    uint32 ChannelCount;
    int16 *Samples[2];

    void *Free;
};

internal loaded_sound
LoadWAV(char *Filename, uint32 SectionFirstSampleIndex, uint32 SectionSampleCount)
{
    loaded_sound Result = {};

    entire_file ReadResult = ReadEntireFile(Filename);
    if(ReadResult.ContentsSize != 0)
    {
        Result.Free = ReadResult.Contents;

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
                int16 Source = SampleData[2*SampleIndex];
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
#if 0
                for(uint32 SampleIndex = SampleCount;
                    SampleIndex < (SampleCount + 4);
                    SampleIndex++)
                {
                    Result.Samples[ChannelIndex][SampleIndex] = 0;
                }
#endif
            }
        }

        Result.SampleCount = SampleCount;
    }

    return(Result);
}

internal loaded_bitmap
LoadGlyphBitmap(char *Filename, char *FontName, uint32 Codepoint)
{
    loaded_bitmap Result = {};

#if USE_FONTS_FROM_WINDOWS
    int MaxWidth = 1024;
    int MaxHeight = 1024;
    static HDC DeviceContext = 0;
    static VOID *Bits = 0;
    if(!DeviceContext)
    {
        AddFontResourceEx(Filename, FR_PRIVATE, 0);
        int Height = 128; // TODO(georgy): Figure out how to specify pixels properly here
        HFONT Font = CreateFontA(Height, 0, 0, 0, 
                     FW_NORMAL, // NOTE(georgy): Weight
                     FALSE, // NOTE(georgy): Italic
                     FALSE, // NOTE(georgy): Underline
                     FALSE, // NOTE(georgy): StrikeOut,
                     DEFAULT_CHARSET,
                     OUT_DEFAULT_PRECIS,
                     CLIP_DEFAULT_PRECIS,
                     ANTIALIASED_QUALITY,
                     DEFAULT_PITCH|FF_DONTCARE,
                     FontName);

        DeviceContext = CreateCompatibleDC(GetDC(0));

        BITMAPINFO Info = {};
        Info.bmiHeader.biSize = sizeof(Info.bmiHeader);
        Info.bmiHeader.biWidth = MaxWidth;
        Info.bmiHeader.biHeight = MaxHeight;
        Info.bmiHeader.biPlanes = 1;
        Info.bmiHeader.biBitCount = 32;
        Info.bmiHeader.biCompression = BI_RGB;
        Info.bmiHeader.biSizeImage = 0;
        Info.bmiHeader.biXPelsPerMeter = 0;
        Info.bmiHeader.biYPelsPerMeter = 0;
        Info.bmiHeader.biClrUsed = 0;
        Info.bmiHeader.biClrImportant = 0;
        HBITMAP Bitmap = CreateDIBSection(DeviceContext, &Info, DIB_RGB_COLORS, &Bits, 0, 0);
        SelectObject(DeviceContext, Bitmap);
        SelectObject(DeviceContext, Font);
        SetBkColor(DeviceContext, RGB(0, 0, 0));

        TEXTMETRIC TextMetric;
        GetTextMetrics(DeviceContext, &TextMetric);
    } 

    wchar_t CheesePoint = (wchar_t)Codepoint;

    SIZE Size;
    GetTextExtentPoint32W(DeviceContext, &CheesePoint, 1, &Size);

    int Width = Size.cx;
    if(Width > MaxWidth)
    {
        Width = MaxWidth;
    }
    int Height = Size.cy;
    if(Height > MaxHeight)
    {
        Height = MaxHeight;
    }

    // PatBlt(DeviceContext, 0, 0, Width, Height, BLACKNESS);
    SetTextColor(DeviceContext, RGB(255, 255, 255));
    TextOutW(DeviceContext, 0, 0, &CheesePoint, 1);

    int32 MinX = 10000;
    int32 MinY = 10000;
    int32 MaxX = -10000;
    int32 MaxY = -10000;

    uint32 *Row = (uint32 *)Bits + (MaxHeight - 1)*MaxWidth;
    for(int32 Y = 0;
        Y < Height;
        Y++)
    {
        uint32 *Pixel = Row;
        for(int32 X = 0;
            X < Width;
            X++)
        {
            if (*Pixel != 0)
            {
                if(MinX > X)
                {
                    MinX = X;
                }
                if(MinY > Y)
                {
                    MinY = Y;
                }
                if(MaxX < X)
                {
                    MaxX = X;
                }
                if(MaxY < Y)
                {
                    MaxY = Y;
                }
            }

            Pixel++;
        }
        Row -= MaxWidth;
    }

    if(MinX <= MaxX)
    {
#if 0
        MinX--;
        MinY--;
        MaxX++;
        MaxY++;
#endif 
        int Width = (MaxX - MinX) + 1;
        int Height = (MaxY - MinY) + 1;

        Result.Width = Width + 2;
        Result.Height = Height + 2;
        Result.Pitch = Result.Width*BITMAP_BYTES_PER_PIXEL;
        Result.Memory = malloc(Result.Height*Result.Pitch);
        Result.Free = Result.Memory;

        memset(Result.Memory, 0, Result.Height*Result.Pitch);

        uint8 *DestRow = (uint8 *)Result.Memory + (Result.Height - 1)*Result.Pitch;
        uint32 *SourceRow = (uint32 *)Bits + (MaxHeight - 1 - MinY)*MaxWidth;
        for(int32 Y = MinY;
            Y <= MaxY;
            Y++)
        {
            uint32 *Source = (uint32 *)SourceRow + MinX;
            uint32 *Dest = (uint32 *)DestRow + 1;
            for(int32 X = MinX;
                X <= MaxX;
                X++)
            {
#if 0
                COLORREF Pixel = GetPixel(DeviceContext, X, Y);
                Assert(Pixel == *Source);
#endif
                uint32 Pixel = *Source++;
                uint8 Gray = (uint8)(Pixel & 0xFF);
                uint8 Alpha = Gray;
                *Dest++ = ((Alpha << 24) | 
                          (Gray << 16) |
                          (Gray <<  8) | 
                          (Gray <<  0));
            }
            DestRow -= Result.Pitch;
            SourceRow -= MaxWidth;
        }
    }
#else
    entire_file TTFFile = ReadEntireFile(Filename);
    if(TTFFile.ContentsSize != 0)
    {
        stbtt_fontinfo Font;    
        stbtt_InitFont(&Font, (uint8 *)TTFFile.Contents, stbtt_GetFontOffsetForIndex((uint8 *)TTFFile.Contents, 0));

        int Width, Height, XOffset, YOffset;
        uint8 *MonoBitmap = stbtt_GetCodepointBitmap(&Font, 0, stbtt_ScaleForPixelHeight(&Font, 128.0f),
                                                     Codepoint, &Width, &Height, &XOffset, &YOffset);

        Result.Width = Width;
        Result.Height = Height;
        Result.Pitch = Result.Width*BITMAP_BYTES_PER_PIXEL;
        Result.Memory = malloc(Result.Height*Result.Pitch);
        Result.Free = Result.Memory;

        uint8 *Source = MonoBitmap;
        uint8 *DestRow = (uint8 *)Result.Memory + (Height - 1)*Result.Pitch;
        for(int32 Y = 0;
            Y < Height;
            Y++)
        {
            uint32 *Dest = (uint32 *)DestRow;
            for(int32 X = 0;
                X < Width;
                X++)
            {
                uint8 Grey = *Source++;
                uint8 Alpha = 0xFF;
                *Dest++ = ((Alpha << 24) | 
                           (Grey << 16) |
                           (Grey <<  8) | 
                           (Grey <<  0));
            }
            DestRow -= Result.Pitch;
        }

        stbtt_FreeBitmap(MonoBitmap, 0);
        free(TTFFile.Contents);
    }
#endif

    return(Result);
}

internal void
BeginAssetType(game_assets *Assets, asset_type_id TypeID)
{
    Assert(Assets->DEBUGAssetType == 0);

    Assets->DEBUGAssetType = Assets->AssetTypes + TypeID;
    Assets->DEBUGAssetType->TypeID = TypeID;
    Assets->DEBUGAssetType->FirstAssetIndex = Assets->AssetCount;
    Assets->DEBUGAssetType->OnePastLastAssetIndex = Assets->DEBUGAssetType->FirstAssetIndex;
}

internal bitmap_id
AddBitmapAsset(game_assets *Assets, char *Filename, real32 AlignPercentageX = 0.5f, real32  AlignPercentageY = 0.5f)
{
    Assert(Assets->DEBUGAssetType);
    Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex <= ArrayCount(Assets->Assets));

    bitmap_id Result = {Assets->DEBUGAssetType->OnePastLastAssetIndex++};
    asset_source *Source = Assets->AssetsSource + Result.Value;
    hha_asset *HHA = Assets->Assets + Result.Value;
    HHA->FirstTagIndex = Assets->TagCount;
    HHA->OnePastLastTagIndex = HHA->FirstTagIndex;
    HHA->Bitmap.AlignPercentage[0] = AlignPercentageX;
    HHA->Bitmap.AlignPercentage[1] = AlignPercentageY;

    Source->Type = AssetType_Bitmap;
    Source->Filename = Filename;

    Assets->AssetIndex = Result.Value;

    return(Result);
}

internal bitmap_id
AddCharacterAsset(game_assets *Assets, char *FontFile, char *FontName, uint32 Codepoint, real32 AlignPercentageX = 0.5f, real32  AlignPercentageY = 0.5f)
{
    Assert(Assets->DEBUGAssetType);
    Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex <= ArrayCount(Assets->Assets));

    bitmap_id Result = {Assets->DEBUGAssetType->OnePastLastAssetIndex++};
    asset_source *Source = Assets->AssetsSource + Result.Value;
    hha_asset *HHA = Assets->Assets + Result.Value;
    HHA->FirstTagIndex = Assets->TagCount;
    HHA->OnePastLastTagIndex = HHA->FirstTagIndex;
    HHA->Bitmap.AlignPercentage[0] = AlignPercentageX;
    HHA->Bitmap.AlignPercentage[1] = AlignPercentageY;

    Source->Type = AssetType_Font;
    Source->Filename = FontFile;
    Source->FontName = FontName;
    Source->Codepoint = Codepoint;

    Assets->AssetIndex = Result.Value;

    return(Result);
}

internal sound_id
AddSoundAsset(game_assets *Assets, char *Filename, uint32 FirstSampleIndex = 0, uint32 SampleCount = 0)
{
    Assert(Assets->DEBUGAssetType);
    Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < ArrayCount(Assets->Assets));

    sound_id Result = {Assets->DEBUGAssetType->OnePastLastAssetIndex++};
    asset_source *Source = Assets->AssetsSource + Result.Value;
    hha_asset *HHA = Assets->Assets + Result.Value;
    HHA->FirstTagIndex = Assets->TagCount;
    HHA->OnePastLastTagIndex = HHA->FirstTagIndex;
    HHA->Sound.SampleCount = SampleCount;
    HHA->Sound.Chain = HHASoundChain_None;

    Source->Type = AssetType_Sound;
    Source->Filename = Filename;
    Source->FirstSampleIndex = FirstSampleIndex;
    
    Assets->AssetIndex = Result.Value;

    return(Result);
}

internal void
AddTag(game_assets *Assets, asset_tag_id ID, real32 Value)
{
    Assert(Assets->AssetIndex);

    hha_asset *HHA = Assets->Assets + Assets->AssetIndex;
    HHA->OnePastLastTagIndex++;
    hha_tag *Tag = Assets->Tags + Assets->TagCount++;

    Tag->ID = ID;
    Tag->Value = Value;
}

internal void
EndAssetType(game_assets *Assets)
{
    Assert(Assets->DEBUGAssetType);
    Assets->AssetCount = Assets->DEBUGAssetType->OnePastLastAssetIndex;
    Assets->DEBUGAssetType = 0;
    Assets->AssetIndex = 0;
}

internal void
WriteHHA(game_assets *Assets, char *Filename)
{
	FILE *Out = fopen(Filename, "wb");
    if(Out)
	{
		hha_header Header = {};
        Header.MagicValue = HHA_MAGIC_VALUE;
        Header.Version = HHA_VERSION;
        Header.TagCount = Assets->TagCount;
        Header.AssetTypeCount = Asset_Count; // TODO(georgy): Do we really want to do this? Sparseness!
        Header.AssetCount = Assets->AssetCount;

        uint32 TagArraySize = Header.TagCount*sizeof(hha_tag);
        uint32 AssetTypeArraySize = Header.AssetTypeCount*sizeof(hha_asset_type);
        uint32 AssetArraySize = Header.AssetCount*sizeof(hha_asset);

        Header.Tags = sizeof(Header); 
        Header.AssetTypes = Header.Tags + TagArraySize;
        Header.Assets = Header.AssetTypes + AssetTypeArraySize; 

        fwrite(&Header, sizeof(Header), 1, Out);
        fwrite(Assets->Tags, TagArraySize, 1, Out);
        fwrite(Assets->AssetTypes, AssetTypeArraySize, 1, Out);
        fseek(Out, AssetArraySize, SEEK_CUR);
        for(uint32 AssetIndex = 1;
            AssetIndex < Header.AssetCount;
            AssetIndex++)
        {
            asset_source *Source = Assets->AssetsSource + AssetIndex;
            hha_asset *Dest = Assets->Assets + AssetIndex;

            Dest->DataOffset = ftell(Out);

            if(Source->Type == AssetType_Sound)
            {
                loaded_sound WAV = LoadWAV(Source->Filename, 
                                           Source->FirstSampleIndex, 
                                           Dest->Sound.SampleCount);
                Dest->Sound.SampleCount = WAV.SampleCount;
                Dest->Sound.ChannelCount = WAV.ChannelCount;

                for(uint32 ChannelIndex = 0;
                    ChannelIndex < WAV.ChannelCount;
                    ChannelIndex++)
                {
                    fwrite(WAV.Samples[ChannelIndex], Dest->Sound.SampleCount*sizeof(int16), 1, Out);
                }

                free(WAV.Free);
            }
            else
            {
                loaded_bitmap Bitmap;
                if(Source->Type == AssetType_Font)
                {
                    Bitmap = LoadGlyphBitmap(Source->Filename, Source->FontName, Source->Codepoint);
                }
                else
                {
                    Assert(Source->Type == AssetType_Bitmap);
                    Bitmap = LoadBMP(Source->Filename);
                }

                Dest->Bitmap.Dim[0] = Bitmap.Width;
                Dest->Bitmap.Dim[1] = Bitmap.Height;

                Assert((Bitmap.Width*4) == Bitmap.Pitch);
                fwrite(Bitmap.Memory, Bitmap.Width*Bitmap.Height*4, 1, Out);
                
                free(Bitmap.Free);
            }
        }
        fseek(Out, (uint32)Header.Assets, SEEK_SET);
        fwrite(Assets->Assets, AssetArraySize, 1, Out);

		fclose(Out);
	}
    else
    {
        printf("ERROR: Couldn't open file!\n");
    }
}

internal void
Initialize(game_assets *Assets)
{
    Assets->TagCount = 1;
    Assets->AssetCount = 1;
    Assets->DEBUGAssetType = 0; 
    Assets->AssetIndex = 0;

    Assets->AssetTypeCount = Asset_Count;
    memset(Assets->AssetTypes, 0, sizeof(Assets->AssetTypes));
}

internal void
WriteHero(void)
{
    game_assets Assets_;
    game_assets *Assets = &Assets_;
    Initialize(Assets);

    real32 AngleRight = 0.0f*Pi32;
    real32 AngleBack = 0.5f*Pi32;
    real32 AngleLeft = 1.0f*Pi32;
    real32 AngleFront = 1.5f*Pi32;

    real32 HeroAlign[] = {0.461538464f , 0.0f};

    BeginAssetType(Assets, Asset_Head);
    AddBitmapAsset(Assets, "test/hero_right_head.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleRight);
    AddBitmapAsset(Assets, "test/hero_back_head.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleBack);
    AddBitmapAsset(Assets, "test/hero_left_head.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleLeft);
    AddBitmapAsset(Assets, "test/hero_front_head.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleFront);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Torso);
    AddBitmapAsset(Assets, "test/hero_right_torso.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleRight);
    AddBitmapAsset(Assets, "test/hero_back_torso.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleBack);
    AddBitmapAsset(Assets, "test/hero_left_torso.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleLeft);
    AddBitmapAsset(Assets, "test/hero_front_torso.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleFront);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Legs);
    AddBitmapAsset(Assets, "test/hero_right_legs.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleRight);
    AddBitmapAsset(Assets, "test/hero_back_legs.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleBack);
    AddBitmapAsset(Assets, "test/hero_left_legs.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleLeft);
    AddBitmapAsset(Assets, "test/hero_front_legs.bmp", HeroAlign[0], HeroAlign[1]);
    AddTag(Assets, Tag_FacingDirection, AngleFront);
    EndAssetType(Assets);

	WriteHHA(Assets, "test1.hha");
}


internal void
WriteSounds(void)
{
    game_assets Assets_;
    game_assets *Assets = &Assets_;
    Initialize(Assets);

    uint32 OneMusicChunk = 10*48000;
    uint32 TotalMusicSampleCount = 8640540;
    BeginAssetType(Assets, Asset_Music);
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
        if((FirstSampleIndex + OneMusicChunk) < TotalMusicSampleCount)
        {
            Assets->Assets[ThisMusic.Value].Sound.Chain = HHASoundChain_Advance;
        }
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

	WriteHHA(Assets, "test2.hha");
}

internal void
WriteNonHero(void)
{
    game_assets Assets_;
    game_assets *Assets = &Assets_;
    Initialize(Assets);

    BeginAssetType(Assets, Asset_Shadow);
    AddBitmapAsset(Assets, "test/hero_shadow.bmp", 0.5f, 0.409090906f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Tree);
    AddBitmapAsset(Assets, "test/tree00.bmp", 0.506329119f, 0.282051295f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Sword);
    AddBitmapAsset(Assets, "test/sword1.bmp", 0.333333343f, 0.566666663f);
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Stone);
    AddBitmapAsset(Assets, "test/stone3.bmp");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Grass);
    AddBitmapAsset(Assets, "test/grass3.bmp");
    EndAssetType(Assets);

    BeginAssetType(Assets, Asset_Font);
    for(uint32 Character = 'A';
        Character <= 'Z';
        Character++)
    {
        // AddCharacterAsset(Assets, "C:/Windows/Fonts/arial.ttf", "Arial", Character);
        AddCharacterAsset(Assets, "C:/Windows/Fonts/cour.ttf", "Courier New", Character);
        AddTag(Assets, Tag_UnicodeCodepoint, (real32)Character);
    }
    EndAssetType(Assets);

	WriteHHA(Assets, "test3.hha");
}

int 
main(int ArgCount, char **Args)
{
    WriteHero();
    WriteSounds();
    WriteNonHero();

	return(0);
}