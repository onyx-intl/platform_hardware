/* alsa_imx51.cpp
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

/* Copyright 2010-2011 Freescale Semiconductor Inc. */

#define LOG_TAG "iMX ALSA"
#include <utils/Log.h>

#include "AudioHardwareALSA.h"
#include <media/AudioRecord.h>

#include <cutils/properties.h>
//#define BLUETOOTH_SCO_DEVICE "hw:0,1"
//#define FM_TRANSMIT_DEVICE "hw:0,2"

#ifndef ALSA_DEFAULT_SAMPLE_RATE
#define ALSA_DEFAULT_SAMPLE_RATE 44100 // in Hz
#endif

#define DEVICE_DEFAULT    0
#define DEVICE_SPDIF      1
#define DEVICE_SGTL5000   2
#define DEVICE_WM8958     3


#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

namespace android
{

static int s_device_open(const hw_module_t*, const char*, hw_device_t**);
static int s_device_close(hw_device_t*);
static status_t s_init(alsa_device_t *, ALSAHandleList &);
static status_t s_open(alsa_handle_t *, uint32_t, int);
static status_t s_close(alsa_handle_t *);
static status_t s_route(alsa_handle_t *, uint32_t, int);

char spdifcardname[32];
char sgtlcardname[32];
char wm8958cardname[32];    
int  selecteddevice ;


    
static hw_module_methods_t s_module_methods = {
    open            : s_device_open
};

extern "C" const hw_module_t HAL_MODULE_INFO_SYM = {
    tag             : HARDWARE_MODULE_TAG,
    version_major   : 1,
    version_minor   : 0,
    id              : ALSA_HARDWARE_MODULE_ID,
    name            : "i.MX51 ALSA module",
    author          : "Freescale Semiconductor",
    methods         : &s_module_methods,
    dso             : 0,
    reserved        : {0,},
};

static int s_device_open(const hw_module_t* module, const char* name,
        hw_device_t** device)
{
    alsa_device_t *dev;
    dev = (alsa_device_t *) malloc(sizeof(*dev));
    if (!dev) return -ENOMEM;

    memset(dev, 0, sizeof(*dev));

    /* initialize the procs */
    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (hw_module_t *) module;
    dev->common.close = s_device_close;
    dev->init = s_init;
    dev->open = s_open;
    dev->close = s_close;
    dev->route = s_route;

    *device = &dev->common;

    LOGD("i.MX51 ALSA module opened");

    return 0;
}

static int s_device_close(hw_device_t* device)
{
    free(device);
    return 0;
}

// ----------------------------------------------------------------------------

static const int DEFAULT_SAMPLE_RATE = ALSA_DEFAULT_SAMPLE_RATE;

static void setDefaultControls(uint32_t devices, int mode, const char *cardname);

typedef void (*AlsaControlSet)(uint32_t devices, int mode, const char *cardname);

#define IMX51_OUT_CODEC_DEFAULT   (\
        AudioSystem::DEVICE_OUT_EARPIECE | \
        AudioSystem::DEVICE_OUT_SPEAKER | \
        AudioSystem::DEVICE_OUT_WIRED_HEADSET | \
        AudioSystem::DEVICE_OUT_WIRED_HEADPHONE | \
        AudioSystem::DEVICE_OUT_BLUETOOTH_A2DP | \
        AudioSystem::DEVICE_OUT_BLUETOOTH_A2DP_HEADPHONES | \
        AudioSystem::DEVICE_OUT_BLUETOOTH_A2DP_SPEAKER | \
        AudioSystem::DEVICE_OUT_DEFAULT \
	)

#define IMX51_OUT_SPDIF_DEFAULT   (\
        AudioSystem::DEVICE_OUT_WIRED_HDMI  \
	)

#define IMX51_IN_CODEC_DEFAULT    (\
        AudioSystem::DEVICE_IN_ALL &\
	~AudioSystem::DEVICE_IN_BLUETOOTH_SCO_HEADSET \
	)

