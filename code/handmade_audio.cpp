internal void
OutputTestSineWave(game_state *GameState, game_sound_output_buffer *SoundBuffer, int ToneHz)
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

internal playing_sound *
PlaySound(audio_state *AudioState, sound_id SoundID)
{
    if(!AudioState->FirstFreePlayingSound)
    {
        AudioState->FirstFreePlayingSound = PushStruct(AudioState->PermArena, playing_sound);
        AudioState->FirstFreePlayingSound->Next = 0;
    }

    playing_sound *PlayingSound = AudioState->FirstFreePlayingSound;
    AudioState->FirstFreePlayingSound = PlayingSound->Next;

    PlayingSound->CurrentVolume = PlayingSound->TargetVolume = V2(1.0f, 1.0f);
    PlayingSound->dCurrentVolume = V2(0.0f, 0.0f);
    PlayingSound->ID = SoundID;
    PlayingSound->SamplesPlayed = 0;
    PlayingSound->dSample = 1.0f;

    PlayingSound->Next = AudioState->FirstPlayingSound;
    AudioState->FirstPlayingSound = PlayingSound;

    return(PlayingSound);
}

internal void
ChangeVolume(audio_state *AudioState, playing_sound *Sound, real32 FadeDurationInSeconds, v2 Volume)
{
    if(FadeDurationInSeconds <= 0.0f)
    {
        Sound->CurrentVolume = Sound->TargetVolume = Volume;
    }
    else
    {
        real32 OneOverFade = 1.0f / FadeDurationInSeconds;
        Sound->dCurrentVolume = (Sound->TargetVolume - Sound->CurrentVolume) * OneOverFade;
        Sound->TargetVolume = Volume;
    }
}

inline void
ChangePitch(audio_state *AudioState, playing_sound *Sound, real32 dSample)
{
    Sound->dSample = dSample;
}

