#include "../fmtlib/printf.h"
#include "common/common.h"
#include "../ini.h"
#include "../log.h"
#include <math.h>
#include "sound.h"
#include "common/common.h"
#include "inivals.h"


SFXEngine SFXEngine::SFXe;

void wrapper_setSampleVRP(void *, walsmpl *hSample, int rate, int volume, int pan)
{
    hSample->playback_rate(rate);
    hSample->volume(volume);
    hSample->pan(pan);
}

void wrapper_playSound(waldev *driver, walsmpl *hSample, void (*funceos)(void *), void *start, int len, int rate, int vol, int mastersnd, int pan, int loop_cnt, int pos)
{
    hSample->reset();
    hSample->EOS_callback(funceos);
    hSample->address(start, len, rate, AL_FORMAT_MONO8);
    hSample->setMasterVolume(mastersnd);
    hSample->volume(vol);
    hSample->pan(pan);
    hSample->loop_count(loop_cnt);
    hSample->play();

    if (pos > 0)
        hSample->position(pos);
}


int SFXEngine::init()
{
    digDriver = NULL;
    audio_volume = 0;
    audio_channels = 0;
    audio_num_palfx = 0;
    audio_num_shakefx = 0;

    audio_rev_stereo = false;
    palFxCleared = 0;
    currentTime = 0;
    audioRndIndex = 0;

    for (int i = 0; i < 64; i++)
        audioRndTable[i] = 0.0;

    musPlayer = NULL;
    musWait = false;
    musOn = false;
    musWaitSTime = 0;
    musWaitDelay = 0;
    musMinDelay = 0;
    musMaxDelay = 0;
    musTrack = 0;

    System::IniConf::ReadFromNucleusIni();

    audio_channels  = System::IniConf::AudioChannels.Get<int>();
    audio_volume    = System::IniConf::AudioVolume.Get<int>();
    audio_num_palfx = System::IniConf::AudioNumPalfx.Get<int>();
    audio_rev_stereo = System::IniConf::AudioRevStereo.Get<bool>();

    if ( audio_volume > 127 )
        audio_volume = 127;

    if ( audio_channels < 1 )
        audio_channels = 1;

    if ( audio_channels > AUDIO_CHANNELS)
        audio_channels = AUDIO_CHANNELS;

    if ( audio_num_palfx > 8 )
        audio_num_palfx = 8;

    audio_num_shakefx = 4;
    audioRndIndex = 0;

    for (int i = 0; i < 64; i++)
        audioRndTable[i] = (float)(16383 - (rand() & 0x7FFF) ) / 16383.0;

    digDriver = new waldev(); //miles_init(audio_channels);

    if ( digDriver->inited() )
    {
        for (int i = 0; i < audio_channels; i++)
        {
            snd_channels[i].hSample = digDriver->newSample(); //miles_allocSample(digDriver);
            snd_channels[i].hSample->setMasterVolume(audio_volume);

            if ( !snd_channels[i].hSample )
            {
                audio_channels = i;
                break;
            }
        }

        musPlayer = digDriver->createMusicPlayer();
        
        AudioStream = digDriver->CreateStream();
    }
    else
    {
        ypa_log_out("Couldn't initialize audio!\n");
    }

    return 1;
}

void SFXEngine::deinit()
{
    //deinit_redbook();

    if (digDriver)
    {
        delete digDriver;
        digDriver = NULL;
    }
}

void SFXEngine::setMasterVolume(int vol)
{
    audio_volume = vol;

    for (int i = 0; i < audio_channels; i++)
        snd_channels[i].hSample->setMasterVolume(audio_volume);

//    wrapper_setVolume(digDriver, vol);
}

void SFXEngine::setReverseStereo(bool rev)
{
    audio_rev_stereo = rev;
}

int SFXEngine::getMasterVolume()
{
    return audio_volume;
}


