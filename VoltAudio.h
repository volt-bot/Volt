#ifndef MUSIC_PLAYER_VOLTAUDIO_H
#define MUSIC_PLAYER_VOLTAUDIO_H

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <assert.h>

namespace VA {

    class Decoder {
    public:
    };

    // engine interfaces
    static SLObjectItf engineObject = nullptr;
    static SLEngineItf engineEngine;
    // output mix interfaces
    static SLObjectItf outputMixObject = nullptr;
    SLEnvironmentalReverbItf outputMixEnvironmentalReverb = nullptr;

    // aux effect on the output mix, used by the buffer queue player
    static const SLEnvironmentalReverbSettings reverbSettings = SL_I3DL2_ENVIRONMENT_PRESET_STONECORRIDOR;
    static const SLEnvironmentalReverbSettings reverbSettings2 =SL_I3DL2_ENVIRONMENT_PRESET_PLAIN;

    // URI player interfaces
    static SLObjectItf uriPlayerObject = nullptr;
    static SLPlayItf uriPlayerPlay;
    static SLSeekItf uriPlayerSeek;
    static SLMuteSoloItf uriPlayerMuteSolo;
    static SLVolumeItf uriPlayerVolume;
    static SLBassBoostItf uriPlayBassboost;
    static SLEqualizerItf equalizerItf;
    static SLPrefetchStatusItf prefetchStatusItf;

    static SLMetadataExtractionItf metadataExtractionItf;
    static SLMetadataTraversalItf metadataTraversalItf;
/* size of the struct to retrieve the PCM format metadata values: the values we're interested in
 * are SLuint32, but it is saved in the data field of a SLMetadataInfo, hence the larger size.
 * Nate that this size is queried and displayed at l.452 for demonstration/test purposes.
 *  */
#define PCM_METADATA_VALUE_SIZE 32
/* used to query metadata values */
static SLMetadataInfo *pcmMetaData = NULL;

    bool MYSLESINIT = false;

// create the engine and output mix objects
    bool native_createAudioEngine() {
        SLresult result;

        // create engine
        result = slCreateEngine(&engineObject, 0, nullptr, 0, nullptr, nullptr);
        assert(SL_RESULT_SUCCESS == result);
        if (SL_RESULT_SUCCESS == result) {
            MYSLESINIT = true;
        } else {
            MYSLESINIT = false;
        }
        (void) result;

        // realize the engine
        result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
        assert(SL_RESULT_SUCCESS == result);
        if (SL_RESULT_SUCCESS == result) {
            MYSLESINIT = true;
        } else {
            MYSLESINIT = false;
        }
        (void) result;

        // get the engine interface, which is needed in order to create other objects
        result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);
        assert(SL_RESULT_SUCCESS == result);
        if (SL_RESULT_SUCCESS == result) {
            MYSLESINIT = true;
        } else {
            MYSLESINIT = false;
        }
        (void) result;

        // create output mix, with environmental reverb specified as a non-required interface
        const SLInterfaceID ids[3] = {SL_IID_ENVIRONMENTALREVERB, SL_IID_BASSBOOST,
                                      SL_IID_PREFETCHSTATUS};
        const SLboolean req[] = {SL_BOOLEAN_FALSE, SL_BOOLEAN_FALSE,
                                 SL_BOOLEAN_FALSE};
        result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 3, ids, req);
        assert(SL_RESULT_SUCCESS == result);
        if (SL_RESULT_SUCCESS == result) {
            MYSLESINIT = true;
        } else {
            MYSLESINIT = false;
        }
        (void) result;

        // realize the output mix
        result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
        assert(SL_RESULT_SUCCESS == result);
        if (SL_RESULT_SUCCESS == result) {
            MYSLESINIT = true;
        } else {
            MYSLESINIT = false;
        }
        (void) result;

        // get the environmental reverb interface
        // this could fail if the environmental reverb effect is not available,
        // either because the feature is not present, excessive CPU load, or
        // the required MODIFY_AUDIO_SETTINGS permission was not requested and granted
        result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB,
                                                  &outputMixEnvironmentalReverb);
        if (SL_RESULT_SUCCESS == result) {
            SDL_Log("ENVIRONMENTALREVERB_INTERFACE: true");
            result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
                    outputMixEnvironmentalReverb, &reverbSettings2);
            (void) result;
        }

        result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_BASSBOOST,
                                                  &uriPlayBassboost);
        if (SL_RESULT_SUCCESS == result) {
            SDL_Log("BASSBOOST: true");
            result = (*uriPlayBassboost)->SetEnabled(uriPlayBassboost, false);
            //result = (*uriPlayBassboost)->SetStrength(uriPlayBassboost, 500);
            SLpermille bs;
            result = (*uriPlayBassboost)->GetRoundedStrength(uriPlayBassboost,&bs);
            SDL_Log("Bass Strength: %d",bs);
            (void) result;
        } else {
            SDL_Log("BASSBOOST: FALSE");
        }

        result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_EQUALIZER, &equalizerItf);
        if (SL_RESULT_SUCCESS == result) {
            (void) result;
        } else {
            SDL_Log("Equalizer: FALSE");
        }

        if (MYSLESINIT) {
            SDL_Log("OPEN_SLES_ENGINE_CREATED_SUCCESSFULY");
            return true;
        } else {
            SDL_Log("OPEN_SLES_ENGINE_INIT_FAILED!");
        }
        // ignore unsuccessful result codes for environmental reverb, as it is optional for this example
        return false;
    }


