#include "handmade.h"
#include "handmade_render_group.cpp"
#include "handmade_asset.cpp"
#include "handmade_audio.cpp"
#include "handmade_world.cpp"
#include "handmade_sim_region.cpp"
#include "handmade_entity.cpp"
#include "handmade_world_mode.cpp"
#include "handmade_meta.cpp"
#include "handmade_cutscene.cpp"

internal task_with_memory *
BeginTaskWithMemory(transient_state *TranState, b32 DependsOnGameMode)
{
    task_with_memory *FoundTask = 0;

    for(uint32 TaskIndex = 0;
        TaskIndex < ArrayCount(TranState->Tasks);
        TaskIndex++)
    {
        task_with_memory *Task = TranState->Tasks + TaskIndex;
        if(!Task->BeingUsed)
        {
            FoundTask = Task;
            Task->BeingUsed = true;
            Task->DependsOnGameMode = DependsOnGameMode;
            Task->MemoryFlush = BeginTemporaryMemory(&Task->Arena);
            break;
        }
    }

    return(FoundTask);
}

internal void 
EndTaskWithMemory(task_with_memory *Task)
{
    EndTemporaryMemory(Task->MemoryFlush);

    CompletePreviousWritesBeforeFutureWrites;

    Task->BeingUsed = false;    
}

internal loaded_bitmap
MakeEmptyBitmap(memory_arena *Arena, int32 Width, int32 Height, bool32 ClearToZero = true)
{
    loaded_bitmap Result = {};

    Result.AlignPercentage = V2(0.5f, 0.5f);
    Result.WidthOverHeight = SafeRatio1((real32)Width, (real32)Height);

    Result.Width = Width;
    Result.Height = Height;
    Result.Pitch = Result.Width*BITMAP_BYTES_PER_PIXEL;
    int32 TotalBitmapSize = Width*Height*BITMAP_BYTES_PER_PIXEL;
    Result.Memory = PushSize(Arena, TotalBitmapSize, Align(16, ClearToZero));

    return(Result);
}

internal void
MakeSphereNormalMap(loaded_bitmap *Bitmap, real32 Roughless, real32 Cx = 1.0f, real32 Cy = 1.0f)
{
    real32 InvWidth = 1.0f / (real32)(Bitmap->Width - 1);
    real32 InvHeight = 1.0f / (real32)(Bitmap->Height - 1);

    uint8 *Row = (uint8 *)Bitmap->Memory;
    for(int32 Y = 0;
        Y < Bitmap->Height;
        Y++)
    {
        uint32 *Pixel = (uint32 *)Row;
        for(int32 X = 0;
            X < Bitmap->Width;
            X++)
        {
            v2 BitmapUV = {InvWidth*(real32)X, InvHeight*(real32)Y};

            real32 Nx = Cx*(2.0f*BitmapUV.x - 1.0f);
            real32 Ny = Cy*(2.0f*BitmapUV.y - 1.0f);
            
            real32 RootTerm = 1.0f - Nx*Nx - Ny*Ny;
            v3 Normal = {0, 0.7f, 0.7f};            
            real32 Nz = 0.0f;
            if(RootTerm >= 0.0f)
            {   
                Nz = SquareRoot(RootTerm);
                Normal = {Nx, Ny, Nz};
            }

            v4 Color = {255.0f*(0.5f*(Normal.x + 1.0f)), 
                        255.0f*(0.5f*(Normal.y + 1.0f)), 
                        255.0f*(0.5f*(Normal.z + 1.0f)), 
                        255.0f*Roughless};
        
            *Pixel++ = ((uint32)(Color.w + 0.5f) << 24) |
                        ((uint32)(Color.x + 0.5f) << 16) |
                        ((uint32)(Color.y + 0.5f) << 8) |
                        ((uint32)(Color.z + 0.5f) << 0);   

        }

        Row += Bitmap->Pitch;
    }
}