static alsa_handle_t _defaults[] = {
    {
        module      : 0,
        devices     : IMX51_OUT_CODEC_DEFAULT,
        curDev      : 0,
        curMode     : 0,
        handle      : 0,
        format      : SND_PCM_FORMAT_S16_LE, // AudioSystem::PCM_16_BIT
        channels    : 2,
        sampleRate  : DEFAULT_SAMPLE_RATE,
        //latency     : 200000, // Desired Delay in usec
        //bufferSize  : 2048, // Desired Number of samples
        latency     : 250000, // Desired Delay in usec
        bufferSize  : 6144, // Desired Number of samples
        modPrivate  : (void *)&setDefaultControls,
    },
    {
        module      : 0,
        devices     : IMX51_OUT_SPDIF_DEFAULT,
        curDev      : 0,
        curMode     : 0,
        handle      : 0,
        format      : SND_PCM_FORMAT_S16_LE, // AudioSystem::PCM_16_BIT
        channels    : 2,
        sampleRate  : DEFAULT_SAMPLE_RATE,
        latency     : 200000, // Desired Delay in usec
        bufferSize  : 6144, // Desired Number of samples
        modPrivate  : (void *)&setDefaultControls,
    },
    {
        module      : 0,
        devices     : IMX51_IN_CODEC_DEFAULT,
        curDev      : 0,
        curMode     : 0,
        handle      : 0,
        format      : SND_PCM_FORMAT_S16_LE, // AudioSystem::PCM_16_BIT
        channels    : 2,
        sampleRate  : DEFAULT_SAMPLE_RATE,
        latency     : 250000, // Desired Delay in usec
        bufferSize  : 6144, // Desired Number of samples
        modPrivate  : (void *)&setDefaultControls,
    },
};

// ----------------------------------------------------------------------------

snd_pcm_stream_t direction(alsa_handle_t *handle)
{
    return (handle->devices & AudioSystem::DEVICE_OUT_ALL) ? SND_PCM_STREAM_PLAYBACK
            : SND_PCM_STREAM_CAPTURE;
}

//card_device =0, return the card name, card_device=1, return the card device name
const char *deviceName(alsa_handle_t *alsa_handle, uint32_t device, int mode, int card_device)
{

	snd_ctl_t *handle;
	int card, err, dev, idx;
	snd_ctl_card_info_t *info;
	snd_pcm_info_t *pcminfo;
	snd_ctl_card_info_alloca(&info);
	snd_pcm_info_alloca(&pcminfo);
    int  cardnum = 0;
    char value[PROPERTY_VALUE_MAX];
    snd_pcm_stream_t stream = direction(alsa_handle);
    bool havespdifdevice = false;
    bool havesgtldevice =false;
    bool havewm8958device =false; 
        
	card = -1;
	if (snd_card_next(&card) < 0 || card < 0) {
		LOGD("no soundcards found...");
		return "default";
	}
	LOGD("**** List of %s Hardware Devices ****\n",
	       snd_pcm_stream_name(stream));
	while (card >= 0) {
		char name[32];
		sprintf(name, "hw:%d", card);
		if ((err = snd_ctl_open(&handle, name, 0)) < 0) {
			LOGD("control open (%i): %s", card, snd_strerror(err));
			goto next_card;
		}
		if ((err = snd_ctl_card_info(handle, info)) < 0) {
			LOGD("control hardware info (%i): %s", card, snd_strerror(err));
			snd_ctl_close(handle);
			goto next_card;
		}
		dev = -1;
		while (1) {
			unsigned int count;
			if (snd_ctl_pcm_next_device(handle, &dev)<0)
				LOGD("snd_ctl_pcm_next_device");
			if (dev < 0)
				break;
			snd_pcm_info_set_device(pcminfo, dev);
			snd_pcm_info_set_subdevice(pcminfo, 0);
			snd_pcm_info_set_stream(pcminfo, stream);
			if ((err = snd_ctl_pcm_info(handle, pcminfo)) < 0) {
				if (err != -ENOENT)
					LOGD("control digital audio info (%i): %s", card, snd_strerror(err));
				continue;
			}
			
			LOGD("card %i: %s [%s], device %i: %s [%s]\n",
				card, snd_ctl_card_info_get_id(info), snd_ctl_card_info_get_name(info),
				dev,
				snd_pcm_info_get_id(pcminfo),
				snd_pcm_info_get_name(pcminfo));
                			
			if(strcmp(snd_pcm_info_get_id(pcminfo),"IMX SPDIF mxc spdif-0")==0) {
			     if(card_device==0)  sprintf(spdifcardname, "hw:0%d", card);
			     else         		 sprintf(spdifcardname, "hw:%d,%d", card, dev);
			     havespdifdevice =  true;
			}
            
			if(strcmp(snd_pcm_info_get_id(pcminfo),"SGTL5000 SGTL5000-0")==0) {
			     if(card_device==0) sprintf(sgtlcardname, "hw:0%d", card);
			     else               sprintf(sgtlcardname, "hw:%d,%d", card, dev);
			     havesgtldevice =  true;                
			}
			if(strcmp(snd_pcm_info_get_id(pcminfo),"WM8994 HiFi WM8994 AIF1-0")==0) {
			     if(card_device==0) sprintf(wm8958cardname, "hw:0%d", card);
			     else               sprintf(wm8958cardname, "hw:%d,%d", card, dev);
			     havewm8958device =  true;                
			}
			cardnum++;
		}
		snd_ctl_close(handle);
	next_card:

		if (snd_card_next(&card) < 0) {
			LOGD("snd_card_next");
			break;
		}
	}
        

    property_get("ro.HDMI_AUDIO_OUTPUT", value, "");
    if((device & AudioSystem::DEVICE_OUT_WIRED_HDMI) && havespdifdevice && (strcmp(value, "1") == 0))
    {
        selecteddevice = DEVICE_SPDIF;
        return spdifcardname;

    }else if(havesgtldevice)
    {
        selecteddevice = DEVICE_SGTL5000;
        return sgtlcardname;
        
    }else if(havewm8958device)
    {
        selecteddevice = DEVICE_WM8958;
        return wm8958cardname;
    }    
    selecteddevice = DEVICE_DEFAULT;
    return "default";
}