// create URI audio player
    bool createUriPlayer(const std::string &uri) {
        SLresult result;
        // convert Java string to UTF-8
        const char *utf8 = uri.c_str();
        assert(nullptr != utf8);

        // configure audio source
        // (requires the INTERNET permission depending on the uri parameter)
        SLDataLocator_URI loc_uri = {SL_DATALOCATOR_URI, (SLchar *) utf8};
        SLDataFormat_MIME format_mime = {SL_DATAFORMAT_MIME, nullptr, SL_CONTAINERTYPE_UNSPECIFIED};
        SLDataSource audioSrc = {&loc_uri, &format_mime};

        {
            /*const SLInterfaceID id_mt[1] = {SL_IID_METADATAEXTRACTION};
            const SLboolean req_mt[1] = {SL_BOOLEAN_FALSE};

            if (metadataObject != NULL) {
                (*metadataObject)->Destroy(metadataObject);
                metadataObject = NULL;
            }
            result = (*engineEngine)->CreateMetadataExtractor(engineEngine, &metadataObject, &audioSrc,
                                                              1,
                                                              id_mt, req_mt);
            //assert(SL_RESULT_SUCCESS == result);
            if (result == SL_RESULT_SUCCESS) {
                SDL_Log("MetaData Extractor created successfuly");
                result = (*metadataObject)->Realize(metadataObject, SL_BOOLEAN_FALSE);
                assert(SL_RESULT_SUCCESS == result);
                (void) result;

                result = (*metadataObject)->GetInterface(metadataObject, SL_IID_METADATAEXTRACTION,
                                                         &metadataExtractionItf);
                assert(SL_RESULT_SUCCESS == result);
                if (result == SL_RESULT_SUCCESS) {
                    SLuint32 iCnt;
                    result = (*metadataExtractionItf)->GetItemCount(metadataExtractionItf, &iCnt);
                    SDL_Log("MetaData ItemCount: %d", iCnt);
                }
                (void) result;
            } else if (result == SL_RESULT_FEATURE_UNSUPPORTED) {
                SDL_Log("MetaData Extractor Failed");
            }
            (void) result;*/
        }

        // configure audio sink
        SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
        SLDataSink audioSnk = {&loc_outmix, NULL};

        // create audio player
        const SLInterfaceID ids[8] = {SL_IID_SEEK, SL_IID_MUTESOLO, SL_IID_VOLUME,
                                      SL_IID_PREFETCHSTATUS, SL_IID_ANDROIDCONFIGURATION,
                                      SL_IID_METADATAEXTRACTION,
                                      SL_IID_METADATATRAVERSAL, SL_IID_EQUALIZER};
        const SLboolean req[8] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE,
                                  SL_BOOLEAN_TRUE,
                                  SL_BOOLEAN_TRUE,
                                  SL_BOOLEAN_FALSE, SL_BOOLEAN_FALSE, SL_BOOLEAN_TRUE};
        /* allocate memory to receive the PCM format metadata */
        /*if (!pcmMetaData) {
            pcmMetaData = (SLMetadataInfo*) malloc(PCM_METADATA_VALUE_SIZE);
        }*/

        if (uriPlayerObject != NULL) {
            (*uriPlayerObject)->Destroy(uriPlayerObject);
            uriPlayerObject = NULL;
        }
        result = (*engineEngine)->CreateAudioPlayer(engineEngine, &uriPlayerObject, &audioSrc,
                                                    &audioSnk, 8, ids, req);
        // note that an invalid URI is not detected here, but during prepare/prefetch on Android,
        // or possibly during Realize on other platforms
        assert(SL_RESULT_SUCCESS == result);
        if (SL_RESULT_SUCCESS == result) {
            SDL_Log("SLES_URI_PLAYER_CREATED");
        } else {
            SDL_Log("SLES_URI_PLAYER_FAILED_TO_CREATE");
        }
        (void) result;

        SLAndroidConfigurationItf playerConfig;
        result = (*uriPlayerObject)->GetInterface(uriPlayerObject,
                                                  SL_IID_ANDROIDCONFIGURATION, &playerConfig);
        assert(SL_RESULT_SUCCESS == result);
        SLint32 streamType = SL_ANDROID_STREAM_MEDIA;
        result = (*playerConfig)->SetConfiguration(playerConfig,
                                                   SL_ANDROID_KEY_STREAM_TYPE, &streamType,
                                                   sizeof(SLint32));
        if(SDL_GetAndroidSDKVersion()>=25) {
            // Set the performance mode.
            SLuint32 performanceMode = SL_ANDROID_PERFORMANCE_NONE;
            result = (*playerConfig)->SetConfiguration(playerConfig,
                                                       SL_ANDROID_KEY_PERFORMANCE_MODE,
                                                       &performanceMode, sizeof(performanceMode));
        }
        assert(SL_RESULT_SUCCESS == result);


        // release the Java string and UTF-8
        //delete utf8;

        // realize the player
        result = (*uriPlayerObject)->Realize(uriPlayerObject, SL_BOOLEAN_FALSE);
        // this will always succeed on Android, but we check result for portability to other platforms
        if (SL_RESULT_SUCCESS == result) {
            SDL_Log("SLES_URI_PLAYER_REALIZED");
        } else {
            SDL_Log("SLES_URI_PLAYER_FAILED_TO_REALIZE");
        }
        if (SL_RESULT_SUCCESS != result) {
            (*uriPlayerObject)->Destroy(uriPlayerObject);
            uriPlayerObject = NULL;
            return false;
        }

        result = (*uriPlayerObject)->GetInterface(uriPlayerObject, SL_IID_METADATAEXTRACTION,
                                                  &metadataExtractionItf);
        if (result == SL_RESULT_SUCCESS) {
            SDL_Log("MetaData Extractor created successfuly");
        } else {
            SDL_Log("MetaData Extractor Failed");
        }
        (void) result;

        result = (*uriPlayerObject)->GetInterface(uriPlayerObject, SL_IID_METADATATRAVERSAL,
                                                  &metadataTraversalItf);
        if (result == SL_RESULT_SUCCESS) {
            SDL_Log("MetaData traversal created successfuly");
        } else {
            SDL_Log("MetaData traversal Failed");
        }
        (void) result;

        // get the play interface
        result = (*uriPlayerObject)->GetInterface(uriPlayerObject, SL_IID_PLAY, &uriPlayerPlay);
        assert(SL_RESULT_SUCCESS == result);
        (void) result;

        // get the seek interface
        result = (*uriPlayerObject)->GetInterface(uriPlayerObject, SL_IID_SEEK, &uriPlayerSeek);
        assert(SL_RESULT_SUCCESS == result);
        (void) result;

        // get the mute/solo interface
        result = (*uriPlayerObject)->GetInterface(uriPlayerObject, SL_IID_MUTESOLO,
                                                  &uriPlayerMuteSolo);
        assert(SL_RESULT_SUCCESS == result);
        (void) result;

        // get the volume interface
        result = (*uriPlayerObject)->GetInterface(uriPlayerObject, SL_IID_VOLUME, &uriPlayerVolume);
        assert(SL_RESULT_SUCCESS == result);
        (void) result;

        result = (*uriPlayerObject)->GetInterface(uriPlayerObject, SL_IID_PREFETCHSTATUS,
                                                  &prefetchStatusItf);
        //assert(SL_RESULT_SUCCESS == result);
        if (result != SL_RESULT_SUCCESS) {
            SDL_Log("failed to prefetch");
            return false;
        }
        (void) result;

        result = (*uriPlayerObject)->GetInterface(uriPlayerObject, SL_IID_EQUALIZER, &equalizerItf);
        if (SL_RESULT_SUCCESS == result) {
            /*SDL_Log("Equalizer: true");
            SLmillibel min, max;
            result = (*equalizerItf)->GetBandLevelRange(equalizerItf, &min, &max);
            SDL_Log("Band Level Range min: %d, max %d", min, max);
            result = (*equalizerItf)->SetEnabled(equalizerItf, true);
            SLuint16 numBn;
            result = (*equalizerItf)->GetNumberOfBands(equalizerItf, &numBn);
            SDL_Log("Number of Bands: %d", numBn);
            SLuint16 nump;
            result = (*equalizerItf)->GetNumberOfPresets(equalizerItf, &nump);
            SDL_Log("Preset no: %d", nump);
            SLmilliHertz mhz1, mhz2;
            for (int i = 0; i < numBn; i++) {
                result = (*equalizerItf)->GetBandFreqRange(equalizerItf, i, &mhz1, &mhz2);
                SDL_Log("Band %d freq range: %d - %d", i, mhz1, mhz2);
            }*/
            /*SLuint16 nump;
            result = (*equalizerItf)->GetNumberOfPresets(equalizerItf, &nump);
            SDL_Log("Preset no: %d", nump);

            SLmilliHertz mhz1, mhz2;
            for (int i = 0; i < 5; i++) {
                result = (*equalizerItf)->GetBandFreqRange(equalizerItf, i, &mhz1, &mhz2);
                SDL_Log("Band %d freq range: %d - %d", i, mhz1, mhz2);
            }
            //result = (*equalizerItf)->GetBand(equalizerItf,60000,&nump);
            //SDL_Log("GET BAND 60000: %d", nump);
            for (int i = 0; i < 10; i++) {
                const SLchar *nm;
                result = (*equalizerItf)->GetPresetName(equalizerItf, i, &nm);
                SDL_Log("PresetName: %s", nm);
            }*/
            (void) result;
        } else {
            SDL_Log("Equalizer: FALSE");
        }

        return true;
    }

    void setEQEnabled(const bool &enabled) {
        SLresult result;
        if (equalizerItf != nullptr) {
            result = (*equalizerItf)->SetEnabled(equalizerItf, enabled);
        } else {
            SDL_LogError(1, "error: eqItf==NULL");
        }
    }

    void setBandLev(const SLuint16 &bandIndex, const SLmillibel &bandLev) {
        SLresult result;
        if (equalizerItf != NULL) {
            result = (*equalizerItf)->SetBandLevel(equalizerItf, bandIndex, bandLev);
        } else {
            SDL_LogError(1, "error: eqItf==NULL");
        }
    }

    void setCurrentPreset(const SLuint16 &index) {
        SLresult result;
        if (equalizerItf != NULL) {
            result = (*equalizerItf)->UsePreset(equalizerItf, index);
        } else {
            SDL_LogError(1, "error: eqItf==NULL");
        }
    }

    void getPresetName(const SLuint16 &index) {
        SLresult result;
        if (equalizerItf != NULL) {
            const SLchar *nm;
            result = (*equalizerItf)->GetPresetName(equalizerItf, index, &nm);
            SDL_Log("PresetName: %s", nm);
            //return 0;
        } else {
            SDL_LogError(1, "error: eqItf==NULL");
        }
        //return 0;
    }

    bool isEQEnabled() {
        SLresult result;
        if (equalizerItf != NULL) {
            SLboolean enabled;
            result = (*equalizerItf)->IsEnabled(equalizerItf, &enabled);
            return enabled;
        } else {
            SDL_LogError(1, "error: eqItf==NULL");
        }
        return false;
    }

    SLuint16 getNumBands() {
        SLresult result;
        if (equalizerItf != NULL) {
            SLuint16 numBand;
            result = (*equalizerItf)->GetNumberOfBands(equalizerItf, &numBand);
            return numBand;
        } else {
            SDL_LogError(1, "error: eqItf==NULL");
        }
        return 0;
    }

    SLmillibel getBandLev(SLuint16 bandIndex) {
        SLresult result;
        if (equalizerItf != NULL) {
            SLmillibel bandLev;
            result = (*equalizerItf)->GetBandLevel(equalizerItf, bandIndex, &bandLev);
            return bandLev;
        } else {
            SDL_LogError(1, "error: eqItf==NULL");
        }
        return 0;
    }

    SLuint16 getNumPresets() {
        SLresult result;
        if (equalizerItf != NULL) {
            SLuint16 numPresets;
            result = (*equalizerItf)->GetNumberOfPresets(equalizerItf, &numPresets);
            return numPresets;
        } else {
            SDL_LogError(1, "error: eqItf==NULL");
        }
        return 0;
    }

    SLuint16 getCurrentPreset() {
        SLresult result;
        if (equalizerItf != NULL) {
            SLuint16 curPres;
            result = (*equalizerItf)->GetCurrentPreset(equalizerItf, &curPres);
            return curPres;
        } else {
            SDL_LogError(1, "error: eqItf==NULL");
        }
        return 0;
    }

    SLmillibel getVol() {
        SLresult result;
        SLmillibel vlMill;
        result = (*uriPlayerVolume)->GetVolumeLevel(uriPlayerVolume, &vlMill);
        assert(SL_RESULT_SUCCESS == result);
        return vlMill;
    }


