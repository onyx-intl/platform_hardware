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

#include <hardware/hardware.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <hardware/hwcomposer.h>

#include <EGL/egl.h>
#include "gralloc_priv.h"
#include "hwc_common.h"
#include "blit_gpu.h"
/*****************************************************************************/
using namespace android;

blit_gpu::blit_gpu()
{
		init();
}

blit_gpu::~blit_gpu()
{
		uninit();
}

int blit_gpu::init()
{
		return 0;
}

int blit_gpu::uninit()
{
		return 0;
}

int blit_gpu::blit(hwc_layer_t *layer, hwc_buffer *out_buf)
{
		return 0;
}
