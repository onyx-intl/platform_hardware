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
#include "V4l2UVCDevice.h"
#include "V4l2CsiDevice.h"
namespace android{
    extern "C" sp<CaptureDeviceInterface> createCaptureDevice(char *deviceName)
    {
        if(strstr(deviceName, UVC_NAME_STRING)){
            CAMERA_HAL_LOG_INFO("It is the UVC device");

            sp<CaptureDeviceInterface>  device(new V4l2UVCDevice());
            device->SetDevName(deviceName);
            return device;
        }else{
            CAMERA_HAL_LOG_INFO("It is the CSI device");
            sp<CaptureDeviceInterface>  device(new V4l2CsiDevice());
            device->SetDevName(deviceName);
            return device;
        }
    }


}