void SFXEngine::startSound(TSndCarrier *smpls, size_t id)
{
    if (id >= smpls->Sounds.size())
        return;
    
    TSoundSource *result = &smpls->Sounds.at(id);

    result->StartTime = currentTime;

    if ( result->IsFragmented() || result->PSample )
    {
        result->CurrentFrag = 0;
        result->SetEnabled(true);
        result->SetPlay(false);
    }
    if ( result->IsPFx() )
    {
        result->SetPFxEnable(true);
        result->SetPFxPlay(false);
        result->PFxMag = result->PPFx->mag0;
    }
    if ( result->IsShk() )
    {
        result->SetShkEnable(true);
        result->SetShkPlay(false);
        result->ShkMag = result->PShkFx->mag0;
    }
}

void SFXEngine::stopSound(TSndCarrier *smpls, size_t id)
{
    if (id >= smpls->Sounds.size())
        return;
    
    smpls->Sounds[id].SetShkEnable(false);
    smpls->Sounds[id].SetPFxEnable(false);
    smpls->Sounds[id].SetEnabled(false);
}




void SFXEngine::SetMusicIgnoreCommandsFlag(bool flag)
{
    if (!flag)
        musTrack = 0;

    musOn = flag;
}

void SFXEngine::SetMusicVolume(int vol)
{
    if (digDriver && musPlayer && musOn)
    {
        musPlayer->volume(vol);
    }
}

void SFXEngine::StopMusicTrack(bool reset)
{
    if (digDriver && musPlayer && musOn)
    {
        musPlayer->stop();
        if (reset)
            musTrack = 0;
    }
}

void SFXEngine::PlayMusicTrack()
{
    if (digDriver && musPlayer && musOn  && musTrack > 0)
    {
        musPlayer->play();
    }
}

void SFXEngine::SetMusicTrack(int trackID, int minDelay, int maxDelay)
{
    if (digDriver && musPlayer && musOn && trackID > 0)
    {
        if ( musPlayer->open( fmt::sprintf("music/%d.ogg", trackID) ) )
        {
            musTrack = trackID;
            musMinDelay = minDelay;
            musMaxDelay = maxDelay;
        }
        else
            musTrack = 0;
    }
}

// Shutoff samples set
void SFXEngine::StopCarrier(TSndCarrier *carr)
{
    for (int i = 0; i < audio_channels; i++)
    {
        if ( digDriver )
        {
            if (snd_channels[i].sndSouce && snd_channels[i].sndSouce->PCarrier == carr )
            {
                snd_channels[i].sndSouce->SetPlay(false);
                snd_channels[i].sndSouce->SetEnabled(false);

                snd_channels[i].hSample->stop();

                snd_channels[i].sndSouce = NULL;
            }

            if (soundSources[i] && soundSources[i]->PCarrier == carr )
            {
                soundSources[i]->SetPlay(false);
                soundSources[i]->SetEnabled(false);
                soundSources[i] = NULL;
            }
        }
    }

    for (int i = 0; i < audio_num_palfx; i++)
    {
        if ( palFXs[i] && palFXs[i]->PCarrier == carr )
        {
            palFXs[i]->SetPFxEnable(false);
            palFXs[i]->SetPFxPlay(false);
            palFXs[i] = NULL;
        }
    }

    for (int i = 0; i < audio_num_shakefx; i++)
    {
        if ( ShakeFXs[i] && ShakeFXs[i]->PCarrier == carr )
        {
            ShakeFXs[i]->SetShkEnable(false);
            ShakeFXs[i]->SetShkPlay(false);
            ShakeFXs[i] = NULL;
        }
    }

    for (TSoundSource &snd : carr->Sounds)
    {
        snd.SetShkEnable(false);
        snd.SetPFxEnable(false);
        snd.SetEnabled(false);
    }
}

