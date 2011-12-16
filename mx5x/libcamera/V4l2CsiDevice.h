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
#ifndef V4L2_CSI_DEVICE_H
#define V4L2_CSI_DEVICE_H

#include <linux/videodev2.h>
#include "V4l2CapDeviceBase.h"
#define ENUM_SUPPORTED_FMT     3

#define OV3640_NAME_STR   "ov3640"
#define OV5640_NAME_STR   "ov5640"
#define OV5642_NAME_STR   "ov5642"
namespace android{

class V4l2CsiDevice : public V4l2CapDeviceBase{
    public:
        
        V4l2CsiDevice();
        virtual ~V4l2CsiDevice();
    protected:
		
		CAPTURE_DEVICE_ERR_RET V4l2Open();
		CAPTURE_DEVICE_ERR_RET V4l2EnumFmt(void *retParam);
		CAPTURE_DEVICE_ERR_RET V4l2EnumSizeFps(void *retParam);
		CAPTURE_DEVICE_ERR_RET V4l2SetConfig(struct capture_config_t *pCapcfg);
        CAPTURE_DEVICE_ERR_RET V4l2ConfigInput(struct capture_config_t *pCapcfg);
        CAPTURE_DEVICE_ERR_RET V4l2GetCaptureMode(struct capture_config_t *pCapcfg, unsigned int *pMode); 
        CAPTURE_DEVICE_ERR_RET V4l2SetRot(struct capture_config_t *pCapcfg);

		unsigned int mSupportedFmt[ENUM_SUPPORTED_FMT];
   };

};
#endif