const char *streamName(alsa_handle_t *handle)
{
    return snd_pcm_stream_name(direction(handle));
}

status_t setHardwareParams(alsa_handle_t *handle)
{
    snd_pcm_hw_params_t *hardwareParams;
    status_t err;
    snd_pcm_access_mask_t *mask;

    snd_pcm_uframes_t bufferSize = handle->bufferSize;
    unsigned int requestedRate = handle->sampleRate;
    unsigned int latency = handle->latency;

    // snd_pcm_format_description() and snd_pcm_format_name() do not perform
    // proper bounds checking.
    bool validFormat = (static_cast<int> (handle->format)
            > SND_PCM_FORMAT_UNKNOWN) && (static_cast<int> (handle->format)
            <= SND_PCM_FORMAT_LAST);
    const char *formatDesc = validFormat ? snd_pcm_format_description(
            handle->format) : "Invalid Format";
    const char *formatName = validFormat ? snd_pcm_format_name(handle->format)
            : "UNKNOWN";

    if (snd_pcm_hw_params_malloc(&hardwareParams) < 0) {
        LOG_ALWAYS_FATAL("Failed to allocate ALSA hardware parameters!");
        return NO_INIT;
    }

    err = snd_pcm_hw_params_any(handle->handle, hardwareParams);
    if (err < 0) {
        LOGE("Unable to configure hardware: %s", snd_strerror(err));
        goto done;
    }

    // Set the interleaved read and write format.
    mask = (snd_pcm_access_mask_t *)malloc(snd_pcm_access_mask_sizeof());
    snd_pcm_access_mask_none(mask);
    snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_INTERLEAVED);
    snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_NONINTERLEAVED);
    snd_pcm_access_mask_set(mask, SND_PCM_ACCESS_MMAP_COMPLEX);
    err = snd_pcm_hw_params_set_access_mask(handle->handle, hardwareParams, mask);

    if (err < 0) {
        LOGW("Unable to enable MMAP access for PCM: %s", snd_strerror(err));
        err = snd_pcm_hw_params_set_access(handle->handle, hardwareParams,
                SND_PCM_ACCESS_RW_INTERLEAVED);
        if (err < 0) {
            LOGE("Unable to configure PCM read/write format: %s",
                    snd_strerror(err));
            free(mask);
            goto done;
        }
        handle->mmap = 0;
    } else {
        handle->mmap = 1;
        LOGW("enable MMAP access for PCM");
    }
    free(mask);

    err = snd_pcm_hw_params_set_format(handle->handle, hardwareParams,
            handle->format);
    if (err < 0) {
        LOGE("Unable to configure PCM format %s (%s): %s",
                formatName, formatDesc, snd_strerror(err));
        goto done;
    }

    LOGV("Set %s PCM format to %s (%s)", streamName(handle), formatName, formatDesc);

    err = snd_pcm_hw_params_set_channels(handle->handle, hardwareParams,
            handle->channels);
    if (err < 0) {
        LOGE("Unable to set channel count to %i: %s",
                handle->channels, snd_strerror(err));
        goto done;
    }

    LOGV("Using %i %s .", handle->channels,
            handle->channels == 1 ? "channel" : "channels");

    err = snd_pcm_hw_params_set_rate_near(handle->handle, hardwareParams,
            &requestedRate, 0);

    if (err < 0)
        LOGE("Unable to set %s sample rate to %u: %s",
                streamName(handle), handle->sampleRate, snd_strerror(err));
    else if (requestedRate != handle->sampleRate)
        // Some devices have a fixed sample rate, and can not be changed.
        // This may cause resampling problems; i.e. PCM playback will be too
        // slow or fast.
        LOGW("Requested rate (%u HZ) does not match actual rate (%u HZ)",
                handle->sampleRate, requestedRate);
    else
        LOGV("Set sample rate to %u HZ", requestedRate);

    // Make sure we have at least the size we originally wanted
    err = snd_pcm_hw_params_set_buffer_size(handle->handle, hardwareParams,
            bufferSize);
    if (err < 0) {
        LOGE("Unable to set buffer size to %d:  %s",
                (int)bufferSize, snd_strerror(err));
        goto done;
    }

    // Setup buffers for latency
    err = snd_pcm_hw_params_set_buffer_time_near(handle->handle,
            hardwareParams, &latency, NULL);
    if (err < 0) {
        /* That didn't work, set the period instead */
        unsigned int periodTime = latency / 4;
        err = snd_pcm_hw_params_set_period_time_near(handle->handle,
                hardwareParams, &periodTime, NULL);
        if (err < 0) {
            LOGE("Unable to set the period time for latency: %s", snd_strerror(err));
            goto done;
        }
        snd_pcm_uframes_t periodSize;
        err = snd_pcm_hw_params_get_period_size(hardwareParams, &periodSize,
                NULL);
        if (err < 0) {
            LOGE("Unable to get the period size for latency: %s", snd_strerror(err));
            goto done;
        }
        bufferSize = periodSize * 4;
        if (bufferSize < handle->bufferSize) bufferSize = handle->bufferSize;
        err = snd_pcm_hw_params_set_buffer_size_near(handle->handle,
                hardwareParams, &bufferSize);
        if (err < 0) {
            LOGE("Unable to set the buffer size for latency: %s", snd_strerror(err));
            goto done;
        }
        LOGV("Setup buffers time near for latency failed %d", latency);
    } else {
        // OK, we got buffer time near what we expect. See what that did for bufferSize.
        err = snd_pcm_hw_params_get_buffer_size(hardwareParams, &bufferSize);
        if (err < 0) {
            LOGE("Unable to get the buffer size for latency: %s", snd_strerror(err));
            goto done;
        }
        // Does set_buffer_time_near change the passed value? It should.
        err = snd_pcm_hw_params_get_buffer_time(hardwareParams, &latency, NULL);
        if (err < 0) {
            LOGE("Unable to get the buffer time for latency: %s", snd_strerror(err));
            goto done;
        }
        unsigned int periodTime = latency / 4;
        err = snd_pcm_hw_params_set_period_time_near(handle->handle,
                hardwareParams, &periodTime, NULL);
        if (err < 0) {
            LOGE("Unable to set the period time for latency: %s", snd_strerror(err));
            goto done;
        }
        LOGV("Setup buffers time near for latency ok %d", latency);
    }

    LOGV("Buffer size: %d", (int)bufferSize);
    LOGV("Latency: %d", (int)latency);

    handle->bufferSize = bufferSize;
    handle->latency = latency;

    // Commit the hardware parameters back to the device.
    err = snd_pcm_hw_params(handle->handle, hardwareParams);
    if (err < 0) LOGE("Unable to set hardware parameters: %s", snd_strerror(err));

    done:
    snd_pcm_hw_params_free(hardwareParams);

    return err;
}

