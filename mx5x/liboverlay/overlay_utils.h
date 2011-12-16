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

#ifndef __OVERLAY_UTILS_H__
#define __OVERLAY_UTILS_H__

#define LOG_TAG "FslOverlay"

#define MAX_OVERLAY_INSTANCES 2
#define DEFAULT_OVERLAY_BUFFER_NUM 0
#define MAX_OVERLAY_BUFFER_NUM 24

#define DEFAULT_FB_DEV_NAME "/dev/graphics/fb0"
#define FB1_DEV_NAME "/dev/graphics/fb1"
#define V4L_DEV_NAME "/dev/video16"
#define DEFAULT_V4L_LAYER  3
#define TV_V4L_LAYER 5
#define DEFAULT_ALPHA_BUFFERS 2
#define DEFAULT_V4L_BUFFERS 3
#define MAX_V4L_BUFFERS 3

#define MAX_OVERLAY_INPUT_W 1280
#define MAX_OVERLAY_INPUT_H 720

#define OVERLAY_QUEUE_THRESHOLD 2

#define FULL_TRANSPARANT_VALUE 255

#define SHARED_CONTROL_MARKER             (0x4F564354) //OVCT
#define SHARED_DATA_MARKER             (0x4F564441) //OVDA

 
#define OVERLAY_LOG_INFO(format, ...) LOGI((format), ## __VA_ARGS__)
#define OVERLAY_LOG_ERR(format, ...) LOGE((format), ## __VA_ARGS__)
#define OVERLAY_LOG_WARN(format, ...) LOGW((format), ## __VA_ARGS__)

#ifdef OVERLAY_DEBUG_LOG
#define OVERLAY_LOG_RUNTIME(format, ...) LOGI((format), ## __VA_ARGS__)
#define OVERLAY_LOG_FUNC LOGI("%s: %s",  __FILE__, __FUNCTION__)
#else
#define OVERLAY_LOG_RUNTIME(format, ...) 
#define OVERLAY_LOG_FUNC
#endif

struct OVERLAY_BUFFER{
    void *vir_addr;
    unsigned int phy_addr;
    unsigned int size;
};

struct WIN_REGION {
    int left;
    int right;
    int top;
    int bottom;
};

typedef struct _RECTTYPE {
    int nLeft; 
    int nTop;
    int nWidth;
    int nHeight;
} RECTTYPE;

class OverlayAllocator{
public:
    virtual ~OverlayAllocator(){}
    virtual int allocate(OVERLAY_BUFFER *overlay_buf, int size){ return -1;}
    virtual int deAllocate(OVERLAY_BUFFER *overlay_buf){  return -1;  }
    virtual int getHeapID(){  return 0;  }
};


#endif