void SFXEngine::StopSource(TSoundSource *snd)
{
    for (int i = 0; i < audio_channels; i++)
    {
        if ( digDriver )
        {
            if (snd_channels[i].sndSouce == snd )
            {
                snd_channels[i].sndSouce->SetPlay(false);
                snd_channels[i].sndSouce->SetEnabled(false);

                snd_channels[i].hSample->stop();

                snd_channels[i].sndSouce = NULL;
            }

            if (soundSources[i] == snd )
            {
                soundSources[i]->SetPlay(false);
                soundSources[i]->SetEnabled(false);
                soundSources[i] = NULL;
            }
        }
    }

    for (int i = 0; i < audio_num_palfx; i++)
    {
        if ( palFXs[i] == snd )
        {
            palFXs[i]->SetPFxEnable(false);
            palFXs[i]->SetPFxPlay(false);
            palFXs[i] = NULL;
        }
    }

    for (int i = 0; i < audio_num_shakefx; i++)
    {
        if ( ShakeFXs[i] == snd )
        {
            ShakeFXs[i]->SetShkEnable(false);
            ShakeFXs[i]->SetShkPlay(false);
            ShakeFXs[i] = NULL;
        }
    }
}

float SFXEngine::audio_rnd()
{
    float tmp = audioRndTable[audioRndIndex];

    audioRndIndex++;

    if ( audioRndIndex >= 64 )
        audioRndIndex = 0;

    return tmp;
}

void SFXEngine::audio_InsertSoundSource(TSoundSource *smpl)
{
    int min_i = -1;
    int minPriority = 1000000;

    if ( digDriver->inited() )
    {
        for (int i = 0; i < audio_channels; i++)
        {
            if ( soundSources[i] )
            {
                if ( soundSources[i]->Priority < minPriority )
                {
                    min_i = i;
                    minPriority = soundSources[i]->Priority;
                }
            }
            else
            {
                min_i = i;
                minPriority = 0;
                i = audio_channels;
            }
        }

        if ( min_i == -1 )
            ypa_log_out("-> audio_InsertSoundSource(): <min_i> not initialized.\n");

        if ( min_i != -1 && minPriority < smpl->Priority )
            soundSources[min_i] = smpl;
    }
}

int SFXEngine::FindFragmentAt(TSoundSource *smpl, int elapsed, int *fragTime)
{
    int i = 0;

    for (TSampleParams &pr : *(smpl->PFragments))
    {
        if ( !pr.Loop )
            break;

        int rate = pr.SampleRate + pr.Sample->SampleRate + smpl->Pitch;

        if ( rate <= 0 )
            rate = 1;

        int fragDuration = (pr.Loop * pr.rlSmplCnt / 1024) / rate;

        if ( elapsed < fragDuration )
            break;

        elapsed -= fragDuration;
        
        i++;
    }

    if ( fragTime )
        *fragTime = elapsed;

    return i;
}

void SFXEngine::SoundCarrierProcessSounds(TSndCarrier *smpls, float distance)
{
    float attenuation = distance / 200.0;

    for (TSoundSource &snd : smpls->Sounds)
    {
        if ( snd.PSample || snd.IsFragmented() )
        {
            size_t fragId = 0;

            if ( snd.IsEnabled() )
            {
                int elapsed = currentTime - snd.StartTime;

                if ( snd.IsFragmented() )
                {
                    if ( snd.IsPlay() )
                        fragId = snd.CurrentFrag;
                    else
                        fragId = FindFragmentAt(&snd, currentTime - snd.StartTime, NULL);
                }

                bool ok = true;

                if ( !snd.IsLoop() && !snd.IsPlay() && elapsed > 0 )
                {
                    if ( snd.IsFragmented() )
                    {
                        if ( fragId >= snd.PFragments->size() )
                        {
                            snd.SetEnabled(false);
                            ok = false;
                        }
                    }
                    else if ( (elapsed * (snd.Pitch + snd.PSample->SampleRate) / 1024) > snd.PSample->bufsz )
                    {
                        snd.SetEnabled(false);
                        ok = false;
                    }
                }

                if (ok)
                {
                    snd.ResultVol = snd.Volume;

                    if ( snd.IsFragmented() )
                        snd.ResultVol += snd.PFragments->at(fragId).Vol;

                    if ( attenuation >= 1.0 )
                        snd.ResultVol /= attenuation;

                    if ( snd.ResultVol > 4 )
                    {
                        snd.Priority = snd.ResultVol;
                        audio_InsertSoundSource(&snd);
                    }
                }

            }
        }
    }
}