status_t setSoftwareParams(alsa_handle_t *handle)
{
    snd_pcm_sw_params_t * softwareParams;
    int err;

    snd_pcm_uframes_t bufferSize = 0;
    snd_pcm_uframes_t periodSize = 0;
    snd_pcm_uframes_t startThreshold, stopThreshold;

    if (snd_pcm_sw_params_malloc(&softwareParams) < 0) {
        LOG_ALWAYS_FATAL("Failed to allocate ALSA software parameters!");
        return NO_INIT;
    }

    // Get the current software parameters
    err = snd_pcm_sw_params_current(handle->handle, softwareParams);
    if (err < 0) {
        LOGE("Unable to get software parameters: %s", snd_strerror(err));
        goto done;
    }

    // Configure ALSA to start the transfer when the buffer is almost full.
    snd_pcm_get_params(handle->handle, &bufferSize, &periodSize);

    if (handle->devices & AudioSystem::DEVICE_OUT_ALL) {
        // For playback, configure ALSA to start the transfer when the
        // buffer is full.
        startThreshold = bufferSize - 1;
        stopThreshold = bufferSize;
    } else {
        // For recording, configure ALSA to start the transfer on the
        // first frame.
        startThreshold = 1;
        stopThreshold = bufferSize;
    }

    err = snd_pcm_sw_params_set_start_threshold(handle->handle, softwareParams,
            startThreshold);
    if (err < 0) {
        LOGE("Unable to set start threshold to %lu frames: %s",
                startThreshold, snd_strerror(err));
        goto done;
    }

    err = snd_pcm_sw_params_set_stop_threshold(handle->handle, softwareParams,
            stopThreshold);
    if (err < 0) {
        LOGE("Unable to set stop threshold to %lu frames: %s",
                stopThreshold, snd_strerror(err));
        goto done;
    }

    // Allow the transfer to start when at least periodSize samples can be
    // processed.
    err = snd_pcm_sw_params_set_avail_min(handle->handle, softwareParams,
            periodSize);
    if (err < 0) {
        LOGE("Unable to configure available minimum to %lu: %s",
                periodSize, snd_strerror(err));
        goto done;
    }

    // Commit the software parameters back to the device.
    err = snd_pcm_sw_params(handle->handle, softwareParams);
    if (err < 0) LOGE("Unable to configure software parameters: %s",
            snd_strerror(err));

    done:
    snd_pcm_sw_params_free(softwareParams);

    return err;
}

