#define CUTSCENE_WARMUP_SECONDS 2.0f

internal void
RenderLayeredScene(game_assets *Assets, render_group *RenderGroup, loaded_bitmap *DrawBuffer, 
                   layered_scene *Scene, r32 tNormal)
{
    // TODO(georgy): Unify this stuff?
    real32 WidthOfMonitor = 0.635f; // NOTE(george): Horizontal measurment of monitor in meters
    real32 MetersToPixels = (real32)DrawBuffer->Width/WidthOfMonitor;
    real32 FocalLength = 0.6f;

    r32 SceneFadeValue = 1.0f;
    if(tNormal < Scene->tFadeIn)
    {
        SceneFadeValue = Clamp01MapToRange(0.0f, tNormal, Scene->tFadeIn);
    }

    v4 Color = {SceneFadeValue, SceneFadeValue, SceneFadeValue, 1.0f};

    v3 CameraStart = Scene->CameraStart;
    v3 CameraEnd = Scene->CameraEnd;
    v3 CameraOffset = Lerp(CameraStart, tNormal, CameraEnd);
    if(RenderGroup)
    {
        Perspective(RenderGroup, DrawBuffer->Width, DrawBuffer->Height, MetersToPixels, FocalLength, 0.0f);
    }

    asset_vector MatchVector = {};
    MatchVector.E[Tag_ShotIndex] = (r32)Scene->ShotIndex;
    asset_vector WeightVector = {};
    WeightVector.E[Tag_ShotIndex] = 1.0f;
    WeightVector.E[Tag_LayerIndex] = 1.0f;

    if(Scene->LayerCount == 0)
    {
        Clear(RenderGroup, V4(0.0f, 0.0f, 0.0f, 0.0f));
    }

    for(u32 LayerIndex = 1;
        LayerIndex <= Scene->LayerCount;
        LayerIndex++)
    {
        scene_layer Layer = Scene->Layers[LayerIndex - 1];
        b32 Active = true;
        if(Layer.Flags &SceneLayerFlag_Transient)
        {
            Active = (tNormal >= Layer.Param.x) && (tNormal < Layer.Param.y);
        }

        if(Active)
        {
            MatchVector.E[Tag_LayerIndex] = (r32)LayerIndex;
            bitmap_id LayerImage = GetBestMatchBitmapFrom(Assets, Scene->AssetType, &MatchVector, &WeightVector);

            if(RenderGroup)
            {
                v3 P = Layer.P;
                if(Layer.Flags & SceneLayerFlag_AtInfinity)
                {
                    P.z += CameraOffset.z;
                }

                if(Layer.Flags & SceneLayerFlag_Floaty)
                {
                    P.y += Layer.Param.x*Sin(Layer.Param.y*tNormal);
                }

                if(Layer.Flags & SceneLayerFlag_CounterCameraX)
                {
                    RenderGroup->Transform.OffsetP.x = P.x + CameraOffset.x;
                }
                else
                {
                    RenderGroup->Transform.OffsetP.x = P.x - CameraOffset.x;
                }

                if(Layer.Flags & SceneLayerFlag_CounterCameraY)
                {
                    RenderGroup->Transform.OffsetP.y = P.y + CameraOffset.y;
                }
                else
                {
                    RenderGroup->Transform.OffsetP.y = P.y - CameraOffset.y;
                }

                RenderGroup->Transform.OffsetP.z = P.z - CameraOffset.z;
    
                PushBitmap(RenderGroup, LayerImage, Layer.Height, V3(0, 0, 0), Color);  
            }
            else
            {
                PrefetchBitmap(Assets, LayerImage);
            }
        }
    }
}