internal void
MakeSphereDiffuseMap(loaded_bitmap *Bitmap, real32 Cx = 1.0f, real32 Cy = 1.0f)
{
    real32 InvWidth = 1.0f / (real32)(Bitmap->Width - 1);
    real32 InvHeight = 1.0f / (real32)(Bitmap->Height - 1);

    uint8 *Row = (uint8 *)Bitmap->Memory;
    for(int32 Y = 0;
        Y < Bitmap->Height;
        Y++)
    {
        uint32 *Pixel = (uint32 *)Row;
        for(int32 X = 0;
            X < Bitmap->Width;
            X++)
        {
            v2 BitmapUV = {InvWidth*(real32)X, InvHeight*(real32)Y};

            real32 Nx = Cx*(2.0f*BitmapUV.x - 1.0f);
            real32 Ny = Cy*(2.0f*BitmapUV.y - 1.0f);
            
            real32 RootTerm = 1.0f - Nx*Nx - Ny*Ny;
            real32 Alpha = 0.0f;
            if(RootTerm >= 0.0f)
            {   
                Alpha = 1.0f;
            }

            v3 BaseColor = {0.0f, 0.0f, 0.0f};
            Alpha *= 255.0f;
            v4 Color = {Alpha*BaseColor.r, 
                        Alpha*BaseColor.g, 
                        Alpha*BaseColor.b, 
                        Alpha};
        
            *Pixel++ = ((uint32)(Color.w + 0.5f) << 24) |
                        ((uint32)(Color.x + 0.5f) << 16) |
                        ((uint32)(Color.y + 0.5f) << 8) |
                        ((uint32)(Color.z + 0.5f) << 0);   

        }

        Row += Bitmap->Pitch;
    }
}

internal void
MakePyramidNormalMap(loaded_bitmap *Bitmap, real32 Roughless)
{
    real32 InvWidth = 1.0f / (real32)(Bitmap->Width - 1);
    real32 InvHeight = 1.0f / (real32)(Bitmap->Height - 1);

    uint8 *Row = (uint8 *)Bitmap->Memory;
    for(int32 Y = 0;
        Y < Bitmap->Height;
        Y++)
    {
        uint32 *Pixel = (uint32 *)Row;
        for(int32 X = 0;
            X < Bitmap->Width;
            X++)
        {
            v2 BitmapUV = {InvWidth*(real32)X, InvHeight*(real32)Y};

            int32 InvX = (Bitmap->Width - 1) - X;
            real32 Seven = 0.70710678118f;
            v3 Normal = {0, 0, Seven};

            if(X < Y)
            {
                if(InvX < Y)
                {
                    Normal.x = -Seven;
                }
                else
                {
                    Normal.y = Seven;
                }
            }
            else 
            {
                if(InvX < Y)
                {
                    Normal.y = -Seven;
                }
                else
                {
                    Normal.x = Seven;
                }
            }

            v4 Color = {255.0f*(0.5f*(Normal.x + 1.0f)), 
                        255.0f*(0.5f*(Normal.y + 1.0f)), 
                        255.0f*(0.5f*(Normal.z + 1.0f)), 
                        255.0f*Roughless};
        
            *Pixel++ = ((uint32)(Color.w + 0.5f) << 24) |
                        ((uint32)(Color.x + 0.5f) << 16) |
                        ((uint32)(Color.y + 0.5f) << 8) |
                        ((uint32)(Color.z + 0.5f) << 0);   

        }

        Row += Bitmap->Pitch;
    }
}

// TODO(georgy): Really want to get rid of main generation ID
internal u32
DEBUGGetMainGenerationID(game_memory *Memory)
{
    u32 Result = 0;

    transient_state *TranState = (transient_state *)Memory->TransientStorage;
    if(TranState->IsInitialized)
    {
        Result = TranState->MainGenerationID;
    }

    return(Result);
}

internal game_assets *
DEBUGGetGameAssets(game_memory *Memory)
{
    game_assets *Assets = 0;

    transient_state *TranState = (transient_state *)Memory->TransientStorage;
    if(TranState->IsInitialized)
    {
        Assets = TranState->Assets;
    }

    return(Assets);
}

