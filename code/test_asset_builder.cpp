#include "test_asset_builder.h"

FILE *Out = 0;

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
    Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < ArrayCount(Assets->AssetTypes));

    bitmap_id Result = {Assets->DEBUGAssetType->OnePastLastAssetIndex++};
    asset *Asset = Assets->Assets + Result.Value;
    Asset->FirstTagIndex = Assets->TagCount;
    Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
    Asset->Bitmap.Filename = Filename;
    Asset->Bitmap.AlignPercentage[0] = AlignPercentageX;
    Asset->Bitmap.AlignPercentage[1] = AlignPercentageY;

    Assets->DEBUGAsset = Asset;

    return(Result);
}

internal sound_id
AddSoundAsset(game_assets *Assets, char *Filename, uint32 FirstSampleIndex = 0, uint32 SampleCount = 0)
{
    Assert(Assets->DEBUGAssetType);
    Assert(Assets->DEBUGAssetType->OnePastLastAssetIndex < ArrayCount(Assets->AssetTypes));

    sound_id Result = {Assets->DEBUGAssetType->OnePastLastAssetIndex++};
    asset *Asset = Assets->Assets + Result.Value;
    Asset->FirstTagIndex = Assets->TagCount;
    Asset->OnePastLastTagIndex = Asset->FirstTagIndex;
    Asset->Sound.Filename = Filename;
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
    Assets->DEBUGAsset = 0;
}

int 
main(int ArgCount, char **Args)
{
    game_assets Assets_;
    game_assets *Assets = &Assets_;

    Assets->TagCount = 1;
    Assets->AssetCount = 1;
    Assets->DEBUGAssetType = 0; 
    Assets->DEBUGAsset = 0;

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
        if(LastMusic.Value)
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

	Out = fopen("test.hha", "wb");
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
 //       fwrite(AssetArray, AssetArraySize, 1, Out);

		fclose(Out);
	}
    else
    {
        printf("ERROR: Couldn't open file!\n");
    }

	return(0);
}