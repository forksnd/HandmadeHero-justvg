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

    Assert((SoundBuffer->SampleCount & 7) == 0);
    uint32 SampleCount8 = SoundBuffer->SampleCount / 8;
    uint32 SampleCount4 = SoundBuffer->SampleCount / 4;

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

        uint32 TotalSamplesToMix8 = SampleCount8;
        __m128 *Dest0 = RealChannel0;
        __m128 *Dest1 = RealChannel1;
            
        while(TotalSamplesToMix8 && !SoundFinished)
        {
            loaded_sound *LoadedSound = GetSound(Assets, PlayingSound->ID);
            if(LoadedSound)
            {
                asset_sound_info *Info = GetSoundInfo(Assets, PlayingSound->ID);
                PrefetchSound(Assets, Info->NextIDToPlay);

                v2 Volume = PlayingSound->CurrentVolume;
                v2 dVolume = SecondsPerSample*PlayingSound->dCurrentVolume;
                v2 dVolume8 = 8.0f*dVolume;
                real32 dSample = PlayingSound->dSample;
                real32 dSample8 = 8.0f*dSample;

                __m128 MasterVolume4_0 = _mm_set1_ps(AudioState->MasterVolume.E[0]);
                __m128 MasterVolume4_1 = _mm_set1_ps(AudioState->MasterVolume.E[1]);
                __m128 Volume4_0 = _mm_setr_ps(Volume.E[0] + 0.0f*dVolume.E[0],
                                               Volume.E[0] + 1.0f*dVolume.E[0],
                                               Volume.E[0] + 2.0f*dVolume.E[0],
                                               Volume.E[0] + 3.0f*dVolume.E[0]);
                __m128 dVolume4_0 = _mm_set1_ps(dVolume.E[0]);
                __m128 dVolume84_0 = _mm_set1_ps(dVolume8.E[0]);
                __m128 Volume4_1 = _mm_setr_ps(Volume.E[1] + 0.0f*dVolume.E[1],
                                               Volume.E[1] + 1.0f*dVolume.E[1],
                                               Volume.E[1] + 2.0f*dVolume.E[1],
                                               Volume.E[1] + 3.0f*dVolume.E[1]);                                               
                __m128 dVolume4_1 = _mm_set1_ps(dVolume.E[1]);
                __m128 dVolume84_1 = _mm_set1_ps(dVolume8.E[1]);

                Assert(PlayingSound->SamplesPlayed >= 0);

                uint32 SamplesToMix8 = TotalSamplesToMix8;
                real32 RealSamplesRemainingInSound8 = (LoadedSound->SampleCount - RoundReal32ToInt32(PlayingSound->SamplesPlayed)) / dSample8;
                uint32 SamplesRemainingInSound8 = RoundReal32ToUInt32(RealSamplesRemainingInSound8);
                if(SamplesToMix8 > SamplesRemainingInSound8)
                {
                    SamplesToMix8 = SamplesRemainingInSound8;
                }

                bool32 VolumeEnded[AudioStateOutputChannelCount] = {};
                for(uint32 ChannelIndex = 0;
                    ChannelIndex < ArrayCount(VolumeEnded);
                    ChannelIndex++)
                {
                    if(dVolume8.E[ChannelIndex] != 0.0f)
                    {
                        real32 DeltaVolume = (PlayingSound->TargetVolume.E[ChannelIndex] - Volume.E[ChannelIndex]);
                        uint32 VolumeSampleCount8 = (uint32)((DeltaVolume / dVolume8.E[ChannelIndex]) + 0.5f);
                        if(SamplesToMix8 > VolumeSampleCount8)
                        {
                            SamplesToMix8 = VolumeSampleCount8;
                            VolumeEnded[ChannelIndex] = true;
                        }
                    }                
                }

                // TODO(georgy): Handle stereo!
                // TODO(georgy): This is not correct yet! Need to truncate the loop!
                real32 SamplePos = PlayingSound->SamplesPlayed;
                for(uint32 LoopIndex = 0;
                    LoopIndex < SamplesToMix8;
                    LoopIndex++)
                {
#if 0
                    real32 OffsetSamplePos = SamplePos + (real32)SampleOffset*dSample;
                    uint32 SampleIndex = FloorReal32ToInt32(OffsetSamplePos);
                    real32 Frac = OffsetSamplePos - (real32)SampleIndex; 

                    real32 Sample0 = LoadedSound->Samples[0][SampleIndex];
                    real32 Sample1 = LoadedSound->Samples[0][SampleIndex + 1];
                    real32 SampleValue = Lerp(Sample0, Frac, Sample1);
#else
                    __m128 SampleValue_0 = _mm_setr_ps(LoadedSound->Samples[0][RoundReal32ToUInt32(SamplePos + 0.0f*dSample)],
                                                       LoadedSound->Samples[0][RoundReal32ToUInt32(SamplePos + 1.0f*dSample)],
                                                       LoadedSound->Samples[0][RoundReal32ToUInt32(SamplePos + 2.0f*dSample)],
                                                       LoadedSound->Samples[0][RoundReal32ToUInt32(SamplePos + 3.0f*dSample)]);
                    __m128 SampleValue_1 = _mm_setr_ps(LoadedSound->Samples[0][RoundReal32ToUInt32(SamplePos + 4.0f*dSample)],
                                                       LoadedSound->Samples[0][RoundReal32ToUInt32(SamplePos + 5.0f*dSample)],
                                                       LoadedSound->Samples[0][RoundReal32ToUInt32(SamplePos + 6.0f*dSample)],
                                                       LoadedSound->Samples[0][RoundReal32ToUInt32(SamplePos + 7.0f*dSample)]);
#endif
                    __m128 D0_0 = _mm_load_ps((real32 *)&Dest0[0]);
                    __m128 D0_1 = _mm_load_ps((real32 *)&Dest0[1]);
                    __m128 D1_0 = _mm_load_ps((real32 *)&Dest1[0]);
                    __m128 D1_1 = _mm_load_ps((real32 *)&Dest1[1]);

                    D0_0 = _mm_add_ps(D0_0, _mm_mul_ps(_mm_mul_ps(MasterVolume4_0, Volume4_0), SampleValue_0));
                    D0_1 = _mm_add_ps(D0_1, _mm_mul_ps(_mm_mul_ps(MasterVolume4_0, _mm_add_ps(Volume4_0, dVolume84_0)), SampleValue_0));
                    D1_0 = _mm_add_ps(D1_0, _mm_mul_ps(_mm_mul_ps(MasterVolume4_1, Volume4_1), SampleValue_1));
                    D1_1 = _mm_add_ps(D1_1, _mm_mul_ps(_mm_mul_ps(MasterVolume4_1, _mm_add_ps(Volume4_1, dVolume84_1)), SampleValue_1));

                    _mm_store_ps((real32 *)&Dest0[0], D0_0);
                    _mm_store_ps((real32 *)&Dest0[1], D0_1);
                    _mm_store_ps((real32 *)&Dest1[0], D1_0);
                    _mm_store_ps((real32 *)&Dest1[1], D1_1);

                    Dest0 += 2;
                    Dest1 += 2;
                    Volume4_0 = _mm_add_ps(Volume4_0, dVolume84_0);
                    Volume4_1 = _mm_add_ps(Volume4_1, dVolume84_1);
                    Volume += dVolume8;
                    SamplePos += dSample8;
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

                PlayingSound->SamplesPlayed = SamplePos;
                Assert(TotalSamplesToMix8 >= SamplesToMix8);
                TotalSamplesToMix8 -= SamplesToMix8;

                if((uint32)PlayingSound->SamplesPlayed >= LoadedSound->SampleCount)
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
