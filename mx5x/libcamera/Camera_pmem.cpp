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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <linux/time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <linux/android_pmem.h>
#include "Camera_pmem.h"


/*
 *   input parameter: 
 */

using namespace android;

PmemAllocator::PmemAllocator(int bufCount, int bufSize):
    err_ret(0), mFD(0),mTotalSize(0),mBufCount(bufCount),mBufSize(bufSize),
    mVirBase(NULL),mPhyBase(NULL)
{

    memset(mSlotAllocated, 0, sizeof(bool)*MAX_SLOT);

    int err;
    struct pmem_region region;
    mFD = open(PMEM_DEV, O_RDWR);
    if (mFD < 0) {
        CAMERA_HAL_ERR("Error!PmemAllocator constructor");
        err_ret = -1;
        return;
    }

    err = ioctl(mFD, PMEM_GET_TOTAL_SIZE, &region);
    if (err == 0)
    {
        CAMERA_HAL_ERR("Info!get pmem total size %d",(int)region.len);
    }
    else
    {
        CAMERA_HAL_ERR("Error!Cannot get total length in PmemAllocator constructor");
        err_ret = -1;
        return;
    }

    mBufSize = (bufSize + DEFAULT_PMEM_ALIGN-1) & ~(DEFAULT_PMEM_ALIGN-1);

    mTotalSize = mBufSize*bufCount;
    if((mTotalSize > region.len)||(mBufCount > MAX_SLOT)) {
        CAMERA_HAL_ERR("Error!Out of PmemAllocator capability");
    }
    else
    {
        uint8_t *virtualbase = (uint8_t*)mmap(0, mTotalSize,
                PROT_READ|PROT_WRITE, MAP_SHARED, mFD, 0);

        if (virtualbase == MAP_FAILED) {
            CAMERA_HAL_ERR("Error!mmap(fd=%d, size=%u) failed (%s)",
                    mFD, (unsigned int)mTotalSize, strerror(errno));
            return;
        }

        memset(&region, 0, sizeof(region));

        if (ioctl(mFD, PMEM_GET_PHYS, &region) == -1)
        {
            CAMERA_HAL_ERR("Error!Failed to get physical address of source!\n");
            munmap(virtualbase, mTotalSize);
            return;
        }
        mVirBase = (void *)virtualbase;
        mPhyBase = region.offset;
        CAMERA_HAL_LOG_RUNTIME("Allocator total size %d, vir addr 0x%x, phy addr 0x%x",mTotalSize,mVirBase,mPhyBase);
    }
}

PmemAllocator::~PmemAllocator()
{
    CAMERA_HAL_LOG_FUNC;

    for(int index=0;index < MAX_SLOT;index ++) {
        if(mSlotAllocated[index]) {
            CAMERA_HAL_ERR("Error!Cannot deinit PmemAllocator before all memory back to allocator");
        }
    }

    if(mVirBase) {
        munmap(mVirBase, mTotalSize);
    }
    if(mFD) {
        close(mFD);
    }

}

int PmemAllocator::allocate(DMA_BUFFER *pbuf, int size)
{
    CAMERA_HAL_LOG_FUNC;

    if((!mVirBase)||(!pbuf)||(size>mBufSize)) {
        CAMERA_HAL_ERR("Error!No memory for allocator");
        return DMA_ALLOCATE_ERR_BAD_PARAM;
    }

    for(int index=0;index < MAX_SLOT;index ++) {
        if(!mSlotAllocated[index]) {
            CAMERA_HAL_ERR("Free slot %d for allocating mBufSize %d request size %d",
                    index,mBufSize,size);

            pbuf->virt_start= (unsigned char *)mVirBase+index*mBufSize; 
            pbuf->phy_offset= mPhyBase+index*mBufSize;
            pbuf->length= mBufSize;
            mSlotAllocated[index] = true;
            return DMA_ALLOCATE_ERR_NONE;
        }
    }
    return DMA_ALLOCATE_ERR_BAD_PARAM;
}

int PmemAllocator::deAllocate(DMA_BUFFER *pbuf)
{
    CAMERA_HAL_LOG_FUNC;
    if((!mVirBase)||(!pbuf)) {
        CAMERA_HAL_ERR("Error!No memory for allocator");
        return DMA_ALLOCATE_ERR_BAD_PARAM;
    }
    int nSlot = ((unsigned int)pbuf->virt_start- (unsigned int)mVirBase)/mBufSize;
    if((nSlot<MAX_SLOT)&&(mSlotAllocated[nSlot])) {
        CAMERA_HAL_ERR("Info!deAllocate for slot %d",nSlot);
        mSlotAllocated[nSlot] = false;
        return DMA_ALLOCATE_ERR_NONE;
    }
    else{
        CAMERA_HAL_ERR("Error!Not a valid buffer");
        return DMA_ALLOCATE_ERR_BAD_PARAM;
    }
}
