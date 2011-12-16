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
 * Copyright 2009-2011 Freescale Semiconductor, Inc. All Rights Reserved.
 */

#ifndef POSTPROCESS_DEVICE_INTERFACE_H
#define POSTPROCESS_DEVICE_INTERFACE_H

#include <utils/RefBase.h>
#include "Camera_utils.h"
#include <linux/videodev2.h>

namespace android {

    typedef enum{
        PPDEVICE_ERROR_NONE = 0,
        PPDEVICE_ERROR_INIT = -1,
        PPDEVICE_ERROR_PROCESS  = -2,
        PPDEVICE_ERROR_DEINIT = -3
    }PPDEVICE_ERR_RET;

    struct pp_fb_pos{
        unsigned short x;
        unsigned short y;
    };

    struct win_t{
        struct pp_fb_pos pos;
        unsigned int win_w;
        unsigned int win_h;
    } ;

    typedef struct {
        unsigned int width;
        unsigned int height;
        unsigned int fmt;
        struct win_t input_crop_win;
        int user_def_paddr;
    } pp_input_param_t;

    typedef struct {
        unsigned int width;
        unsigned int height;
        unsigned int fmt;
        unsigned int rot;
        struct win_t output_win;
        int user_def_paddr;
    } pp_output_param_t;


    class PostProcessDeviceInterface : public virtual RefBase{
    public:
        virtual  PPDEVICE_ERR_RET PPDeviceInit(pp_input_param_t *pp_input, pp_output_param_t *pp_output)=0;
        virtual  PPDEVICE_ERR_RET DoPorcess(DMA_BUFFER *pp_input_addr, DMA_BUFFER *pp_output_addr)=0;
        virtual  PPDEVICE_ERR_RET PPDeviceDeInit()=0;

        virtual ~PostProcessDeviceInterface(){}
    }; 
    extern "C" sp<PostProcessDeviceInterface> createPPDevice();

};
#endif

