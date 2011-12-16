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

#ifndef PP_IPU_LIB_H
#define PP_IPU_LIB_H
extern "C" {
#include "mxc_ipu_hl_lib.h" 
} 

#include "PostProcessDeviceInterface.h"

namespace android{
    class PPIpuLib : public PostProcessDeviceInterface
    {
    public:
        virtual PPDEVICE_ERR_RET PPDeviceInit(pp_input_param_t *pp_input, pp_output_param_t *pp_output);
        virtual PPDEVICE_ERR_RET DoPorcess(DMA_BUFFER *pp_input_addr, DMA_BUFFER *pp_output_addr);
        virtual PPDEVICE_ERR_RET PPDeviceDeInit();
        static sp<PostProcessDeviceInterface> createInstance();
    private:
        PPIpuLib();
        virtual ~PPIpuLib();
        static wp<PostProcessDeviceInterface> singleton;

        ipu_lib_input_param_t mIPUInputParam;	
        ipu_lib_output_param_t mIPUOutputParam; 
        ipu_lib_handle_t			mIPUHandle;
    };
};
#endif