internal void
OutputPlayingSounds(audio_state *AudioState, game_sound_output_buffer *SoundBuffer, 
					game_assets *Assets, memory_arena *TempArena)
{
	temporary_memory MixerMemory = BeginTemporaryMemory(TempArena);

    uint32 SampleCountAlign4 = Align4(SoundBuffer->SampleCount);
    uint32 SampleCount4 = SampleCountAlign4 / 4;

    __m128 *RealChannel0 = PushArray(TempArena, SampleCount4, __m128, 16);
    __m128 *RealChannel1 = PushArray(TempArena, SampleCount4, __m128, 16);

    real32 SecondsPerSample = 1.0f / SoundBuffer->SamplesPerSecond;
#define AudioStateOutputChannelCount  2

    __m128 Zero = _mm_set1_ps(0.0f);

    // NOTE(georgy): Clear out the mixer channel
    {
        __m128 *Dest0 = RealChannel0;
        __m128 *Dest1 = RealChannel1;
        for(uint32 SampleIndex = 0;
            SampleIndex < SampleCount4;
            SampleIndex++)
        {
            _mm_store_ps((real32 *)Dest0++, Zero);
            _mm_store_ps((real32 *)Dest1++, Zero);
        }
    }

    // NOTE(georgy): Sum all sounds
    for(playing_sound **PlayingSoundPtr = &AudioState->FirstPlayingSound;
        *PlayingSoundPtr;
        )
    {
        playing_sound *PlayingSound = *PlayingSoundPtr;
        bool32 SoundFinished = false;

        uint32 TotalSamplesToMix = SoundBuffer->SampleCount;
        real32 *Dest0 = (real32 *)RealChannel0;
        real32 *Dest1 = (real32 *)RealChannel1;
            
        while(TotalSamplesToMix && !SoundFinished)
        {
            loaded_sound *LoadedSound = GetSound(Assets, PlayingSound->ID);
            if(LoadedSound)
            {
                asset_sound_info *Info = GetSoundInfo(Assets, PlayingSound->ID);
                PrefetchSound(Assets, Info->NextIDToPlay);

                v2 Volume = PlayingSound->CurrentVolume;
                v2 dVolume = SecondsPerSample*PlayingSound->dCurrentVolume;
                real32 dSample = PlayingSound->dSample;

                Assert(PlayingSound->SamplesPlayed >= 0);

                uint32 SamplesToMix = TotalSamplesToMix;
                real32 RealSampleRemainingInSound = (LoadedSound->SampleCount - RoundReal32ToInt32(PlayingSound->SamplesPlayed)) / dSample;
                uint32 SamplesRemainingInSound = RoundReal32ToUInt32(RealSampleRemainingInSound);
                if(SamplesToMix > SamplesRemainingInSound)
                {
                    SamplesToMix = SamplesRemainingInSound;
                }

                bool32 VolumeEnded[AudioStateOutputChannelCount] = {};
                for(uint32 ChannelIndex = 0;
                    ChannelIndex < ArrayCount(VolumeEnded);
                    ChannelIndex++)
                {
                    if(dVolume.E[ChannelIndex] != 0.0f)
                    {
                        real32 DeltaVolume = (PlayingSound->TargetVolume.E[ChannelIndex] - Volume.E[ChannelIndex]);
                        uint32 VolumeSampleCount = (uint32)((DeltaVolume / dVolume.E[ChannelIndex]) + 0.5f);
                        if(SamplesToMix > VolumeSampleCount)
                        {
                            SamplesToMix = VolumeSampleCount;
                            VolumeEnded[ChannelIndex] = true;
                        }
                    }                
                }

                // TODO(georgy): Handle stereo!
                // TODO(georgy): This is not correct yet! Need to truncate the loop!
                real32 SamplePos = PlayingSound->SamplesPlayed;
                for(uint32 LoopIndex = 0;
                    LoopIndex < SamplesToMix;
                    LoopIndex++)
                {
#if 1
                    uint32 SampleIndex = FloorReal32ToInt32(SamplePos);
                    real32 Frac = SamplePos - (real32)SampleIndex; 
                    real32 Sample0 = LoadedSound->Samples[0][SampleIndex];
                    real32 Sample1 = LoadedSound->Samples[0][SampleIndex + 1];
                    real32 SampleValue = Lerp(Sample0, Frac, Sample1);
#else
                    uint32 SampleIndex = RoundReal32ToUInt32(SamplePos);
                    real32 SampleValue = LoadedSound->Samples[0][SampleIndex];
#endif
                    *Dest0++ += AudioState->MasterVolume.E[0]*Volume.E[0]*SampleValue; 
                    *Dest1++ += AudioState->MasterVolume.E[1]*Volume.E[1]*SampleValue;

                    Volume += dVolume;
                    SamplePos += dSample;
                }

                PlayingSound->CurrentVolume = Volume;

                for(uint32 ChannelIndex = 0;
                    ChannelIndex < ArrayCount(VolumeEnded);
                    ChannelIndex++)
                {
                    if(VolumeEnded[ChannelIndex])
                    {
                        PlayingSound->CurrentVolume.E[ChannelIndex] =
                            PlayingSound->TargetVolume.E[ChannelIndex];
                        PlayingSound->dCurrentVolume.E[ChannelIndex] = 0.0f;
                    }
                }

                Assert(TotalSamplesToMix >= SamplesToMix);
                PlayingSound->SamplesPlayed = SamplePos;
                TotalSamplesToMix -= SamplesToMix;

                if((uint32)PlayingSound->SamplesPlayed == LoadedSound->SampleCount)
                {
                    if(IsValid(Info->NextIDToPlay))
                    {
                        PlayingSound->ID = Info->NextIDToPlay;
                        PlayingSound->SamplesPlayed = 0;
                    }
                    else
                    {
                        SoundFinished = true;
                    }
                }
            }
            else
            {
                LoadSound(Assets, PlayingSound->ID);
                break;
            }
        }

        if(SoundFinished)
        {
            *PlayingSoundPtr = PlayingSound->Next;
            PlayingSound->Next = AudioState->FirstFreePlayingSound;
            AudioState->FirstFreePlayingSound = PlayingSound;
        }
        else
        {
            PlayingSoundPtr = &PlayingSound->Next;
        }
    }

    // NOTE(georgy): Convert to 16-bit
    {
        __m128 *Source0 = RealChannel0;
        __m128 *Source1 = RealChannel1;

        __m128i *SampleOut = (__m128i *)SoundBuffer->Samples;
        for(uint32 SampleIndex = 0; 
            SampleIndex < SampleCount4; 
            SampleIndex++)
        {
            __m128 S0 = _mm_load_ps((real32 *)Source0++);
            __m128 S1 = _mm_load_ps((real32 *)Source1++);

            __m128i L = _mm_cvtps_epi32(S0);
            __m128i R = _mm_cvtps_epi32(S1);

            __m128i LR0 = _mm_unpacklo_epi32(L, R);
            __m128i LR1 = _mm_unpackhi_epi32(L, R);

            __m128i S01 = _mm_packs_epi32(LR0, LR1);

            *SampleOut++ = S01;
        }
    }

    EndTemporaryMemory(MixerMemory);
}

internal void
InitializeAudioState(audio_state *AudioState, memory_arena *PermArena)
{
    AudioState->PermArena = PermArena;
    AudioState->FirstPlayingSound = 0;
    AudioState->FirstFreePlayingSound = 0;

    AudioState->MasterVolume = V2(1.0f, 1.0f);
}
