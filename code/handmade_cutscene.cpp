internal void
RenderCutscene(game_assets *Assets, render_group *RenderGroup, loaded_bitmap *DrawBuffer,
               r32 tCutScene)
{
    // TODO(georgy): Unify this stuff?
    real32 WidthOfMonitor = 0.635f; // NOTE(george): Horizontal measurment of monitor in meters
    real32 MetersToPixels = (real32)DrawBuffer->Width/WidthOfMonitor;

    real32 FocalLength = 0.6f;

    r32 tStart = 0.0f;
    r32 tEnd = 5.0f;

    r32 tNormal = Clamp01MapToRange(tStart, tCutScene, tEnd);

    v3 CameraStart = {0.0f, 0.0f, 0.0f};
    v3 CameraEnd = {-4.0f, -2.0f, 0.0f};
    v3 CameraOffset = Lerp(CameraStart, tNormal, CameraEnd);
    real32 DistanceAboveGround = 10.0f - tNormal*5.0f;
    Perspective(RenderGroup, DrawBuffer->Width, DrawBuffer->Height, MetersToPixels, FocalLength, DistanceAboveGround);

    asset_vector MatchVector = {};
    asset_vector WeightVector = {};
    WeightVector.E[Tag_ShotIndex] = 1.0f;
    WeightVector.E[Tag_LayerIndex] = 1.0f;

    int ShotIndex = 1;
    MatchVector.E[Tag_ShotIndex] = (r32)ShotIndex;
    v4 LayerPlacement[] =
    {
        {0.0f, 0.0f, DistanceAboveGround - 200.0f, 300.0f}, // NOTE(georgy): Sky background
        {0.0f, 0.0f, -170.0f, 300.0f}, // NOTE(georgy): Weird sky light
        {0.0f, 0.0f, -100.0f, 40.0f}, // NOTE(georgy): Backmost row of trees
        {0.0f, 10.0f, -70.0f, 80.0f}, // NOTE(georgy): Middle hills and trees
        {0.0f, 0.0f, -50.0f, 70.0f}, // NOTE(georgy): Front hills and trees
        {30.0f, 0.0f, -30.0f, 50.0f}, // NOTE(georgy): Right side tree and fence
        {0.0f, -2.0f, -20.0f, 40.0f}, // NOTE(georgy): 7
        {2.0f, -1.0f, -5.0f, 25.0f}, // NOTE(georgy): 8
    };

    for(u32 LayerIndex = 1;
        LayerIndex <= 8;
        LayerIndex++)
    {
        v4 Placement = LayerPlacement[LayerIndex - 1];
        RenderGroup->Transform.OffsetP = Placement.xyz - CamearOffset;
        MatchVector.E[Tag_LayerIndex] = (r32)LayerIndex;
        bitmap_id LayerImage = GetBestMatchBitmapFrom(Assets, Asset_OpeningCutscene, &MatchVector, &WeightVector);
        PushBitmap(RenderGroup, LayerImage, Placement.w, V3(0, 0, 0));  
    }
}