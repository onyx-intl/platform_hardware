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

#ifndef CAPTURE_DEVICE_INTERFACE_H
#define CAPTURE_DEVICE_INTERFACE_H

#include <utils/RefBase.h>
#include "Camera_utils.h"


namespace android {
#define UVC_NAME_STRING "uvc"

    typedef enum{
        CAPTURE_DEVICE_ERR_ALRADY_OPENED  = 3,
        CAPTURE_DEVICE_ERR_ENUM_CONTINUE  = 2,
        CAPTURE_DEVICE_ERR_ENUM_PARAM_END = 1,
        CAPTURE_DEVICE_ERR_NONE = 0,
        CAPTURE_DEVICE_ERR_OPEN = -1,
        CAPTURE_DEVICE_ERR_GET_PARAM = -2,
        CAPTURE_DEVICE_ERR_SET_PARAM = -3,
        CAPTURE_DEVICE_ERR_ALLOCATE_BUF = -4,
        CAPTURE_DEVICE_ERR_BAD_PARAM  = -5,
        CAPTURE_DEVICE_ERR_SYS_CALL=-6,
        CAPTURE_DEVICE_ERR_UNKNOWN = -100
    }CAPTURE_DEVICE_ERR_RET;

    typedef enum{
        MOTION_MODE = 0,
        HIGH_QUALITY_MODE = 1
    }CAPTURE_MODE;

    typedef enum{
        OUTPU_FMT = 0,
        FRAME_SIZE_FPS = 1
    }DevParamType;

	typedef enum{
        SENSOR_PREVIEW_BACK_REF = 0,
        SENSOR_PREVIEW_VERT_FLIP = 1,
        SENSOR_PREVIEW_HORIZ_FLIP = 2,
        SENSOR_PREVIEW_ROATE_180 = 3,
        SENSOR_PREVIEW_ROATE_LAST = 3
	}SENSOR_PREVIEW_ROTATE;

    struct timeval_fract{
        unsigned int numerator;
        unsigned int denominator;
    };

    struct capture_config_t{
        unsigned int fmt;
        unsigned int width;
        unsigned int height;
        unsigned int framesize;   //out
        unsigned int picture_waite_number;//out
        struct timeval_fract tv;
		SENSOR_PREVIEW_ROTATE rotate;
    };


    class CaptureDeviceInterface : public virtual RefBase{
    public:

        virtual CAPTURE_DEVICE_ERR_RET SetDevName(char * deviceName)=0;
        virtual CAPTURE_DEVICE_ERR_RET GetDevName(char * deviceName)=0;
        virtual CAPTURE_DEVICE_ERR_RET DevOpen()=0;
        virtual CAPTURE_DEVICE_ERR_RET EnumDevParam(DevParamType devParamType, void *retParam)=0;
        virtual CAPTURE_DEVICE_ERR_RET DevSetConfig(struct capture_config_t *pCapcfg)=0;
        virtual CAPTURE_DEVICE_ERR_RET DevAllocateBuf(DMA_BUFFER *DevBufQue, unsigned int *pBufQueNum)=0;
        virtual CAPTURE_DEVICE_ERR_RET DevPrepare()=0;
        virtual CAPTURE_DEVICE_ERR_RET DevStart()=0;
        virtual CAPTURE_DEVICE_ERR_RET DevDequeue(unsigned int *pBufQueIdx)=0;
        virtual CAPTURE_DEVICE_ERR_RET DevQueue(unsigned int BufQueIdx)=0;
        virtual CAPTURE_DEVICE_ERR_RET DevStop()=0;
        virtual CAPTURE_DEVICE_ERR_RET DevDeAllocate()=0;
        virtual CAPTURE_DEVICE_ERR_RET DevClose()=0;

        virtual ~ CaptureDeviceInterface(){}
    };
    extern "C" sp<CaptureDeviceInterface> createCaptureDevice(char *deviceName);

};
#endif

