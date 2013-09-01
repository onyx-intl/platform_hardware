/*
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

/*Copyright 2009-2011 Freescale Semiconductor, Inc. All Rights Reserved.*/

#ifndef _BLIT_IPU_H_
#define _BLIT_IPU_H_

#include <hardware/hardware.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <hardware/hwcomposer.h>

#include <EGL/egl.h>
#include "gralloc_priv.h"
#include "hwc_common.h"
#include <linux/ipu.h>
//extern "C" {
//#include "mxc_ipu_hl_lib.h"
//}
/*****************************************************************************/

#define BLIT_PIXEL_FORMAT_RGB_565  209

class blit_ipu : public blit_device
{
public:
    virtual int blit(hwc_layer_t *layer, hwc_buffer *out_buf);

	blit_ipu();
	virtual ~blit_ipu();

private:
    struct ipu_task mTask;
    int mIpuFd;
	//ipu_lib_input_param_t  mIPUInputParam;
    //ipu_lib_output_param_t mIPUOutputParam;
    //ipu_lib_handle_t       mIPUHandle;
//    int                    mIPURet;
private:
	int init();
    int uninit();

	blit_ipu& operator = (blit_ipu& out);
	blit_ipu(const blit_ipu& out);
};


//int ipu_init(struct blit_device *dev);
//
//int ipu_uninit(struct blit_device*dev);
//
//int ipu_blit(struct blit_device *dev, hwc_layer_t *layer, hwc_buffer *out_buf);

#endif // _BLIT_IPU_H_
