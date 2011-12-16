/*
 * Copyright (C) 2008 The Android Open Source Project
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

/*
 * Copyright 2009-2011 Freescale Semiconductor, Inc. 
 */
#include "JpegEncoderSoftware.h"
namespace android{

    extern "C" sp<JpegEncoderInterface> createJpegEncoder(JPEG_ENCODER_TYPE jpeg_enc_type)
    {
        if (jpeg_enc_type == SOFTWARE_JPEG_ENC){
            CAMERA_HAL_LOG_INFO("Create the software encoder");
            return JpegEncoderSoftware::createInstance();
        }
        else{
            CAMERA_HAL_ERR("the hardware encoder is not supported");
            return NULL;
        }
    }
};