// set the playing state for the URI audio player
// to PLAYING (true) or PAUSED (false)
    bool setUriPlaying(const bool &isPlaying) {
        SLresult result;
        // make sure the URI audio player was created
        if (nullptr != uriPlayerPlay) {

            // set the player's state
            result = (*uriPlayerPlay)->SetPlayState(uriPlayerPlay, isPlaying ? SL_PLAYSTATE_PLAYING
                                                                             : SL_PLAYSTATE_PAUSED);
            if (SL_RESULT_SUCCESS == result)return true;
            (void) result;
        }
        return false;
    }

// set the whole file looping state for the URI audio player
    bool setLoopingUriPlayer(const bool &isLooping) {
        SLresult result;

        // make sure the URI audio player was created
        if (nullptr != uriPlayerSeek) {

            // set the looping state
            result = (*uriPlayerSeek)->SetLoop(uriPlayerSeek, (SLboolean) isLooping, 0,
                                               SL_TIME_UNKNOWN);
            if (SL_RESULT_SUCCESS == result)return true;
            (void) result;
        }
        return false;
    }

    void setMusicPosition(const uint32_t &arg_ms) {
        SLresult result;
        SLmicrosecond ms = arg_ms * 1000;
        // make sure the URI audio player was created
        if (nullptr != uriPlayerSeek) {
            result = (*uriPlayerSeek)->SetPosition(uriPlayerSeek, ms, SL_SEEKMODE_FAST);
            assert(SL_RESULT_SUCCESS == result);
            if (SL_RESULT_SUCCESS == result) {
            } else {
                SDL_Log("Failed to set pos");
            }
            (void) result;
        }
    }

    uint32_t getPrefetchStatus() {
        SLresult result;
        SLuint32 pfStatus;
        if (nullptr != prefetchStatusItf) {
            result = (*prefetchStatusItf)->GetPrefetchStatus(prefetchStatusItf, &pfStatus);
            //assert(SL_RESULT_SUCCESS == result);
            if (SL_RESULT_SUCCESS == result) {
                /*if (pfStatus==SL_PREFETCHSTATUS_UNDERFLOW)SDL_Log("SL_PREFETCHSTATUS_UNDERFLOW");
                if (pfStatus==SL_PREFETCHSTATUS_SUFFICIENTDATA)SDL_Log("SL_PREFETCHSTATUS_SUFFICIENTDATA");
                if (pfStatus==SL_PREFETCHSTATUS_OVERFLOW)SDL_Log("SL_PREFETCHSTATUS_OVERFLOW");*/
                return pfStatus;
            }
            else SDL_LogError(1, "failed to prefetch");
        }
        return 0;
    }

    SLuint32 getMetaItemCount() {
        SLresult result;
        SLuint32 iCnt;
        result = (*metadataExtractionItf)->GetItemCount(metadataExtractionItf, &iCnt);
        SDL_Log("MetaData ItemCount: %d", iCnt);
        return iCnt;
    }

