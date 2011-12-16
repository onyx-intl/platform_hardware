/*
 * Copyright (C) 2009 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "AudioPolicyManagerALSA"
//#define LOG_NDEBUG 0
#include <utils/Log.h>
#include "AudioPolicyManagerALSA.h"
#include <media/mediarecorder.h>

namespace android {

status_t AudioPolicyManagerALSA::stopInput(audio_io_handle_t input)
{
    LOGV("stopInput() input %d", input);
    ssize_t index = mInputs.indexOfKey(input);
    if (index < 0) {
        LOGW("stopInput() unknow input %d", input);
        return BAD_VALUE;
    }
    AudioInputDescriptor *inputDesc = mInputs.valueAt(index);

    if (inputDesc->mRefCount == 0) {
        LOGW("stopInput() input %d already stopped", input);
        return INVALID_OPERATION;
    } else {
        AudioParameter param = AudioParameter();
      //  param.addInt(String8(AudioParameter::keyRouting), 0);
         param.addInt(String8(AudioParameter::keyRouting), inputDesc->mDevice); //set the device name
        mpClientInterface->setParameters(input, param.toString());
        inputDesc->mRefCount = 0;
        return NO_ERROR;
    }
}
// ----------------------------------------------------------------------------
// AudioPolicyManagerALSA
// ----------------------------------------------------------------------------

// ---  class factory

extern "C" AudioPolicyInterface* createAudioPolicyManager(AudioPolicyClientInterface *clientInterface)
{
    return new AudioPolicyManagerALSA(clientInterface);
}

extern "C" void destroyAudioPolicyManager(AudioPolicyInterface *interface)
{
    delete interface;
}

// Nothing currently different between the Base implementation.

AudioPolicyManagerALSA::AudioPolicyManagerALSA(AudioPolicyClientInterface *clientInterface)
    : AudioPolicyManagerBase(clientInterface)
{
}

AudioPolicyManagerALSA::~AudioPolicyManagerALSA()
{
}

}; // namespace android