internal void
SetGameMode(game_state *GameState, transient_state *TranState, game_mode GameMode)
{
    b32 NeedToWait = false;
    for(u32 TaskIndex = 0;
        TaskIndex < ArrayCount(TranState->Tasks);
        TaskIndex++)
    {
        NeedToWait = NeedToWait || TranState->Tasks[TaskIndex].DependsOnGameMode;
    }
    if(NeedToWait)
    {
        Platform.CompleteAllWork(TranState->LowPriorityQueue);
    }
    Clear(&GameState->ModeArena);
    GameState->GameMode = GameMode;
}

#if HANDMADE_INTERNAL
    game_memory *DebugGlobalMemory;
#endif
extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Platform = Memory->PlatformAPI;

#if HANDMADE_INTERNAL
    DebugGlobalMemory = Memory;

    {DEBUG_DATA_BLOCK("Renderer");
        {DEBUG_DATA_BLOCK("Renderer/Camera");
            DEBUG_VALUE(Global_Renderer_Camera_UseDebug);
            DEBUG_VALUE(Global_Renderer_Camera_DebugDistance);
            DEBUG_VALUE(Global_Renderer_Camera_UseRoomBasedCamera);
        }  
    }
    {DEBUG_DATA_BLOCK("GroundChunks");
        DEBUG_VALUE(Global_GroundChunks_Checkboards);
        DEBUG_VALUE(Global_GroundChunks_Outlines);
    }
    {DEBUG_DATA_BLOCK("Particles");
        DEBUG_VALUE(Global_Particles_Test);
    }
    {DEBUG_DATA_BLOCK("Simulation");
        DEBUG_VALUE(Global_Simulation_UseSpaceOutlines);
    }
    {DEBUG_DATA_BLOCK("Profile");
        DEBUG_PROFILE(GameUpdateAndRender);
    }

#endif

    TIMED_FUNCTION();

	Assert((&Input->Controllers[0].Start - &Input->Controllers[0].Buttons[0]) == 
            (ArrayCount(Input->Controllers[0].Buttons) - 1));
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    game_state *GameState = (game_state *)Memory->PermanentStorage;
    if (!GameState->IsInitialized)
    {
        memory_arena TotalArena;
        InitializeArena(&TotalArena, Memory->PermanentStorageSize - sizeof(game_state),
                        (uint8 *)Memory->PermanentStorage + sizeof(game_state));

        SubArena(&GameState->AudioArena, &TotalArena, Megabytes(1));
        SubArena(&GameState->ModeArena, &TotalArena, GetArenaSizeRemaining(&TotalArena));
        
        InitializeAudioState(&GameState->AudioState, &GameState->AudioArena);

        GameState->IsInitialized = true; 
    }

    Assert(sizeof(transient_state) <= Memory->TransientStorageSize);
    transient_state *TranState = (transient_state *)Memory->TransientStorage;
    if(!TranState->IsInitialized)
    {
        InitializeArena(&TranState->TranArena, Memory->TransientStorageSize - sizeof(transient_state),
                        (uint8 *)Memory->TransientStorage + sizeof(transient_state));   

        TranState->HighPriorityQueue = Memory->HighPriorityQueue;
        TranState->LowPriorityQueue = Memory->LowPriorityQueue;
        for(uint32 TaskIndex = 0;
            TaskIndex < ArrayCount(TranState->Tasks);
            TaskIndex++)
        {
            task_with_memory *Task = TranState->Tasks + TaskIndex;

            Task->BeingUsed = false;
            SubArena(&Task->Arena, &TranState->TranArena, Megabytes(1));
        }

        TranState->Assets = AllocateGameAssets(&TranState->TranArena, Megabytes(256), TranState);

        // GameState->Music = PlaySound(&GameState->AudioState, GetFirstSoundFrom(TranState->Assets, Asset_Music));

        TranState->GroundBufferCount = 256;
        TranState->GroundBuffers = PushArray(&TranState->TranArena, TranState->GroundBufferCount, ground_buffer);
        for(uint32 GroundBufferIndex = 0;
            GroundBufferIndex < TranState->GroundBufferCount;
            GroundBufferIndex++)
        {
            ground_buffer *GroundBuffer = TranState->GroundBuffers + GroundBufferIndex;
            GroundBuffer->Bitmap = MakeEmptyBitmap(&TranState->TranArena, GroundBufferWidth, GroundBufferHeight, false);
            GroundBuffer->P = NullPosition();
        }

        GameState->TestDiffuse = MakeEmptyBitmap(&TranState->TranArena, 256, 256, false);
        GameState->TestNormal = MakeEmptyBitmap(&TranState->TranArena, GameState->TestDiffuse.Width, GameState->TestDiffuse.Height, false);
        MakeSphereNormalMap(&GameState->TestNormal, 0.0f);
        MakeSphereDiffuseMap(&GameState->TestDiffuse);

        TranState->EnvMapWidth = 512;
        TranState->EnvMapHeight = 256;
        for(uint32 MapIndex = 0;
            MapIndex < ArrayCount(TranState->EnvMaps);
            MapIndex++)
        {
            environment_map *Map = TranState->EnvMaps + MapIndex;
            uint32 Width = TranState->EnvMapWidth;
            uint32 Height = TranState->EnvMapHeight;
            for(uint32 LODIndex = 0;
                LODIndex < ArrayCount(Map->LOD);
                LODIndex++)
            {
                Map->LOD[LODIndex] = MakeEmptyBitmap(&TranState->TranArena, Width, Height, false);
                Width >>= 1;
                Height >>= 1;
            }
        }

        TranState->IsInitialized = true;
    }

    // TODO(georgy): We should probably pull the generation stuff, because
    // if we don't use ground chunks, it's a huge waste of effort!
    if(TranState->MainGenerationID)
    {
        EndGeneration(TranState->Assets, TranState->MainGenerationID);
    }
    TranState->MainGenerationID = BeginGeneration(TranState->Assets);

    if(GameState->GameMode == GameMode_None)
    {
        PlayIntroCutScene(GameState, TranState);
    }

