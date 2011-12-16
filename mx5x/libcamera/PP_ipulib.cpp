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
#include "PP_ipulib.h"
#include <stdlib.h>
#include <string.h>

namespace android{

    wp<PostProcessDeviceInterface> PPIpuLib :: singleton;

    PPIpuLib :: PPIpuLib(){
        return;
    }

    PPIpuLib :: ~PPIpuLib(){

        singleton.clear();
    }
    PPDEVICE_ERR_RET PPIpuLib :: PPDeviceInit(pp_input_param_t *pp_input, pp_output_param_t *pp_output){
        CAMERA_HAL_LOG_FUNC;
        PPDEVICE_ERR_RET ret = PPDEVICE_ERROR_NONE;

        int mIPURet;

        memset(&mIPUHandle, 0, sizeof(ipu_lib_handle_t));
        //Setting input format
        mIPUInputParam.width = pp_input->width;
        mIPUInputParam.height = pp_input->height;

        mIPUInputParam.input_crop_win.pos.x = pp_input->input_crop_win.pos.x;
        mIPUInputParam.input_crop_win.pos.y = pp_input->input_crop_win.pos.y;  
        mIPUInputParam.input_crop_win.win_w = pp_input->input_crop_win.win_w;
        mIPUInputParam.input_crop_win.win_h = pp_input->input_crop_win.win_h;
        mIPUInputParam.fmt = pp_input->fmt;
        mIPUInputParam.user_def_paddr[0] = pp_input->user_def_paddr;

        //Setting output format
        mIPUOutputParam.fmt = pp_output->fmt;
        mIPUOutputParam.width = pp_output->width;
        mIPUOutputParam.height = pp_output->height;   
        mIPUOutputParam.show_to_fb = 0;
        //Output param should be same as input, since no resize,crop
        mIPUOutputParam.output_win.pos.x = pp_output->output_win.pos.x;
        mIPUOutputParam.output_win.pos.y = pp_output->output_win.pos.y;
        mIPUOutputParam.output_win.win_w = pp_output->output_win.win_w;
        mIPUOutputParam.output_win.win_h = pp_output->output_win.win_h;
        mIPUOutputParam.rot = pp_output->rot;
        mIPUOutputParam.user_def_paddr[0] = pp_output->user_def_paddr;
        CAMERA_HAL_LOG_RUNTIME(" Output param: width %d,height %d, pos.x %d, pos.y %d,win_w %d,win_h %d,rot %d",
                mIPUOutputParam.width,
                mIPUOutputParam.height,
                mIPUOutputParam.output_win.pos.x,
                mIPUOutputParam.output_win.pos.y,
                mIPUOutputParam.output_win.win_w,
                mIPUOutputParam.output_win.win_h,
                mIPUOutputParam.rot);

        CAMERA_HAL_LOG_RUNTIME("Input param: width %d, height %d, fmt %d, crop_win pos x %d, crop_win pos y %d, crop_win win_w %d,crop_win win_h %d",
                mIPUInputParam.width,
                mIPUInputParam.height,
                mIPUInputParam.fmt,
                mIPUInputParam.input_crop_win.pos.x,
                mIPUInputParam.input_crop_win.pos.y,
                mIPUInputParam.input_crop_win.win_w,
                mIPUInputParam.input_crop_win.win_h);	  

        mIPURet =  mxc_ipu_lib_task_init(&mIPUInputParam,NULL,&mIPUOutputParam,OP_NORMAL_MODE|TASK_ENC_MODE,&mIPUHandle);
        if (mIPURet < 0) {
            CAMERA_HAL_ERR("Error! convertYUYVtoNV12, mxc_ipu_lib_task_init ret %d!",mIPURet);
            return PPDEVICE_ERROR_INIT;
        }  

        return ret;
    }

    PPDEVICE_ERR_RET PPIpuLib :: DoPorcess(DMA_BUFFER *pp_input_addr, DMA_BUFFER *pp_output_addr){
        CAMERA_HAL_LOG_FUNC;
        PPDEVICE_ERR_RET ret = PPDEVICE_ERROR_NONE;

        int mIPURet;
        mIPUInputParam.user_def_paddr[0] = pp_input_addr->phy_offset;

        mIPUOutputParam.user_def_paddr[0] = pp_output_addr->phy_offset;

        mIPURet = mxc_ipu_lib_task_buf_update(&mIPUHandle,pp_input_addr->phy_offset,pp_output_addr->phy_offset,NULL,NULL,NULL);
        if (mIPURet < 0) {
            CAMERA_HAL_ERR("Error! convertYUYVtoNV12, mxc_ipu_lib_task_buf_update ret %d!",mIPURet);
            mxc_ipu_lib_task_uninit(&mIPUHandle);
            memset(&mIPUHandle, 0, sizeof(ipu_lib_handle_t));
            return PPDEVICE_ERROR_PROCESS;
        }

        return ret;

    }

    PPDEVICE_ERR_RET PPIpuLib :: PPDeviceDeInit(){
        CAMERA_HAL_LOG_FUNC;
        PPDEVICE_ERR_RET ret = PPDEVICE_ERROR_NONE;

        mxc_ipu_lib_task_uninit(&mIPUHandle);
        memset(&mIPUHandle, 0, sizeof(ipu_lib_handle_t));

        return ret;
    }

    sp<PostProcessDeviceInterface> PPIpuLib :: createInstance(){
        CAMERA_HAL_LOG_FUNC;
        if (singleton != 0) {
            sp<PostProcessDeviceInterface> device = singleton.promote();
            if (device != 0) {
                return device;
            }
        }
        sp<PostProcessDeviceInterface> device(new PPIpuLib());

        singleton = device;
        return device;
    }

};
