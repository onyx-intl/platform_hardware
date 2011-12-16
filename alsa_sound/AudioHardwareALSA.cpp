/* AudioHardwareALSA.cpp
 **
 ** Copyright 2008-2009 Wind River Systems
 **
 ** Licensed under the Apache License, Version 2.0 (the "License");
 ** you may not use this file except in compliance with the License.
 ** You may obtain a copy of the License at
 **
 **     http://www.apache.org/licenses/LICENSE-2.0
 **
 ** Unless required by applicable law or agreed to in writing, software
 ** distributed under the License is distributed on an "AS IS" BASIS,
 ** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 ** See the License for the specific language governing permissions and
 ** limitations under the License.
 */

/* Copyright (C) 2010-2011 Freescale Semiconductor,Inc. */

#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>

#define LOG_TAG "AudioHardwareALSA"
#include <utils/Log.h>
#include <utils/String8.h>

#include <cutils/properties.h>
#include <media/AudioRecord.h>
#include <hardware_legacy/power.h>

#include "AudioHardwareALSA.h"
#define SGTL5000 "imx-3stack"
#define SPDIF    "imx-3stack-spdif"
#define MAXCARDSNUM  2

extern "C"
{
    //
    // Function for dlsym() to look up for creating a new AudioHardwareInterface.
    //
    android::AudioHardwareInterface *createAudioHardware(void) {
        return android::AudioHardwareALSA::create();
    }
}         // extern "C"

