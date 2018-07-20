#include "handmade.h"

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
            
            *Pixel++ = ((Green << 16) | Blue);   
        }
        
        Row += Buffer->Pitch;
    }
}

internal void
RenderPlayer(game_offscreen_buffer *Buffer, int PlayerX, int PlayerY)
{
    uint8 *EndOfBuffer = (uint8 *)Buffer->Memory + Buffer->Pitch*Buffer->Height;
    uint32 Color = 0xFFFFFFFF;
    int Top = PlayerY;
    int Bottom = PlayerY + 10;

    for (int X = PlayerX; X < PlayerX+10; X++)
    {
        uint8 *Pixel = (uint8 *)Buffer->Memory + X*Buffer->BytesPerPixel + Top*Buffer->Pitch;
        for (int Y = Top; Y < Bottom; Y++)
        {
            if((Pixel >= Buffer->Memory) && (Pixel < EndOfBuffer))
            {
                *(uint32 *)Pixel = Color;
            }

            Pixel += Buffer->Pitch;
        }
    }
}

internal void
GameOutputSound(game_sound_output_buffer *SoundBuffer, game_state *GameState)
{ 
    int16 ToneVolume = 3000;
    int WavePeriod = SoundBuffer->SamplesPerSecond/GameState->ToneHz;

    int16 *SampleOut = SoundBuffer->Samples;
    for(int SampleIndex = 0; SampleIndex < SoundBuffer->SampleCount; SampleIndex++)
    {
#if 1
        real32 SineValue = sinf(GameState->tSine);
        int16 SampleValue = (int16)(SineValue * ToneVolume);
#else
        int16 SampleValue = 0;
#endif
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

		GameState->tSine += 2.0f*Pi32*1.0f / (real32)WavePeriod;

        if (GameState->tSine > 2.0f*Pi32)
        {
            GameState->tSine -= 2.0f*Pi32;
        }
    }
}

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
    Assert((&Input->Controllers[0].Start - &Input->Controllers[0].Buttons[0]) == 
            (ArrayCount(Input->Controllers[0].Buttons) - 1));
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);

    game_state *GameState = (game_state *)Memory->PermanentStorage;
    if (!Memory->IsInitialized)
    {
        char *Filename = __FILE__;

        debug_read_file_result FileMemory = Memory->DEBUGPlatformReadEntireFile(Filename);
        if (FileMemory.Contents) 
        {
            Memory->DEBUGPlatformWriteEntireFile("test.out", FileMemory.ContentsSize, FileMemory.Contents);
            Memory->DEBUGPlatformFreeFileMemory(FileMemory.Contents);
        }

        GameState->ToneHz = 512; 

        GameState->PlayerX = 100;
        GameState->PlayerY = 100;

        // TODO(george): This may be more appropriate to do in the platform layer
        Memory->IsInitialized = true;
    }

    for (int ControllerIndex = 0; ControllerIndex < ArrayCount(Input->Controllers); ControllerIndex++)
    {
        game_controller_input *Controller = &Input->Controllers[ControllerIndex];
        if(Controller->IsAnalog)
        {
            // NOTE(george): Use analog movement tuning
            GameState->BlueOffset += (int)(4.0f*Controller->StickAverageX);
            GameState->ToneHz = 256 + (int)(128.0f*Controller->StickAverageY);
        }
        else
        {
            // NOTE(george): Use digital movement tuning
#if 0       
            if (Controller->MoveLeft.EndedDown)
            {
                GameState->BlueOffset--;
            }

            if (Controller->MoveRight.EndedDown)
            {
                GameState->BlueOffset++;
            }
#endif

            GameState->PlayerX -= 8 * Controller->MoveLeft.HalfTransitionCount;
            GameState->PlayerX += 8 * Controller->MoveRight.HalfTransitionCount;
            GameState->PlayerY -= 8 * Controller->MoveUp.HalfTransitionCount;
            GameState->PlayerY += 8 * Controller->MoveDown.HalfTransitionCount;            

            if (GameState->tJump > 0)
            {
                GameState->PlayerY += (int)(10.0f*sinf(0.5f*Pi32*GameState->tJump));
            }

            if (Controller->ActionDown.EndedDown)
            {
                GameState->tJump = 4.0f;
            }
			GameState->tJump -= 0.033f;
        }

        GameState->PlayerX += (int)(4.0f*Controller->StickAverageX);
        GameState->PlayerY -= (int)(4.0f*Controller->StickAverageX);
    }
    
	RenderWeirdGradient(Buffer, GameState->BlueOffset, GameState->GreenOffset);
    RenderPlayer(Buffer, GameState->PlayerX, GameState->PlayerY);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    GameOutputSound(SoundBuffer, GameState);
}