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

/**
 *  Copyright (C) 2011 Freescale Semiconductor,Inc.,
 */
 
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/videodev.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <linux/mxcfb.h>

extern "C" {
#include "mxc_ipu_hl_lib.h" 
} 


#include <hardware/hardware.h>
#include <hardware/overlay.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <pthread.h>
#include <semaphore.h>

#include <utils/List.h>
#include <ui/PixelFormat.h>
#include <linux/android_pmem.h>
#include "overlay_pmem.h"

/*
*   input parameter: 
*/
PmemAllocator::PmemAllocator(int bufCount, int bufSize):
    mFD(0),mTotalSize(0),mBufCount(bufCount),mBufSize(bufSize),
    mVirBase(NULL),mPhyBase(NULL)
{
    OVERLAY_LOG_FUNC;
    memset(mSlotAllocated, 0, sizeof(bool)*MAX_SLOT);

    int err;
    struct pmem_region region;
    mFD = open(PMEM_DEV, O_RDWR);
    if (mFD < 0) {
         OVERLAY_LOG_ERR("Error!PmemAllocator constructor");
         return;
    }

    err = ioctl(mFD, PMEM_GET_TOTAL_SIZE, &region);
    if (err == 0)
    {
         OVERLAY_LOG_INFO("Info!get pmem total size %d",(int)region.len);
    }
    else
    {
        OVERLAY_LOG_ERR("Error!Cannot get total length in PmemAllocator constructor");
        return;
    }
    
    mBufSize = (bufSize + DEFAULT_PMEM_ALIGN-1) & ~(DEFAULT_PMEM_ALIGN-1);
    
    mTotalSize = mBufSize*bufCount;
    if((mTotalSize > region.len)||(mBufCount > MAX_SLOT)) {
        OVERLAY_LOG_ERR("Error!Out of PmemAllocator capability");
    }
    else if(mTotalSize > 0)
    {
        uint8_t *virtualbase = (uint8_t*)mmap(0, mTotalSize,
            PROT_READ|PROT_WRITE, MAP_SHARED, mFD, 0);

        if (virtualbase == MAP_FAILED) {
           OVERLAY_LOG_INFO("mmap(fd=%d, size=%u) failed (%s)",
                   mFD, (unsigned int)mTotalSize, strerror(errno));
           return;
        }

        memset(&region, 0, sizeof(region));
    
        if (ioctl(mFD, PMEM_GET_PHYS, &region) == -1)
        {
          OVERLAY_LOG_ERR("Error!Failed to get physical address of source!\n");
          munmap(virtualbase, mTotalSize);
          return;
        }
        mVirBase = (void *)virtualbase;
        mPhyBase = region.offset;
        OVERLAY_LOG_INFO("Allocator total size %d, vir addr 0x%x, phy addr 0x%x",mTotalSize,mVirBase,mPhyBase);
    }
    else{
        mVirBase = NULL;
        mPhyBase = 0;
    }
 
}

PmemAllocator::~PmemAllocator()
{
    OVERLAY_LOG_FUNC;

    for(int index=0;index < MAX_SLOT;index ++) {
        if(mSlotAllocated[index]) {
            OVERLAY_LOG_ERR("Error!Cannot deinit PmemAllocator before all memory back to allocator");
        }
    }

    if(mVirBase) {
        munmap(mVirBase, mTotalSize);
    }
    if(mFD) {
        close(mFD);
    }
     
}

int PmemAllocator::allocate(struct OVERLAY_BUFFER *overlay_buf, int size)
{
    OVERLAY_LOG_FUNC;
    if((!mVirBase)||(!overlay_buf)||(size>mBufSize)) {
        OVERLAY_LOG_ERR("Error!No memory for allocator");
        return -1;
    }

    for(int index=0;index < MAX_SLOT;index ++) {
        if(!mSlotAllocated[index]) {
            OVERLAY_LOG_RUNTIME("Free slot %d for allocating mBufSize %d request size %d",
                             index,mBufSize,size);

            overlay_buf->vir_addr = (void *)((char *)mVirBase+index*mBufSize); 
            overlay_buf->phy_addr = mPhyBase+index*mBufSize;
            overlay_buf->size = mBufSize;
            mSlotAllocated[index] = true;
            return 0;
        }
    }
    return -1;
}

int PmemAllocator::deAllocate(OVERLAY_BUFFER *overlay_buf)
{
    OVERLAY_LOG_FUNC;
    if((!mVirBase)||(!overlay_buf)) {
        OVERLAY_LOG_ERR("Error!No memory for allocator");
        return -1;
    }
    int nSlot = ((unsigned int)overlay_buf->vir_addr - (unsigned int)mVirBase)/mBufSize;
    if((nSlot<MAX_SLOT)&&(mSlotAllocated[nSlot])) {
        OVERLAY_LOG_RUNTIME("Info!deAllocate for slot %d",nSlot);
        mSlotAllocated[nSlot] = false;
        return 0;
    }
    else{
        OVERLAY_LOG_ERR("Error!Not a valid buffer");
        return -1;
    }
}