void SFXEngine::audio_InsertPalFX(TSoundSource *smpl)
{
    int min_i = -1;
    int minMag = 1000000;

    for (int i = 0; i < audio_num_palfx; i++)
    {
        if ( palFXs[i] )
        {
            // TODO(bug): Never compares with minMag, so with no free slot this picks the last
            // slot with a non-zero PFxMag, not the weakest. InsertShakeFX compares.
            if ( palFXs[i]->PFxMag )
            {
                minMag = palFXs[i]->PFxMag;
                min_i = i;
            }
        }
        else
        {
            min_i = i;
            minMag = 0;
            i = audio_num_palfx;
        }
    }

    if ( min_i == -1 )
        ypa_log_out("-> audio_InsertPalFX(): <min_i> not initialized.\n");

    if ( min_i != -1 && minMag < smpl->PFxMag )
        palFXs[min_i] = smpl;
}

void SFXEngine::SoundCarrierProcessPFx(TSndCarrier *smpls, float distance)
{
    float attenuation = distance / 300.0;

    for (TSoundSource &snd : smpls->Sounds)
    {
        if ( snd.IsPFx() )
        {
            if ( snd.IsPFxEnabled() )
            {
                if ( !snd.IsLoop() && (size_t)(snd.StartTime + snd.PPFx->time) < currentTime )
                    snd.SetPFxEnable(false);
            }

            if ( snd.IsPFxEnabled() )
            {
                if ( snd.IsLoop() )
                    snd.PFxMag = snd.PPFx->mag0;
                else
                    snd.PFxMag = (snd.PPFx->mag1 - snd.PPFx->mag0) * ((float)(currentTime - snd.StartTime) / (float)snd.PPFx->time) + snd.PPFx->mag0;

                if ( attenuation >= 1.0 )
                    snd.PFxMag /= attenuation;

                audio_InsertPalFX(&snd);
            }
        }
    }
}

void SFXEngine::InsertShakeFX(TSoundSource *smpl)
{
    int min_i = -1;
    int minMag = 1000;

    for (int i = 0; i < audio_num_shakefx; i++)
    {
        TSoundSource *fx = ShakeFXs[i];

        if ( fx )
        {
            if ( minMag > fx->ShkMag )
            {
                minMag = fx->ShkMag;
                min_i = i;
            }
        }
        else
        {
            min_i = i;
            minMag = 0;
            i = audio_num_shakefx;
        }
    }

    if ( min_i == -1 )
        ypa_log_out("-> audio_InsertShakeFX(): <min_i> not initialized.\n");

    if ( min_i != -1 && minMag < smpl->ShkMag )
        ShakeFXs[min_i] = smpl;
}

void SFXEngine::SoundCarrierProcessShake(TSndCarrier *smpls, float distance)
{
    for (TSoundSource &snd : smpls->Sounds)
    {
        if ( snd.IsShk() )
        {
            if ( snd.IsShkEnabled() )
            {
                if ( !snd.IsLoop() && (size_t)(snd.StartTime + snd.PShkFx->time) < currentTime )
                    snd.SetShkEnable(false);
            }

            if ( snd.IsShkEnabled() )
            {
                if ( snd.IsLoop() )
                    snd.ShkMag = snd.PShkFx->mag0;
                else
                    snd.ShkMag = (snd.PShkFx->mag1 - snd.PShkFx->mag0) * ((float)(currentTime - snd.StartTime) / (float)snd.PShkFx->time) + snd.PShkFx->mag0;

                float attenuation = distance * snd.PShkFx->mute;

                if ( attenuation >= 1.0 )
                    snd.ShkMag /= attenuation;

                InsertShakeFX(&snd);
            }
        }
    }
}

// Insert new sound
void SFXEngine::UpdateSoundCarrier(TSndCarrier *smpls)
{
    float distance = (smpls->Position - listenerPos).length();

    if ( distance < 6000.0 )
    {
        if ( digDriver->inited() )
            SoundCarrierProcessSounds(smpls, distance);

        if ( distance < 2400.0 )
        {
            if ( audio_num_palfx )
                SoundCarrierProcessPFx(smpls, distance);

            SoundCarrierProcessShake(smpls, distance);
        }
    }
}

