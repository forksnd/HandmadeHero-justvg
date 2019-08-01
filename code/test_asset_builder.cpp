#include <stdio.h>
#include <stdlib.h>
#include "handmade_platform.h"
#include "handmade_asset_type_id.h"

FILE *Out = 0;

struct asset_bitmap_info
{
    char *Filename;
    real32 AlignPercentage[2];
};

struct asset_sound_info
{
    char *Filename;
    uint32 FirstSampleIndex;
    uint32 SampleCount;
    uint32 NextIDToPlay;
};

struct asset_tag
{
    uint32 ID; // NOTE(georgy): Tag ID
    real32 Value;
};
struct asset
{
    uint64 DataOffset;
    uint32 FirstTagIndex;
    uint32 OnePastLastTagIndex;
};
struct asset_type
{
    uint32 FirstAssetIndex;
    uint32 OnePastLastAssetIndex;
};

#define VERY_LARGE_NUMBER 4096

uint32 BitmapCount;
uint32 SoundCount;
uint32 TagCount;
uint32 AssetCount;
asset_bitmap_info BitmapInfos[VERY_LARGE_NUMBER];
asset_sound_info SoundInfos[VERY_LARGE_NUMBER];
asset_tag Tags[VERY_LARGE_NUMBER];
asset Assets[VERY_LARGE_NUMBER];
asset_type AssetTypes[Asset_Count]; 

uint32 DEBUGUsedBitmapCount;
uint32 DEBUGUsedSoundCount;
uint32 DEBUGUsedAssetCount;   
uint32 DEBUGUsedTagCount;   
asset_type *DEBUGAssetType; 
asset *DEBUGAsset;

internal void
BeginAssetType(asset_type_id TypeID)
{
    Assert(DEBUGAssetType == 0);
    DEBUGAssetType = AssetTypes + TypeID;

    DEBUGAssetType->FirstAssetIndex = DEBUGUsedAssetCount;
    DEBUGAssetType->OnePastLastAssetIndex = DEBUGAssetType->FirstAssetIndex;
}

internal void
AddBitmapAsset(char *Filename, real32 AlignPercentageX, real32 AlignPercentageY)
{
    Assert(DEBUGAssetType);
    Assert(DEBUGAssetType->OnePastLastAssetIndex < AssetCount);

    asset *Asset = Assets + DEBUGAssetType->OnePastLastAssetIndex++;
    FirstTagIndex = DEBUGUsedTagCount;
    OnePastLastTagIndex = FirstTagIndex;
    SlotID = DEBUGAddBitmapInfo(Assets, Filename, AlignPercentageX, AlignPercentagY).Value;

/*
	internal bitmap_id
	DEBUGAddBitmapInfo(char *Filename, v2 AlignPercentage)
	{
		Assert(Assets->DEBUGUsedBitmapCount < Assets->BitmapCount);
		bitmap_id ID = {Assets->DEBUGUsedBitmapCount++};

		asset_bitmap_info *Info = Assets->BitmapInfos + ID.Value;
		Info->Filename = PushString(&Assets->Arena, Filename);
		Info->AlignPercentage = AlignPercentage;

		return(ID);
	}
*/

    DEBUGAsset = Asset;
}

internal asset *
AddSoundAsset(game_assets *Assets, char *Filename, uint32 FirstSampleIndex = 0, uint32 SampleCount = 0)
{
    Assert(Assets->DEBUGAssetType);
    Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < Assets->AssetCount);

    asset *Asset = Assets->Assets + Assets->DEBUGAssetType->OnePastLastAssetIndex++;
    Asset->FirstTagIndex = Assets->DEBUGUsedTagCount;
    Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
    Asset->SlotID = DEBUGAddSoundInfo(Assets, Filename, FirstSampleIndex, SampleCount).Value;

/*
	internal sound_id
	DEBUGAddSoundInfo(game_assets *Assets, char *Filename, uint32 FirstSampleIndex = 0, uint32 SampleCount = 0)
	{
		Assert(Assets->DEBUGUsedSoundCount < Assets->SoundCount);
		sound_id ID = {Assets->DEBUGUsedSoundCount++};

		asset_sound_info *Info = Assets->SoundInfos + ID.Value;
		Info->Filename = PushString(&Assets->Arena, Filename);
		Info->FirstSampleIndex = FirstSampleIndex;
		Info->SampleCount = SampleCount;
		Info->NextIDToPlay.Value = 0;

		return(ID);
	}
*/

    Assets->DEBUGAsset = Asset;

    return(Asset);
}

internal void
AddTag(game_assets *Assets, asset_tag_id ID, real32 Value)
{
    Assert(Assets->DEBUGAsset);

    Assets->DEBUGAsset->OnePastLastTagIndex++;
    asset_tag *Tag = Assets->Tags + Assets->DEBUGUsedTagCount++;

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

int 
main(int ArgCount, char **Args)
{
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
    asset *LastMusic = 0;
    for(uint32 FirstSampleIndex = 0;
        FirstSampleIndex < TotalMusicSampleCount;
        FirstSampleIndex += OneMusicChunk)
    {
        uint32 SampleCount = TotalMusicSampleCount - FirstSampleIndex;
        if(SampleCount > OneMusicChunk)
        {
            SampleCount = OneMusicChunk;
        }
        asset *ThisMusic = AddSoundAsset(Assets, "test2/Music.wav", FirstSampleIndex, SampleCount);
        if(LastMusic)
        {
            Assets->SoundInfos[LastMusic->SlotID].NextIDToPlay.Value = ThisMusic->SlotID;
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

	Out = fopen("test.hha", "wb");
	if(Out)
	{
		

		fclose(Out);
	}

	return(0);
}