// expose the mute/solo APIs to Java for the uri player

    SLMuteSoloItf getMuteSolo() {
        if (uriPlayerMuteSolo != nullptr)
            return uriPlayerMuteSolo;
    }


    void setChannelMuteUriPlayer(const int &chan, const bool &mute) {
        SLresult result;
        SLMuteSoloItf muteSoloItf = getMuteSolo();
        if (nullptr != muteSoloItf) {
            result = (*muteSoloItf)->SetChannelMute(muteSoloItf, chan, mute);
            assert(SL_RESULT_SUCCESS == result);
            (void) result;
        }
    }

    void setChannelSoloUriPlayer(const int &chan, const bool &solo) {
        SLresult result;
        SLMuteSoloItf muteSoloItf = getMuteSolo();
        if (nullptr != muteSoloItf) {
            result = (*muteSoloItf)->SetChannelSolo(muteSoloItf, chan, solo);
            assert(SL_RESULT_SUCCESS == result);
            (void) result;
        }
    }

    uint32_t getMusicDuration() {
        SLresult result;
        SLmicrosecond ms;
        // make sure the URI audio player was created
        if (nullptr != uriPlayerPlay) {
            result = (*uriPlayerPlay)->GetDuration(uriPlayerPlay, &ms);
            assert(SL_RESULT_SUCCESS == result);
            if (SL_RESULT_SUCCESS == result) {
                return ms;
            } else {
                SDL_Log("Failed to get Duration");
            }
            (void) result;
        }
        return 0;
    }

    uint32_t getMusicPosition() {
        SLresult result;
        SLmicrosecond ms;
        // make sure the URI audio player was created
        if (nullptr != uriPlayerPlay) {
            result = (*uriPlayerPlay)->GetPosition(uriPlayerPlay, &ms);
            assert(SL_RESULT_SUCCESS == result);
            if (SL_RESULT_SUCCESS == result) {
                //SDL_Log("Music Pos: %d", ms/1000);
                return ms;
            } else {
                SDL_Log("Failed to get pos");
            }
            (void) result;
        }
        return 0;
    }

    int getNumChannelsUriPlayer() {
        SLuint8 numChannels;
        SLresult result;
        SLMuteSoloItf muteSoloItf = getMuteSolo();
        if (nullptr != muteSoloItf) {
            result = (*muteSoloItf)->GetNumChannels(muteSoloItf, &numChannels);
            if (SL_RESULT_PRECONDITIONS_VIOLATED == result) {
                // channel count is not yet known
                numChannels = 0;
            } else {
                assert(SL_RESULT_SUCCESS == result);
            }
        } else {
            numChannels = 0;
        }
        return numChannels;
    }