void SFXEngine::FreeEndedChannels()
{
    //Free ended channels
    for (int i = 0; i < audio_channels; i++)
    {
        TSoundSource *src = snd_channels[i].sndSouce;

        if ( src )
        {
            int orphaned = 1;

            for (int j = 0; j < audio_channels; j++)
            {
                if ( src == soundSources[j] )
                {
                    orphaned = 0;
                    break;
                }
            }

            if ( orphaned )
            {
                src->SetPlay(false);

                snd_channels[i].sndSouce = NULL;

                snd_channels[i].hSample->stop();
            }
        }
    }
}

void SFXEngine::AssignFreeChannels()
{
    //Find free snd_channel
    int chanIdx = 0;

    for (int i = 0; i < audio_channels; i++)
    {
        TSoundSource *src = soundSources[i];

        if ( src && !src->IsPlay() )
        {
            while ( chanIdx < audio_channels )
            {
                if ( !snd_channels[chanIdx].sndSouce )
                {
                    snd_channels[chanIdx].sndSouce = src;
                    break;
                }
                else
                {
                    chanIdx++;
                }
            }

            if ( chanIdx == audio_channels )
                break;
        }
    }
}

void SFXEngine::CalcPlayParams(TSoundSource *smpl)
{
    TSndCarrier *carrier = smpl->PCarrier;

    vec3d toListener = listenerPos - carrier->Position;
    float dist = toListener.length();

    vec3d relVel = carrier->Vector - listenerVelocity;
    float relSpeed = relVel.length();

    float distSpeed = dist  *  relSpeed;
    float cosAngle = 0.0;

    if ( distSpeed > 0.0 )
        cosAngle = toListener.dot( relVel ) / distSpeed;

    int baseRate;

    if ( smpl->IsFragmented() )
        baseRate = smpl->PFragments->at(smpl->CurrentFrag).SampleRate + smpl->PFragments->at(smpl->CurrentFrag).Sample->SampleRate + smpl->Pitch;
    else
        baseRate = smpl->Pitch + smpl->PSample->SampleRate;

    if ( baseRate < 2000 )
        baseRate = 2000;
    else if ( baseRate > 44100 )
        baseRate = 44100;

    float sideDist = listenerRotation.AxisX().dot( toListener );

    smpl->ResultRate = baseRate + baseRate * (int)(cosAngle * relSpeed) / 400;

    float pan;

    if ( dist <= 0.0 )
    {
        pan = 0.0;
    }
    else if ( audio_rev_stereo )
    {
        pan = sideDist / dist;
    }
    else
    {
        pan = sideDist / -dist;
    }

    smpl->ResultPan = (pan * 64.0 + 64.0);

    if ( smpl->ResultVol < 0 )
        smpl->ResultVol = 0;
    else if ( smpl->ResultVol > 127 )
        smpl->ResultVol = 127;

    if ( smpl->ResultPan < 0 )
        smpl->ResultPan = 0;
    else if ( smpl->ResultPan > 127 )
        smpl->ResultPan = 127;

    if ( baseRate / 2 <= smpl->ResultRate )
    {
        if ( smpl->ResultRate > baseRate * 2)
            smpl->ResultRate = baseRate * 2;
    }
    else
    {
        smpl->ResultRate = baseRate / 2;
    }
}