void setDefaultControls(uint32_t devices, int mode, const char *cardname)
{

	ALSAControl *ctl = new ALSAControl(cardname);
	LOGD ("setDefaultControls set card: %s",cardname);

	if(devices & IMX51_OUT_CODEC_DEFAULT)
	{
        if(selecteddevice == DEVICE_SGTL5000)
        {
            if (devices & AudioSystem::DEVICE_OUT_SPEAKER ||
                devices & AudioSystem::DEVICE_OUT_EARPIECE) {
                ctl->set("Speaker Function", "on"); // on
            } else {
                ctl->set("Speaker Function", "off"); // off
            }
        }    
    }

	if(devices & IMX51_OUT_CODEC_DEFAULT)
	{
        if(selecteddevice == DEVICE_WM8958)
        {
            ctl->set("AIF1DAC Mux", 0, 0); /* 0: AIF1DACDAT, 1: AIF3DACDAT */
            ctl->set("AIF2DAC Mux", 0, 0); /* 0: AIF2DACDAT, 1: AIF3DACDAT */
            if (mode == AudioSystem::MODE_IN_CALL)
            {
                ctl->set("DAC1L Mixer AIF1.1 Switch", 0, 0);
                ctl->set("DAC1L Mixer AIF1.2 Switch", 0, 0);
                ctl->set("DAC1L Mixer AIF2 Switch", 1, 0);
                ctl->set("DAC1L Mixer Left Sidetone Switch", 0, 0);
                ctl->set("DAC1L Mixer Right Sidetone Switch", 0, 0);
    
                ctl->set("DAC1R Mixer AIF1.1 Switch", 0, 0);
                ctl->set("DAC1R Mixer AIF1.2 Switch", 0, 0);
                ctl->set("DAC1R Mixer AIF2 Switch", 1, 0);
                ctl->set("DAC1R Mixer Left Sidetone Switch", 0, 0);
                ctl->set("DAC1R Mixer Right Sidetone Switch", 0, 0);
            }
            else
            {
                ctl->set("DAC1L Mixer AIF1.1 Switch", 1, 0);
                ctl->set("DAC1L Mixer AIF1.2 Switch", 0, 0);
                ctl->set("DAC1L Mixer AIF2 Switch", 0, 0);
                ctl->set("DAC1L Mixer Left Sidetone Switch", 0, 0);
                ctl->set("DAC1L Mixer Right Sidetone Switch", 0, 0);
    
                ctl->set("DAC1R Mixer AIF1.1 Switch", 1, 0);
                ctl->set("DAC1R Mixer AIF1.2 Switch", 0, 0);
                ctl->set("DAC1R Mixer AIF2 Switch", 0, 0);
                ctl->set("DAC1R Mixer Left Sidetone Switch", 0, 0);
                ctl->set("DAC1R Mixer Right Sidetone Switch", 0, 0);
            }

        
            if (devices & AudioSystem::DEVICE_OUT_SPEAKER )
            {
                ctl->set("Earpiece Switch", 0, 0); /*0 : mute  1: unmute */
                ctl->set("Headphone Switch", 0, -1); /*0 : mute  1: unmute */
                ctl->set("SPKL DAC1 Switch", 1, 0); 
                ctl->set("SPKL DAC1 Volume", 1, 0); 
                ctl->set("SPKR DAC1 Switch", 1, 0); 
                ctl->set("SPKR DAC1 Volume", 1, 0); 
                ctl->set("SPKL Boost SPKL Switch", 1, 0); 
                ctl->set("SPKR Boost SPKR Switch", 1, 0);             
                ctl->set("Speaker Mixer Volume", 3, -1); 
                ctl->set("DAC1 Switch", 1, -1);    
                ctl->set("Speaker Switch", 1, -1);   /*0 : mute  1: unmute */
                ctl->set("Speaker Volume", 60, -1); /* 0 ~ 63 */
            
            }else if(devices & AudioSystem::DEVICE_OUT_WIRED_HEADSET ||
                 devices & AudioSystem::DEVICE_OUT_WIRED_HEADPHONE ){
                ctl->set("Speaker Switch", 0, -1);   /*0 : mute  1: unmute */
                ctl->set("Earpiece Switch", 0, 0);   /*0 : mute  1: unmute */
                ctl->set("Left Headphone Mux", 1, 0); /* 0: Mixer, 1: DAC */
                ctl->set("Right Headphone Mux", 1, 0); /* 0: Mixer, 1: DAC */
                ctl->set("DAC1 Switch", 1, -1);
                ctl->set("Headphone Switch", 1, -1); /*0 : mute  1: unmute */
                ctl->set("Headphone Volume", 57, -1); /* 0 ~ 63 */
            }
        }
    }
    
    if(devices & IMX51_IN_CODEC_DEFAULT)
	{
        if(selecteddevice == DEVICE_WM8958)
        {
            ctl->set("AIF1DAC Mux", 0, 0); /* 0: AIF1DACDAT, 1: AIF3DACDAT */
            ctl->set("AIF2DAC Mux", 0, 0); /* 0: AIF2DACDAT, 1: AIF3DACDAT */
        
            if (devices & AudioSystem::DEVICE_IN_WIRED_HEADSET ){
                ctl->set("IN1R Switch", 0, 0); 
                ctl->set("IN1L Switch", 1, 0); 
                ctl->set("IN1L Volume", 27, 0); 
                ctl->set("MIXINL IN1L Switch", 1, 0); 
                ctl->set("MIXINL IN1L Volume", 1, 0); 
                ctl->set("IN1L PGA IN1LN Switch", 1, 0); 
                ctl->set("ADCL Mux", 0, 0);   /* 0: ADC  1: DMIC */ 
                if (mode == AudioSystem::MODE_IN_CALL)
                {
                    ctl->set("AIF2DAC2L Mixer Left Sidetone Switch", 1, 0);   
                    ctl->set("Left Sidetone Mux",0, 0);/*0: ADC/DMIC1, 1:DMIC2 */     
                }
                else      
                    ctl->set("AIF1ADC1L Mixer ADC/DMIC Switch", 1, 0);        
            
            }else if(devices & AudioSystem::DEVICE_IN_BUILTIN_MIC ){
                ctl->set("IN1L Switch", 0, 0); 
                ctl->set("IN1R Switch", 1, 0); 
                ctl->set("IN1R Volume", 27, 0); 
                ctl->set("MIXINR IN1R Switch", 1, 0); 
                ctl->set("MIXINR IN1R Volume", 1, 0); 
                ctl->set("IN1R PGA IN1RN Switch", 1, 0); 
                ctl->set("ADCR Mux", 0, 0);   /* 0: ADC  1: DMIC */      
                if (mode == AudioSystem::MODE_IN_CALL)
                {
                    ctl->set("AIF2DAC2R Mixer Right Sidetone Switch", 1, 0);   
                    ctl->set("Right Sidetone Mux", 0, 0);/*0: ADC/DMIC1, 1:DMIC2 */     
                }
                else    
                    ctl->set("AIF1ADC1R Mixer ADC/DMIC Switch", 1, 0);        
            } 
        }   
    }    


}

