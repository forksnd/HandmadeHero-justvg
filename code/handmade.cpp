#include "handmade.h"

#include "handmade_tile.cpp"

internal void
DrawRectangle(game_offscreen_buffer *Buffer, 
              real32 RealMinX, real32 RealMinY, real32 RealMaxX, real32 RealMaxY,
              real32 R, real32 G, real32 B)
{
    int32 MinX = RoundReal32ToInt32(RealMinX);
    int32 MinY = RoundReal32ToInt32(RealMinY);
    int32 MaxX = RoundReal32ToInt32(RealMaxX);
    int32 MaxY = RoundReal32ToInt32(RealMaxY);

    if (MinX < 0)
    {
        MinX = 0;
    }

    if (MinY < 0)
    {
        MinY = 0;
    }

    if (MaxX > Buffer->Width)
    {
        MaxX = Buffer->Width;
    }

    if (MaxY > Buffer->Height)
    {
        MaxY = Buffer->Height;
    }

    uint32 Color = (RoundReal32ToUInt32(R * 255.0f) << 16) | 
                   (RoundReal32ToUInt32(G * 255.0f) << 8) |
                   RoundReal32ToUInt32(B * 255.0f);

    uint8 *Row = (uint8 *)Buffer->Memory + MinX*Buffer->BytesPerPixel + MinY*Buffer->Pitch;

    for (int Y = MinY; Y < MaxY; Y++)
    {
        uint32 *Pixel = (uint32 *)Row;
        for (int X = MinX; X < MaxX; X++)
        {
            *Pixel++ = Color;
        }

        Row += Buffer->Pitch;
    }
}