void SFXEngine::sound_eos_clbk(void *_smpl)
{
    walsmpl *smpl = static_cast<walsmpl *>(_smpl);

    int chanIdx = 0;
    TSoundSource *src = NULL;

    for (int i = 0; i < SFXe.audio_channels; i++)
    {
        if ( smpl == SFXe.snd_channels[i].hSample )
        {
            chanIdx = i;
            src = SFXe.snd_channels[i].sndSouce;
            break;
        }
    }

    if ( src )
    {
        if ( src->IsEnabled() )
        {
            if ( src->IsFragmented() )
            {
                src->CurrentFrag++;

                if ( src->CurrentFrag >= src->PFragments->size() )
                {
                    if ( src->IsLoop() )
                        src->CurrentFrag = 0;
                    else
                        src->SetEnabled(false);
                }

                if ( src->IsEnabled() )
                {
                    // TODO(bug): No break, so src is put on every free channel and smpl ends up as the
                    // last free one. The extra channels stay held by src and play nothing.
                    for (int i = 0; i < SFXe.audio_channels; i++)
                    {
                        if ( !SFXe.snd_channels[i].sndSouce )
                        {
                            smpl = SFXe.snd_channels[i].hSample;
                            SFXe.snd_channels[chanIdx].sndSouce = NULL;
                            SFXe.snd_channels[i].sndSouce = src;
                        }
                    }

                    TSampleParams *frag = &src->PFragments->at( src->CurrentFrag );

                    wrapper_playSound(
                        SFXe.digDriver,
                        smpl,
                        sound_eos_clbk,
                        (char *)frag->Sample->Data + frag->rlOffset,
                        frag->rlSmplCnt,
                        src->ResultRate,
                        src->ResultVol,
                        SFXe.audio_volume,
                        src->ResultPan,
                        frag->Loop,
                        0);
                }
            }
            else
            {
                src->SetEnabled(false);
            }
        }
    }
}


void SFXEngine::StartChannel(int id, TSoundSource *smpl)
{
    walsmpl *hwSample = snd_channels[id].hSample;

    smpl->SetPlay(true);

    if ( smpl->IsFragmented() )
    {
        int elapsed = currentTime - smpl->StartTime;
        int startOffset;

        if ( elapsed <= 0 )
        {
            smpl->CurrentFrag = 0;
            startOffset = 0;
        }
        else
        {
            int fragTime;
            size_t fragIdx = FindFragmentAt(smpl, elapsed, &fragTime);

            if ( fragIdx == smpl->PFragments->size() )
                fragIdx = smpl->PFragments->size() - 1;

            startOffset = ((smpl->ResultRate * fragTime) / 1024) % smpl->PFragments->at(fragIdx).rlSmplCnt;
            smpl->CurrentFrag = fragIdx;
        }

        TSampleParams *frag = &smpl->PFragments->at(smpl->CurrentFrag);

        wrapper_playSound(
            digDriver,
            hwSample,
            sound_eos_clbk,
            (char *)frag->Sample->Data + frag->rlOffset,
            frag->rlSmplCnt,
            smpl->ResultRate,
            smpl->ResultVol,
            audio_volume,
            smpl->ResultPan,
            frag->Loop,
            startOffset);
    }
    else
    {
        wrapper_playSound(
            digDriver,
            hwSample,
            sound_eos_clbk,
            smpl->PSample->Data,
            smpl->PSample->bufsz,
            smpl->ResultRate,
            smpl->ResultVol,
            audio_volume,
            smpl->ResultPan,
            (smpl->IsLoop() ? 0 : 1),
            (int)((smpl->PSample->SampleRate + smpl->Pitch) * (currentTime - smpl->StartTime) >> 10) % smpl->PSample->bufsz);
    }
}

void SFXEngine::UpdateChannels()
{
    for (int i = 0; i < audio_channels; i++)
    {
        TSoundSource *src = snd_channels[i].sndSouce;
        walsmpl *hwSample = snd_channels[i].hSample;

        if ( src )
        {
            if ( src->PSample || src->IsFragmented() )
            {
                CalcPlayParams(snd_channels[i].sndSouce);

                if ( src->IsPlay() )
                    wrapper_setSampleVRP(digDriver, hwSample, src->ResultRate, src->ResultVol, src->ResultPan);
                else
                    StartChannel(i, src);
            }
            else
            {
                ypa_log_out("AUDIO ENGINE ERROR #1!\n");
            }
        }
    }
}