#if 0
    if(Memory->ExecutableReloaded)
    {
        for(uint32 GroundBufferIndex = 0;
            GroundBufferIndex < TranState->GroundBufferCount;
            GroundBufferIndex++)
        {
            ground_buffer *GroundBuffer = TranState->GroundBuffers + GroundBufferIndex;
            GroundBuffer->P = NullPosition();
        }
    }
#endif

    // 
    // NOTE(george): Render
    // 

    temporary_memory RenderMemory = BeginTemporaryMemory(&TranState->TranArena);    

    render_group RenderGroup_ = BeginRenderGroup(TranState->Assets, RenderCommands, TranState->MainGenerationID, false);
    render_group *RenderGroup = &RenderGroup_;

    // TODO(georgy): Eliminate these entirely
    loaded_bitmap DrawBuffer = {};
    DrawBuffer.Width = RenderCommands->Width;
    DrawBuffer.Height = RenderCommands->Height;

    b32 Rerun = false;
    do 
    {
        switch(GameState->GameMode)
        {
            case GameMode_TitleScreen:
            {
                Rerun = UpdateAndRenderTitleScreen(GameState, TranState, RenderGroup, Input, &DrawBuffer,
                                                       GameState->TitleScreen);
            } break;

            case GameMode_CutScene:
            {
                Rerun = UpdateAndRenderCutScene(GameState, TranState, RenderGroup, &DrawBuffer,
                                                    Input, GameState->CutScene);
            } break;

            case GameMode_World:
            {
                Rerun = UpdateAndRenderWorld(GameState, GameState->WorldMode, TranState, Input, RenderGroup, &DrawBuffer);
            } break;

            InvalidDefaultCase;
        }
    } while(Rerun);
    
    EndRenderGroup(RenderGroup);  
    
    EndTemporaryMemory(RenderMemory);
    
#if 0
    if(!HeroesExist && QuitRequested)
    {
        Memory->QuitRequested = true;
    }
#endif

    CheckArena(&GameState->ModeArena);
    CheckArena(&TranState->TranArena);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    transient_state *TranState = (transient_state *)Memory->TransientStorage;

    OutputPlayingSounds(&GameState->AudioState, SoundBuffer, TranState->Assets, &TranState->TranArena);
    // GameOutputSound(GameState, SoundBuffer, 400);
}

#if HANDMADE_INTERNAL
#include "handmade_debug.cpp"
#else
extern "C" DEBUG_GAME_FRAME_END(DEBUGFrameEnd)
{
    return(0);
}
#endif