void setAlsaControls(alsa_handle_t *handle, uint32_t devices, int mode)
{
    AlsaControlSet set = (AlsaControlSet) handle->modPrivate;
    const char *card = deviceName(handle, devices, mode, 0);
    set(devices, mode, card);
}

// ----------------------------------------------------------------------------

static status_t s_init(alsa_device_t *module, ALSAHandleList &list)
{
    LOGD("Initializing devices for IMX51 ALSA module");

    list.clear();

    for (size_t i = 0; i < ARRAY_SIZE(_defaults); i++) {

        _defaults[i].module = module;
        list.push_back(_defaults[i]);
    }

    return NO_ERROR;
}

static status_t s_open(alsa_handle_t *handle, uint32_t devices, int mode)
{
    // Close off previously opened device.
    // It would be nice to determine if the underlying device actually
    // changes, but we might be recovering from an error or manipulating
    // mixer settings (see asound.conf).
    //
    s_close(handle);

    LOGD("open called for devices %08x in mode %d...", devices, mode);

    const char *stream = streamName(handle);
    const char *devName = deviceName(handle, devices, mode, 1);

    // The PCM stream is opened in blocking mode, per ALSA defaults.  The
    // AudioFlinger seems to assume blocking mode too, so asynchronous mode
    // should not be used.
    int err = snd_pcm_open(&handle->handle, devName, direction(handle), 0);

    if (err < 0) {
        LOGE("Failed to Initialize any ALSA %s device: %s", stream, strerror(err));
        return NO_INIT;
    }

    err = setHardwareParams(handle);

    if (err == NO_ERROR) err = setSoftwareParams(handle);

    setAlsaControls(handle, devices, mode);

    LOGI("Initialized ALSA %s device %s", stream, devName);

    handle->curDev = devices;
    handle->curMode = mode;

    return err;
}

static status_t s_close(alsa_handle_t *handle)
{
    LOGW("s_close--");
    status_t err = NO_ERROR;
    snd_pcm_t *h = handle->handle;
    handle->handle = 0;
    handle->curDev = 0;
    handle->curMode = 0;
    if (h) {
        snd_pcm_drain(h);
        err = snd_pcm_close(h);
    }

    return err;
}

static status_t s_route(alsa_handle_t *handle, uint32_t devices, int mode)
{
    status_t status = NO_ERROR;

    LOGD("route called for devices %08x in mode %d...", devices, mode);

    if (handle->handle && handle->curDev == devices && handle->curMode == mode)
        ; // Nothing to do
    else if (handle->handle && (handle->devices & devices)){
        setAlsaControls(handle, devices, mode);
        handle->curDev  = devices;
        handle->curMode = mode;
    }
    else {
        LOGE("Why are we routing to a device that isn't supported by this object?!?!?!?!");
        status = s_open(handle, devices, mode);
    }
    return status;
}

}