void SFXEngine::UpdatePaletteEffects()
{
    float pwr = 1.0;
    std::vector<GFX::ColorFx> fxes;

    int i = 0;
    for (i = 0; i < audio_num_palfx; i++)
    {
        TSoundSource *src = palFXs[i];

        if ( !src )
            break;
        
        float magpwr = src->PFxMag * GFX::Engine.GetColorEffectPower(src->PPFx->slot);

        pwr -= magpwr;
        
        fxes.emplace_back(src->PPFx->slot, magpwr);
    }

    if ( i )
    {
        palFxCleared = 0;
    }
    else
    {
        if ( palFxCleared )
            return;

        palFxCleared = 1;
    }

    if ( pwr > 0.0 )
        fxes.emplace_back(0, pwr);

    GFX::Engine.SetColorEffectsPowers(fxes);
}

void SFXEngine::UpdateShake()
{
    vec3d tmp(0.0, 0.0, 0.0);

    int i = 0;

    for (i = 0; i < audio_num_shakefx && ShakeFXs[i]; i++)
    {
        TSoundSource *src = ShakeFXs[i];

        tmp.x += audio_rnd() * src->ShkMag * src->PShkFx->pos.x;
        tmp.y += audio_rnd() * src->ShkMag * src->PShkFx->pos.y;
        tmp.z += audio_rnd() * src->ShkMag * src->PShkFx->pos.z;
    }

    if ( i > 0 )
        shakeMatrix = mat3x3::Euler_ZXY(tmp);
    else
        shakeMatrix = mat3x3::Ident();
}

void SFXEngine::UpdateMusic()
{
    if (musPlayer && musOn)
    {
        if (musWait)
        {
            if (musWaitSTime + musWaitDelay <= currentTime)
            {
                musWait = false;
                musPlayer->stop();
                musPlayer->play();
            }
        }
        else
        {
            if( musPlayer->isStopped() )
            {
                if( musTrack ) // if audio track was setted - replay it, but do some delay
                {
                    musWait = true;
                    musWaitSTime = currentTime;
                    musWaitDelay = musMinDelay + (musMaxDelay - musMinDelay) * (currentTime % 30) / 30;
                }
            }
        }
    }
}


const mat3x3 &SFXEngine::EndSoundFrame()
{
    if ( digDriver->inited() )
    {
        FreeEndedChannels();
        AssignFreeChannels();
        UpdateChannels();
    }

    if ( audio_num_palfx )
        UpdatePaletteEffects();

    UpdateShake();

    UpdateMusic();

    return shakeMatrix;
}

void SFXEngine::BeginSoundFrame(int dtime, const vec3d &pos, const vec3d &velocity, const mat3x3 &rotation)
{
    currentTime += dtime;

    listenerPos = pos;
    listenerVelocity = velocity;
    listenerRotation = rotation;

    soundSources.fill(NULL);
    palFXs.fill(NULL);
    ShakeFXs.fill(NULL);
}

void SFXEngine::StopPlayingSounds()
{
    if ( digDriver )
    {
        for (int i = 0; i < audio_channels; i++)
        {

            if (snd_channels[i].sndSouce )
            {
                snd_channels[i].sndSouce->SetPlay(false);
                snd_channels[i].sndSouce->SetEnabled(false);

                snd_channels[i].hSample->stop();

                snd_channels[i].sndSouce = NULL;
            }
        }
    }

    for (int i = 0; i < audio_channels; i++)
    {
        if (soundSources[i] )
        {
            soundSources[i]->SetPlay(false);
            soundSources[i]->SetEnabled(false);
            soundSources[i] = NULL;
        }
    }

    for (int i = 0; i < audio_num_palfx; i++)
    {
        if ( palFXs[i] )
        {
            palFXs[i]->SetPFxEnable(false);
            palFXs[i]->SetPFxPlay(false);
            palFXs[i] = NULL;
        }
    }

    for (int i = 0; i < audio_num_shakefx; i++)
    {
        if ( ShakeFXs[i] )
        {
            ShakeFXs[i]->SetShkEnable(false);
            ShakeFXs[i]->SetShkPlay(false);
            ShakeFXs[i] = NULL;
        }
    }
}

TSoundSource *SFXEngine::SndGetTopShake()
{
    return ShakeFXs[0];
}