// expose the volume APIs to Java for one of the 3 players

    SLVolumeItf getVolume() {
        if (uriPlayerVolume != nullptr)
            return uriPlayerVolume;
        return 0;
    }

    void setVolumeUriPlayer(SLmillibel millibel) {
        SLresult result;
        //SLVolumeItf volumeItf = getVolume();
        if (NULL != uriPlayerVolume) {
            result = (*uriPlayerVolume)->SetVolumeLevel(uriPlayerVolume, millibel);
            assert(SL_RESULT_SUCCESS == result);
            (void) result;
        }
    }


    void setMuteUriPlayer(bool mute) {
        SLresult result;
        SLVolumeItf volumeItf = getVolume();
        if (nullptr != volumeItf) {
            result = (*volumeItf)->SetMute(volumeItf, mute);
            assert(SL_RESULT_SUCCESS == result);
            (void) result;
        }
    }

    void enableStereoPositionUriPlayer(bool enable) {
        SLresult result;
        SLVolumeItf volumeItf = getVolume();
        if (nullptr != volumeItf) {
            result = (*volumeItf)->EnableStereoPosition(volumeItf, enable);
            assert(SL_RESULT_SUCCESS == result);
            (void) result;
        }
    }

    void setStereoPositionUriPlayer(int permille) {
        SLresult result;
        SLVolumeItf volumeItf = getVolume();
        if (nullptr != volumeItf) {
            result = (*volumeItf)->SetStereoPosition(volumeItf, permille);
            assert(SL_RESULT_SUCCESS == result);
            (void) result;
        }
    }

// shut down the native audio system
    void native_Audioshutdown() {

        // destroy URI audio player object, and invalidate all associated interfaces
        if (uriPlayerObject != nullptr) {
            (*uriPlayerObject)->Destroy(uriPlayerObject);
            uriPlayerObject = nullptr;
            uriPlayerPlay = nullptr;
            uriPlayerSeek = nullptr;
            uriPlayerMuteSolo = nullptr;
            uriPlayerVolume = nullptr;
        }

        // destroy output mix object, and invalidate all associated interfaces
        if (outputMixObject != nullptr) {
            (*outputMixObject)->Destroy(outputMixObject);
            outputMixObject = nullptr;
            outputMixEnvironmentalReverb = nullptr;
        }

        // destroy engine object, and invalidate all associated interfaces
        if (engineObject != nullptr) {
            (*engineObject)->Destroy(engineObject);
            engineObject = nullptr;
            engineEngine = nullptr;
        }
    }
}
#endif //MUSIC_PLAYER_VOLTAUDIO_H