namespace android
{

// ----------------------------------------------------------------------------

static void ALSAErrorHandler(const char *file,
                             int line,
                             const char *function,
                             int err,
                             const char *fmt,
                             ...)
{
    char buf[BUFSIZ];
    va_list arg;
    int l;

    va_start(arg, fmt);
    l = snprintf(buf, BUFSIZ, "%s:%i:(%s) ", file, line, function);
    vsnprintf(buf + l, BUFSIZ - l, fmt, arg);
    buf[BUFSIZ-1] = '\0';
    LOGE("ALSALib %s", buf);
    va_end(arg);
}

AudioHardwareInterface *AudioHardwareALSA::create() {
    return new AudioHardwareALSA();
}

static void show_control_id(snd_ctl_elem_id_t *id)
{
	unsigned int index, device, subdevice;
	const char *hctlname;
	hctlname = snd_ctl_elem_id_get_name(id);
	LOGV("numid=%u,name='%s'",
	       snd_ctl_elem_id_get_numid(id),
	       hctlname);
	index = snd_ctl_elem_id_get_index(id);
	device = snd_ctl_elem_id_get_device(id);
	subdevice = snd_ctl_elem_id_get_subdevice(id);
	if (index)
		LOGV(",index=%i", index);
	if (device)
		LOGV(",device=%i", device);
	if (subdevice)
		LOGV(",subdevice=%i", subdevice);

	/* For we just need to get to know that whether there is a control for "Playback" and "control"
	 * we just pick up these two elements
	 */
	if (strstr(hctlname,"Playback Volume")|| strstr(hctlname, "Capture Volume"))
		LOGD("found placback/capture volume");
}
static int findSoundCards(char **cardname)
{
	int idx, dev, err;
	snd_ctl_t *handle;
	snd_hctl_t *hctlhandle;
	snd_ctl_card_info_t *cardinfo;
	snd_pcm_info_t *pcminfo;
	char str[32];

	snd_ctl_card_info_alloca(&cardinfo);
	snd_pcm_info_alloca(&pcminfo);

	snd_hctl_elem_t *elem;
	snd_ctl_elem_id_t *id;
	snd_ctl_elem_info_t *info;
	snd_ctl_elem_id_alloca(&id);
	snd_ctl_elem_info_alloca(&info);

	idx = -1;
	while (1) {
		if ((err = snd_card_next(&idx)) < 0) {
			LOGE("Card next error: %s\n", snd_strerror(err));
			break;
		}
		if (idx < 0)
			break;
		sprintf(str, "hw:CARD=%i", idx);
		if ((err = snd_ctl_open(&handle, str, 0)) < 0) {
			LOGE("Open error: %s\n", snd_strerror(err));
			continue;
		}
		if ((err = snd_ctl_card_info(handle, cardinfo)) < 0) {
			LOGE("HW info error: %s\n", snd_strerror(err));
			continue;
		}
		LOGD("Soundcard #%i:\n", idx + 1);
		LOGD("  card - %i\n", snd_ctl_card_info_get_card(cardinfo));
		LOGD("  id - '%s'\n", snd_ctl_card_info_get_id(cardinfo));
		LOGD("  driver - '%s'\n", snd_ctl_card_info_get_driver(cardinfo));
		LOGD("  name - '%s'\n", snd_ctl_card_info_get_name(cardinfo));
		LOGD("  longname - '%s'\n", snd_ctl_card_info_get_longname(cardinfo));
		LOGD("  mixername - '%s'\n", snd_ctl_card_info_get_mixername(cardinfo));
		LOGD("  components - '%s'\n", snd_ctl_card_info_get_components(cardinfo));

		strcpy(cardname[idx], snd_ctl_card_info_get_name(cardinfo));
		LOGD("\n\n-----get cart name and id: %s : %d",cardname[idx],idx);
		snd_ctl_close(handle);

		if ((err = snd_hctl_open(&hctlhandle, str, 0)) < 0) {
			LOGE("Control %s open error: %s", str, snd_strerror(err));
			return err;
		}
		if ((err = snd_hctl_load(hctlhandle)) < 0) {
			LOGE("Control %s local error: %s\n", str, snd_strerror(err));
			return err;
		}

		for (elem = snd_hctl_first_elem(hctlhandle); elem; elem = snd_hctl_elem_next(elem)) {
			if ((err = snd_hctl_elem_info(elem, info)) < 0) {
				LOGE("Control %s snd_hctl_elem_info error: %s\n", str, snd_strerror(err));
				return err;
			}
			snd_hctl_elem_get_id(elem, id);
			show_control_id(id);
		}
		snd_hctl_close(hctlhandle);
	}
	snd_config_update_free_global();
	return 0;
}

AudioHardwareALSA::AudioHardwareALSA() :
    mMixer(0),
    mMixerSpdif(0),
    mMixerSgtl5000(0),
    mALSADevice(0),
    mAcousticDevice(0)
{
    snd_lib_error_set_handler(&ALSAErrorHandler);
    hw_module_t *module;
    char snd_sgtl5000[32], snd_spdif[32];
    char **cardname = new char*[MAXCARDSNUM];
    for (int i = 0; i < MAXCARDSNUM; i++) {
	   cardname[i] = new char[128];
	   memset(cardname[i],0,128);
    }
    int id;
    int err = hw_get_module(ALSA_HARDWARE_MODULE_ID,
            (hw_module_t const**)&module);

    if (err == 0) {
        hw_device_t* device;
        err = module->methods->open(module, ALSA_HARDWARE_NAME, &device);
        if (err == 0) {
            mALSADevice = (alsa_device_t *)device;
            mALSADevice->init(mALSADevice, mDeviceList);
        } else
            LOGE("ALSA Module could not be opened!!!");
    } else
        LOGE("ALSA Module not found!!!");

    /* found out sound cards in the system  and new mixer controller for them*/
    err = findSoundCards(cardname);
    if (err == 0) {
        for (id = 0; id < MAXCARDSNUM; id++) {
            if(cardname[id] && strstr(cardname[id],SPDIF)){
                LOGD("  CARD NAME: %s ID %d", cardname[id],id);
                sprintf(snd_spdif,"hw:0%d",id);
                sprintf(snd_spdif,"hw:CARD=%d",id);
                mMixerSpdif    = new ALSAMixer(snd_spdif);
	       }else if (cardname[id] && strstr(cardname[id],SGTL5000)){
                LOGD("  CARD NAME: %s ID %d", cardname[id],id);
                sprintf(snd_sgtl5000,"hw:0%d",id);
                sprintf(snd_sgtl5000,"hw:CARD=%d",id);
                mMixerSgtl5000 = new ALSAMixer(snd_sgtl5000);
            }
        }
    } else {
        LOGE("Don't find any Sound cards, use default");
        mMixerSgtl5000 = new ALSAMixer("hw:00");
        mMixerSpdif    = new ALSAMixer("hw:00");
    }
    for (int i = 0; i < MAXCARDSNUM; i++) {
        delete []cardname[i];
    }
    delete []cardname;

    mCurCard    = new char[128];
    if (!mCurCard)
	   LOGE("allocate memeory to store current sound card name fail");
    memset(mCurCard,0,sizeof(mCurCard));
    /* set current card as sgtl5000 default */
    if(mMixerSgtl5000)
    {
        strcpy(mCurCard,SGTL5000);
        mMixer = mMixerSgtl5000;
    }else if(mMixerSpdif)
    {
        strcpy(mCurCard,SPDIF);
        mMixer = mMixerSpdif;        
    }

    err = hw_get_module(ACOUSTICS_HARDWARE_MODULE_ID,
            (hw_module_t const**)&module);

    if (err == 0) {
        hw_device_t* device;
        err = module->methods->open(module, ACOUSTICS_HARDWARE_NAME, &device);
        if (err == 0)
            mAcousticDevice = (acoustic_device_t *)device;
        else
            LOGE("Acoustics Module not found.");
    }
}

AudioHardwareALSA::~AudioHardwareALSA()
{
    if (mMixer) delete mMixer;
    if (mMixerSpdif) delete mMixerSpdif;
    if (mMixerSgtl5000) delete mMixerSgtl5000;
    if (mCurCard) delete []mCurCard;
    if (mALSADevice)
        mALSADevice->common.close(&mALSADevice->common);
    if (mAcousticDevice)
        mAcousticDevice->common.close(&mAcousticDevice->common);
}

status_t AudioHardwareALSA::initCheck()
{
    if (mALSADevice && mMixer && mMixer->isValid())
        return NO_ERROR;
    else
        return NO_INIT;
}

status_t AudioHardwareALSA::setVoiceVolume(float volume)
{
    /* could not set this params on SPDIF card, will return directly */
    if (!strcmp(mCurCard,SPDIF))
        return INVALID_OPERATION;
    // The voice volume is used by the VOICE_CALL audio stream.

    if (mMixer)
        return mMixer->setVolume(AudioSystem::DEVICE_OUT_EARPIECE, volume, volume);
    else
        return INVALID_OPERATION;
}

status_t AudioHardwareALSA::setMasterVolume(float volume)
{
    /* could not set this params on SPDIF card, will return directly */
    if (!strcmp(mCurCard,SPDIF))
        return INVALID_OPERATION;
    if (mMixer)
        return mMixer->setMasterVolume(volume);
    else
        return INVALID_OPERATION;
}

status_t AudioHardwareALSA::setMode(int mode)
{
    status_t status = NO_ERROR;

    if (mode != mMode) {
        status = AudioHardwareBase::setMode(mode);

        if (status == NO_ERROR) {
            // take care of mode change.
            for(ALSAHandleList::iterator it = mDeviceList.begin();
                it != mDeviceList.end(); ++it) {
                status = mALSADevice->route(&(*it), it->curDev, mode);
                if (status != NO_ERROR)
                    break;
            }
        }
    }

    return status;
}

AudioStreamOut *
AudioHardwareALSA::openOutputStream(uint32_t devices,
                                    int *format,
                                    uint32_t *channels,
                                    uint32_t *sampleRate,
                                    status_t *status)
{
    AutoMutex lock(mLock);

    LOGD("openOutputStream called for devices: 0x%08x", devices);

    status_t err = BAD_VALUE;
    AudioStreamOutALSA *out = 0;

    if (devices & (devices - 1)) {
        if (status) *status = err;
        LOGD("openOutputStream called with bad devices");
        return out;
    }

    // Find the appropriate alsa device
    for(ALSAHandleList::iterator it = mDeviceList.begin();
        it != mDeviceList.end(); ++it)
        if (it->devices & devices) {
            err = mALSADevice->open(&(*it), devices, mode());
            if (err) break;
            if (devices & AudioSystem::DEVICE_OUT_WIRED_HDMI){
                strcpy(mCurCard ,SPDIF);
                mMixer = mMixerSpdif;
            } else {
                strcpy(mCurCard,SGTL5000);
                mMixer = mMixerSgtl5000;
            }

            out = new AudioStreamOutALSA(this, &(*it));
            err = out->set(format, channels, sampleRate);
            break;
        }

    if (status) *status = err;
    return out;
}

void
AudioHardwareALSA::closeOutputStream(AudioStreamOut* out)
{
    AutoMutex lock(mLock);
    delete out;
}

AudioStreamIn *
AudioHardwareALSA::openInputStream(uint32_t devices,
                                   int *format,
                                   uint32_t *channels,
                                   uint32_t *sampleRate,
                                   status_t *status,
                                   AudioSystem::audio_in_acoustics acoustics)
{
    AutoMutex lock(mLock);

    status_t err = BAD_VALUE;
    AudioStreamInALSA *in = 0;

    if (devices & (devices - 1)) {
        if (status) *status = err;
        return in;
    }

    // Find the appropriate alsa device
    for(ALSAHandleList::iterator it = mDeviceList.begin();
        it != mDeviceList.end(); ++it)
        if (it->devices & devices) {
            err = mALSADevice->open(&(*it), devices, mode());
            if (err) break;
            if (devices & AudioSystem::DEVICE_OUT_WIRED_HDMI){
                strcpy(mCurCard ,SPDIF);
                mMixer = mMixerSpdif;
            } else {
                strcpy(mCurCard,SGTL5000);
                mMixer = mMixerSgtl5000;
            }

            in = new AudioStreamInALSA(this, &(*it), acoustics);
            //set the format, channels, sampleRate to 0, so that it can make use of
            //the parameter from the hardware default config
            *format = 0;
            *channels = 0;
            *sampleRate = 0;
            err = in->set(format, channels, sampleRate);
            LOGW("the hardware default channels  is %d, sampleRate is %d, format is %d", *channels, *sampleRate, *format);
            break;
        }

    if (status) *status = err;
    return in;
}

void
AudioHardwareALSA::closeInputStream(AudioStreamIn* in)
{
    AutoMutex lock(mLock);
    delete in;
}

status_t AudioHardwareALSA::setMicMute(bool state)
{
    /* could not set this params on SPDIF card, will return directly */
    if (!strcmp(mCurCard,SPDIF))
        return INVALID_OPERATION;
    if (mMixer)
        return mMixer->setCaptureMuteState(AudioSystem::DEVICE_OUT_EARPIECE, state);

    return NO_INIT;
}

status_t AudioHardwareALSA::getMicMute(bool *state)
{
    /* could not set this params on SPDIF card, will return directly */
    if (!strcmp(mCurCard,SPDIF))
        return INVALID_OPERATION;
    if (mMixer)
        return mMixer->getCaptureMuteState(AudioSystem::DEVICE_OUT_EARPIECE, state);

    return NO_ERROR;
}

status_t AudioHardwareALSA::dump(int fd, const Vector<String16>& args)
{
    return NO_ERROR;
}

}       // namespace android