global_variable scene_layer IntroLayers1[] =
{
    {{0.0f, 0.0f, -200.0f}, 300.0f, SceneLayerFlag_AtInfinity}, // NOTE(georgy): Sky background
    {{0.0f, 0.0f, -170.0f}, 300.0f}, // NOTE(georgy): Weird sky light
    {{0.0f, 0.0f, -100.0f}, 40.0f}, // NOTE(georgy): Backmost row of trees
    {{0.0f, 10.0f, -70.0f}, 80.0f}, // NOTE(georgy): Middle hills and trees
    {{0.0f, 0.0f, -50.0f}, 70.0f}, // NOTE(georgy): Front hills and trees
    {{30.0f, 0.0f, -30.0f}, 50.0f}, // NOTE(georgy): Right side tree and fence
    {{0.0f, -2.0f, -20.0f}, 40.0f}, // NOTE(georgy): 7
    {{2.0f, -1.0f, -5.0f}, 25.0f}, // NOTE(georgy): 8
};
global_variable scene_layer IntroLayers2[] =
{
    {{0.0f, 0.0f, -20.0f}, 300.0f}, // NOTE(georgy): Sky background
    {{0.0f, 0.0f, -170.0f}, 300.0f}, // NOTE(georgy): Weird sky light
    {{0.0f, 0.0f, -100.0f}, 40.0f}, // NOTE(georgy): Backmost row of trees
    {{0.0f, 10.0f, -70.0f}, 80.0f}, // NOTE(georgy): Middle hills and trees
    {{0.0f, 0.0f, -50.0f}, 70.0f}, // NOTE(georgy): Front hills and trees
    {{30.0f, 0.0f, -30.0f}, 50.0f}, // NOTE(georgy): Right side tree and fence
    {{0.0f, -2.0f, -20.0f}, 40.0f}, // NOTE(georgy): 7
    {{2.0f, -1.0f, -5.0f}, 25.0f}, // NOTE(georgy): 8
};

#define INTRO_SCENE(Index) Asset_OpeningCutscene, Index, ArrayCount(IntroLayers##Index), IntroLayers##Index
// NOTE(georgy): There are more shots for this one, but I decided not to copy them from the stream (laziness)
layered_scene IntroCutScene[] = 
{
    {Asset_None, 0, 0, 0, CUTSCENE_WARMUP_SECONDS},
    {INTRO_SCENE(1), 20.0f, {0.0f, 0.0f, 10.0f}, {-4.0f, -2.0f, 5.0f}, 0.1f},
    {INTRO_SCENE(2), 20.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}}
};

internal b32
RenderCutSceneAtTime(game_assets *Assets, render_group *RenderGroup, loaded_bitmap *DrawBuffer,
                     playing_cutscene *CutScene, r32 tCutScene)
{
    b32 CutSceneComplete = false;

    r32 tBase = 0.0f;
    for(u32 ShotIndex = 0;
        ShotIndex < CutScene->SceneCount;
        ShotIndex++)
    {
        layered_scene *Scene = CutScene->Scenes + ShotIndex;
        r32 tStart = tBase;
        r32 tEnd = tStart + Scene->Duration;

        if((tCutScene >= tStart) &&
           (tCutScene < tEnd))
        {
            r32 tNormal = Clamp01MapToRange(tStart, tCutScene, tEnd);
            RenderLayeredScene(Assets, RenderGroup, DrawBuffer,  Scene, tNormal);
            CutSceneComplete = true;
        }

        tBase += tEnd;
    }

    return(CutSceneComplete);
}   

internal b32
RenderCutScene(game_assets *Assets, render_group *RenderGroup, loaded_bitmap *DrawBuffer,
               playing_cutscene *CutScene)
{
    RenderCutSceneAtTime(Assets, 0, DrawBuffer, CutScene, CutScene->t + CUTSCENE_WARMUP_SECONDS);

    b32 CutSceneComplete = RenderCutSceneAtTime(Assets, RenderGroup, DrawBuffer, CutScene, CutScene->t);
    if(!CutSceneComplete)
    {
        CutScene->t = 0.0f;
    }

    return(CutSceneComplete);
}

internal void
AdvanceCutScene(playing_cutscene *CutScene, r32 dt)
{
    CutScene->t += dt;
}

internal playing_cutscene
MakeIntroCutScene(void)
{
    playing_cutscene Result = {};

    Result.SceneCount = ArrayCount(IntroCutScene);
    Result.Scenes = IntroCutScene;

    return(Result);
}