internal void
GameOutputSound(game_state *GameState, game_sound_output_buffer *SoundBuffer, int ToneHz)
{ 
    int16 ToneVolume = 3000;
    int WavePeriod = SoundBuffer->SamplesPerSecond/ToneHz;

    int16 *SampleOut = SoundBuffer->Samples;
    for(int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; SampleIndex++)
    {
#if 0
        real32 SineValue = sinf(GameState->tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
#else
        int16 SampleValue = 0;
#endif
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

#if 0
		GameState->tSine += 2.0f*Pi32*1.0f / (real32)WavePeriod;

        if (GameState->tSine > 2.0f*Pi32)
        {
            GameState->tSine -= 2.0f*Pi32;
        }
#endif
    }
}

internal void
InitializeArena(memory_arena *Arena, memory_index Size, uint8 *Base)
{
    Arena->Size = Size;
    Arena->Base = Base;
    Arena->Used = 0;
}

#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, Count, type) (type *)PushSize_(Arena, (Count)*sizeof(type))
internal void *
PushSize_(memory_arena *Arena, memory_index Size)
{
    Assert((Arena->Used + Size) <= Arena->Size);
    void *Result = Arena->Base + Arena->Used;
    Arena->Used += Size;

    return(Result);
}


extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert((&Input->Controllers[0].Start - &Input->Controllers[0].Buttons[0]) == 
            (ArrayCount(Input->Controllers[0].Buttons) - 1));
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    real32 PlayerHeight = 1.4f;
    real32 PlayerWidth = 0.75f*PlayerHeight;
    
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    if (!Memory->IsInitialized)
    {
        GameState->PlayerP.AbsTileX = 1;
        GameState->PlayerP.AbsTileY = 3;
        GameState->PlayerP.TileRelX = 0.5f;
        GameState->PlayerP.TileRelY = 0.5f;

        InitializeArena(&GameState->WorldArena, Memory->PermanentStorageSize - sizeof(game_state),
                        (uint8 *)Memory->PermanentStorage + sizeof(game_state));

        GameState->World = PushStruct(&GameState->WorldArena, world);
        world *World = GameState->World;
        World->TileMap = PushStruct(&GameState->WorldArena, tile_map);

        tile_map *TileMap = World->TileMap;

        TileMap->ChunkShift = 4;
        TileMap->ChunkMask = (1 << TileMap->ChunkShift) - 1;
        TileMap->ChunkDim = (1 << TileMap->ChunkShift);

        TileMap->TileChunkCountX = 128;
        TileMap->TileChunkCountY = 128;

        TileMap->TileChunks = PushArray(&GameState->WorldArena, TileMap->TileChunkCountX*TileMap->TileChunkCountY, tile_chunk);
        
        for(uint32 Y = 0; Y < TileMap->TileChunkCountY; Y++)
        {
            for(uint32 X = 0; X < TileMap->TileChunkCountX; X++)
            {
                uint32 TileChunkIndex = Y*TileMap->TileChunkCountX + X;
                uint32 TileCount = TileMap->ChunkDim*TileMap->ChunkDim;
                TileMap->TileChunks[TileChunkIndex].Tiles = PushArray(&GameState->WorldArena, TileCount, uint32);
                for(uint32 TileIndex = 0; TileIndex < TileCount; TileIndex++)
                {
                    TileMap->TileChunks[TileChunkIndex].Tiles[TileIndex] = 1;
                }
            }
        }

        TileMap->TileSideInMeters = 1.4f;
        TileMap->TileSideInPixels = 6;
        TileMap->MetersToPixels = (real32)TileMap->TileSideInPixels / (real32)TileMap->TileSideInMeters;

        real32 LowerLeftX = -(real32)TileMap->TileSideInPixels/2.0f;
        real32 LowerLeftY = (real32)Buffer->Height;  

        uint32 RandomNumberIndex = 0;

        uint32 TilesPerWidth = 17;
        uint32 TilesPerHeight = 9;
        uint32 ScreenX = 0;
        uint32 ScreenY = 0;
        for (uint32 ScreenIndex = 0; ScreenIndex < 100; ScreenIndex++)
        {
            for(uint32 TileY = 0; TileY < TilesPerHeight; TileY++)
            {
                for(uint32 TileX = 0; TileX < TilesPerWidth; TileX++)
                {
                    uint32 AbsTileX = ScreenX*TilesPerWidth + TileX;
                    uint32 AbsTileY = ScreenY*TilesPerHeight + TileY;

                    uint32 TileValue = 1;
                    if((TileX == 0) || (TileX == TilesPerWidth - 1))
                    {
                        if(TileY == (TilesPerHeight/2))
                        {
                            TileValue = 1;
                        }
                        else
                        {
                            TileValue = 2;
                        }
                    }

                    if((TileY == 0) || (TileY == TilesPerHeight - 1))
                    {
                        if(TileX == (TilesPerWidth/2))
                        {
                            TileValue = 1;
                        }
                        else
                        {
                            TileValue = 2;
                        }
                    }

                    SetTileValue(&GameState->WorldArena, World->TileMap, AbsTileX, AbsTileY, TileValue);
                }
            }

            // TODO(george): Random number generator!
            Assert(RandomNumberIndex < ArrayCount(RandomNumberTable));
            uint32 RandomChoice = RandomNumberTable[RandomNumberIndex++] % 2;
            if(RandomChoice == 0)
            {
                ScreenX += 1;
            }
            else
            {
                ScreenY += 1;
            }
        }
    

        Memory->IsInitialized = true;
    }

    world *World = GameState->World;
    tile_map *TileMap = World->TileMap;

    for (int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ControllerIndex++)
    {
        game_controller_input *Controller = &Input->Controllers[ControllerIndex];
        if(Controller->IsAnalog)
        {
            // NOTE(george): Use analog movement tuning
        }
        else
        {
            // NOTE(george): Use digital movement tuning
            real32 dPlayerX = 0.0f; 
            real32 dPlayerY = 0.0f; 

            if(Controller->MoveUp.EndedDown)
            {
                dPlayerY = 1.0f;
            }
            if(Controller->MoveDown.EndedDown)
            {
                dPlayerY = -1.0f;
            }
            if(Controller->MoveLeft.EndedDown)
            {
                dPlayerX = -1.0f;
            }
            if(Controller->MoveRight.EndedDown)
            {
                dPlayerX = 1.0f;
            }
            real32 PlayerSpeed = 3.0f;
            if(Controller->ActionUp.EndedDown)
            {
                PlayerSpeed = 10.0f;
            }

            dPlayerX *= PlayerSpeed;
            dPlayerY *= PlayerSpeed;

            // TODO(george): Diagonal will be faster! Fix once we have vectors!
            tile_map_position NewPlayerP = GameState->PlayerP;
            NewPlayerP.TileRelX += Input->dtForFrame * dPlayerX;
            NewPlayerP.TileRelY += Input->dtForFrame * dPlayerY;
            NewPlayerP = RecanonicalizePosition(TileMap, NewPlayerP);
            // TODO(george): Delta function that auto-recanonicalizes

            // We should use NewPlayerX and Y!
            tile_map_position PlayerLeft = NewPlayerP;
            PlayerLeft.TileRelX -= 0.5f*PlayerWidth;
            PlayerLeft = RecanonicalizePosition(TileMap, PlayerLeft);
            
            tile_map_position PlayerRight = NewPlayerP;
            PlayerRight.TileRelX += 0.5f*PlayerWidth;            
            PlayerRight = RecanonicalizePosition(TileMap, PlayerRight);
            
            if(IsTileMapPointEmpty(TileMap, NewPlayerP) &&
               IsTileMapPointEmpty(TileMap, PlayerLeft) &&
               IsTileMapPointEmpty(TileMap, PlayerRight))
            {
                GameState->PlayerP = NewPlayerP;
            }
        }
    }

    DrawRectangle(Buffer, 0.0f, 0.0f, (real32)Buffer->Width, (real32)Buffer->Height, 1.0f, 0.0f, 0.0f);

    real32 ScreenCenterX = 0.5f*(real32)Buffer->Width;
    real32 ScreenCenterY = 0.5f*(real32)Buffer->Height;

    for (int32 RelRow = -100; RelRow < 100; RelRow++)
    {
        for (int32 RelColumn = -200; RelColumn < 200; RelColumn++)
        {
            uint32 Column = GameState->PlayerP.AbsTileX + RelColumn;
            uint32 Row = GameState->PlayerP.AbsTileY + RelRow;

            uint32 TileID = GetTileValue(TileMap, Column, Row); 
            if(TileID > 0)
            {
                real32 Gray = 0.5f;
                if(TileID == 2)
                {
                    Gray = 1.0f;
                }
                
                if((Row == GameState->PlayerP.AbsTileY) && (Column == GameState->PlayerP.AbsTileX))
                {
                    Gray = 0.0f;
                }

                real32 CenX = ScreenCenterX - TileMap->MetersToPixels*GameState->PlayerP.TileRelX + (real32)RelColumn * TileMap->TileSideInPixels;
                real32 CenY = ScreenCenterY + TileMap->MetersToPixels*GameState->PlayerP.TileRelY - (real32)RelRow * TileMap->TileSideInPixels;
                real32 MinX = CenX - 0.5f*TileMap->TileSideInPixels;
                real32 MinY = CenY - 0.5f*TileMap->TileSideInPixels;
                real32 MaxX = CenX + 0.5f*TileMap->TileSideInPixels;
                real32 MaxY = CenY + 0.5f*TileMap->TileSideInPixels;

                DrawRectangle(Buffer, MinX, MinY, MaxX, MaxY, Gray, Gray, Gray);
            }
        }
    }

    real32 PlayerR = 1.0f;
    real32 PlayerG = 1.0f;
    real32 PlayerB = 0.0f;
    real32 PlayerLeft = ScreenCenterX - (0.5f*TileMap->MetersToPixels*PlayerWidth);
    real32 PlayerTop = ScreenCenterY - TileMap->MetersToPixels*PlayerHeight;
    DrawRectangle(Buffer, 
                  PlayerLeft, PlayerTop, 
                  PlayerLeft + TileMap->MetersToPixels*PlayerWidth, 
                  PlayerTop + TileMap->MetersToPixels*PlayerHeight, 
                  PlayerR, PlayerG, PlayerB);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    GameOutputSound(GameState, SoundBuffer, 400);
}

/*
internal void 
RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
    uint8 *Row = (uint8 *)Buffer->Memory;
    for (int Y = 0; Y < Buffer->Height; Y++)
    {
        uint32 *Pixel = (uint32 *) Row;
        
        for (int X = 0; X < Buffer->Width; X++)
        {
            uint8 Blue = (uint8)(X + BlueOffset);
            uint8 Green = (uint8)(Y + GreenOffset);
            
            *Pixel++ = ((Green << 8) | Blue);   
        }
        
        Row += Buffer->Pitch;
    }
}
*/