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

#ifndef _BLIT_GPU_H_
#define _BLIT_GPU_H_

#include <hardware/hardware.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <hardware/hwcomposer.h>

#include <EGL/egl.h>
#include "gralloc_priv.h"
#include "hwc_common.h"
/*****************************************************************************/

class blit_gpu : public blit_device{
public:  
    virtual int blit(hwc_layer_t *layer, hwc_buffer *out_buf);

		blit_gpu();
		virtual ~blit_gpu();
    
private:
		int init();
    int uninit();
	
		blit_gpu& operator = (blit_gpu& out);
		blit_gpu(const blit_gpu& out);  
    //add private members.		    
};


//int gpu_init(struct blit_device *dev);
//
//int gpu_uninit(struct blit_device*dev);
//
//int gpu_blit(struct blit_device *dev, hwc_layer_t *layer, hwc_buffer *out_buf);

#endif
