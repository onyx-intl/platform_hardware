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

#ifndef __CAMERA_MEM__H__
#define __CAMERA_MEM__H__

#include "Camera_utils.h"
#include <utils/RefBase.h>


#define DEFAULT_PMEM_ALIGN (4096)
#define PMEM_DEV "/dev/pmem_adsp"
#define MAX_SLOT 64

namespace android {

class PmemAllocator : public virtual RefBase
{
public:
    PmemAllocator(int bufCount,int bufSize);
    virtual ~PmemAllocator();
    virtual int allocate(DMA_BUFFER *p_buf, int size);
    virtual int deAllocate(DMA_BUFFER *p_buf);
	int err_ret;
private:
    int mFD;
    unsigned long mTotalSize;
    int mBufCount;
    int mBufSize;
    void *mVirBase;
    unsigned int mPhyBase;
    bool mSlotAllocated[MAX_SLOT];
	
};
};

#endif
