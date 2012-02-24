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

/* Copyright(C) 2010-2011 Freescale Semiconductor,Inc. */

#include <sys/mman.h>

#include <dlfcn.h>

#include <cutils/ashmem.h>
#include <cutils/log.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <string.h>
#include <stdlib.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>

#if HAVE_ANDROID_OS
#include <linux/fb.h>
#include <linux/mxcfb.h>
#include <linux/videodev.h>
#include <sys/mman.h>

extern "C"
{
#include "mxc_ipu_hl_lib.h"
}

#endif
#include <GLES/gl.h>
#include <c2d_api.h>
#include <pthread.h>
#include <semaphore.h>

#include "gralloc_priv.h"
#include "gr.h"
#define  MAX_RECT_NUM   20


// freescale change log
// 1. add fb_setUpdateRect to fb_post
// 2. add update_to_display to fb_post
// 3. init eink framebuffer

// Onyx change log
// 1. clean code, format the code according to onyx code style
// 2. remove second display support

/*****************************************************************************/

// numbers of buffers for page flipping
#define NUM_BUFFERS 3

enum
{
    PAGE_FLIP = 0x00000001,
    LOCKED = 0x00000002
};

enum
{
    SIN_VIDEO_DUAL_UI,
    DUAL_VIDEO_SIN_UI
};

struct fb_context_t
{
    framebuffer_device_t  device;
#ifdef FSL_EPDC_FB
    //Partial udate feature
    bool rect_update;
    int count;      //count need less than MAX_RECT_NUM ;
    int updatemode[20];
    int partial_left[20];
    int partial_top[20];
    int partial_width[20];
    int partial_height[20];
#endif
};

static int nr_framebuffers;
static int no_ipu = 0;

#ifdef FSL_EPDC_FB
#define WAVEFORM_MODE_INIT                      0x0   // Screen goes to white (clears)
#define WAVEFORM_MODE_DU                        0x1   // Grey->white/grey->black
#define WAVEFORM_MODE_GC16                      0x2   // High fidelity (flashing)
#define WAVEFORM_MODE_GC4                       0x3   //
//#define WAVEFORM_MODE_AUTO                    257  // defined in mxcfb.h

#define EINK_WAVEFORM_MODE_INIT      0x00000000
#define EINK_WAVEFORM_MODE_DU        0x00000001
#define EINK_WAVEFORM_MODE_GC16      0x00000002
#define EINK_WAVEFORM_MODE_GC4       0x00000003
#define EINK_WAVEFORM_MODE_AUTO      0x00000004
#define EINK_WAVEFORM_MODE_MASK      0x0000000F
#define EINK_AUTO_MODE_REGIONAL      0x00000000
#define EINK_AUTO_MODE_AUTOMATIC     0x00000010
#define EINK_AUTO_MODE_MASK          0x00000010
#define EINK_UPDATE_MODE_PARTIAL     0x00000000
#define EINK_UPDATE_MODE_FULL        0x00000020
#define EINK_UPDATE_MODE_MASK        0x00000020
#define EINK_WAIT_MODE_NOWAIT        0x00000000
#define EINK_WAIT_MODE_WAIT          0x00000040
#define EINK_WAIT_MODE_MASK          0x00000040
#define EINK_COMBINE_MODE_NOCOMBINE  0x00000000
#define EINK_COMBINE_MODE_COMBINE    0x00000080
#define EINK_COMBINE_MODE_MASK       0x00000080
#define EINK_DITHER_MODE_NODITHER    0x00000000
#define EINK_DITHER_MODE_DITHER      0x00000100
#define EINK_DITHER_MODE_MASK        0x00000100
#define EINK_INVERT_MODE_NOINVERT    0x00000000
#define EINK_INVERT_MODE_INVERT      0x00000200
#define EINK_INVERT_MODE_MASK        0x00000200
#define EINK_CONVERT_MODE_NOCONVERT  0x00000000
#define EINK_CONVERT_MODE_CONVERT    0x00000400
#define EINK_CONVERT_MODE_MASK       0x00000400

#define EINK_DEFAULT_MODE            0x00000004
#define ONYX_GC_MASK                 0x02000000
#define ONYX_AUTO_MASK               0x01000000
#define ONYX_GC_MODE                 EINK_WAVEFORM_MODE_GC16|EINK_UPDATE_MODE_FULL|EINK_WAIT_MODE_WAIT
#define ONYX_GU_MODE                 EINK_WAVEFORM_MODE_GC16|EINK_UPDATE_MODE_PARTIAL|EINK_WAIT_MODE_WAIT




static void update_to_display(int left, int top, int width, int height, int updatemode, int fb_dev)
{
    static __u32 marker_val = 1;
    struct mxcfb_update_data upd_data;
    int retval;
    bool wait_for_complete;
    int auto_update_mode = AUTO_UPDATE_MODE_REGION_MODE;
    memset(&upd_data, 0, sizeof(mxcfb_update_data));

    LOGI("update_to_display:left=%d, top=%d, width=%d, height=%d updatemode=0x%x\n", left, top, width, height,updatemode);


    if((updatemode & EINK_WAVEFORM_MODE_MASK) == EINK_WAVEFORM_MODE_DU)
        upd_data.waveform_mode = WAVEFORM_MODE_DU;
    else if((updatemode & EINK_WAVEFORM_MODE_MASK) == EINK_WAVEFORM_MODE_GC4)
        upd_data.waveform_mode = WAVEFORM_MODE_GC4;
    else if((updatemode & EINK_WAVEFORM_MODE_MASK) == EINK_WAVEFORM_MODE_GC16)
        upd_data.waveform_mode = WAVEFORM_MODE_GC16;
    else if((updatemode & EINK_WAVEFORM_MODE_MASK) == EINK_WAVEFORM_MODE_AUTO)
        upd_data.waveform_mode = WAVEFORM_MODE_AUTO;
    else 
        LOGI("waveform_mode  wrong\n");

    if((updatemode & EINK_AUTO_MODE_MASK) == EINK_AUTO_MODE_REGIONAL)
        auto_update_mode = AUTO_UPDATE_MODE_REGION_MODE;
    else if((updatemode & EINK_AUTO_MODE_MASK) == EINK_AUTO_MODE_AUTOMATIC)
        auto_update_mode = AUTO_UPDATE_MODE_AUTOMATIC_MODE;
    else 
        LOGI("wait_for_complete  wrong\n");

    if((updatemode & EINK_UPDATE_MODE_MASK) == EINK_UPDATE_MODE_PARTIAL)
        upd_data.update_mode = UPDATE_MODE_PARTIAL;
    else if((updatemode & EINK_UPDATE_MODE_MASK) == EINK_UPDATE_MODE_FULL)
        upd_data.update_mode = UPDATE_MODE_FULL;
    else
        LOGI("update_mode  wrong\n");

    if((updatemode & EINK_WAIT_MODE_MASK) == EINK_WAIT_MODE_NOWAIT)
        wait_for_complete = false;
    else if((updatemode & EINK_WAIT_MODE_MASK) == EINK_WAIT_MODE_WAIT)
        wait_for_complete = true;
    else 
        LOGI("wait_for_complete  wrong\n");

    if((updatemode & EINK_INVERT_MODE_MASK) == EINK_INVERT_MODE_INVERT)
    {
        upd_data.flags |= EPDC_FLAG_ENABLE_INVERSION;
        LOGI("invert mode \n");
    }

    retval = ioctl(fb_dev, MXCFB_SET_AUTO_UPDATE_MODE, &auto_update_mode);
    if (retval < 0)
    {
        LOGI("set auto update mode failed.  Error = 0x%x", retval);
    }

    upd_data.temp = 24; //the temperature is get from linux team
    upd_data.update_region.left = left;
    upd_data.update_region.width = width;
    upd_data.update_region.top = top;
    upd_data.update_region.height = height;

    if (wait_for_complete)
    {
        /* Get unique marker value */
        upd_data.update_marker = marker_val++;
    }
    else
    {
        upd_data.update_marker = 0;
    }

    retval = ioctl(fb_dev, MXCFB_SEND_UPDATE, &upd_data);
    while (retval < 0)
    {
        /* We have limited memory available for updates, so wait and
        * then try again after some updates have completed */
        usleep(300000);
        retval = ioctl(fb_dev, MXCFB_SEND_UPDATE, &upd_data);
        LOGI("MXCFB_SEND_UPDATE  retval = 0x%x try again maybe", retval);
    }

    if (wait_for_complete)
    {
        /* Wait for update to complete */
        retval = ioctl(fb_dev, MXCFB_WAIT_FOR_UPDATE_COMPLETE, &upd_data.update_marker);
        if (retval < 0)
        {
            LOGI("Wait for update complete failed.  Error = 0x%x", retval);
        }
    }
}


class UpdateHelper
{
public:
    UpdateHelper() { clear(); clearGCInterval(); }
    ~UpdateHelper(){}

public:
    void merge(int left, int top, int width, int height, int mode)
    {
        LOGI("merge update request: (%d, %d) -- (%d, %d) -- 0x%x\n", left, top, width, height, mode);
        if (left < left_)
        {
            left_ = left;
        }
        if (top < top_)
        {
            top_ = top;
        }
        if (width > width_)
        {
            width_ = width;
        }
        if (height > height_)
        {
            height_ = height;
        }

        // check waveform, gc interval, waiting mode
        checkWaveform(mode);
        checkType(mode);
        checkWaiting(mode);
        checkGCInterval(mode);
        LOGI("merge result: (%d, %d) -- (%d, %d) -- 0x%x\n", left_, top_, width_, height_, update_mode_);
    }

    void updateScreen(int fb_dev)
    {
        if (gc_interval_ >= 0 && waveform_ == EINK_WAVEFORM_MODE_AUTO)
        {
            if (gu_count_++ < gc_interval_)
            {
                update_mode_ = ONYX_GU_MODE;
            }
            else
            {
                update_mode_ = ONYX_GC_MODE;
                gu_count_ = 0;
            }
        }
        else
        {
            update_mode_ = waveform_|full_|waiting_;
        }
        LOGI("onyx_display_update: (%d, %d) -- (%d, %d) -- 0x%x\n", left_, top_, width_, height_, update_mode_);
        update_to_display(left_, top_, width_, height_, update_mode_, fb_dev);
        clear();
    }

private:
    void clear()
    {
        left_ = top_ = INT_MAX;
        width_ = height_ = 0;
        waveform_ = EINK_WAVEFORM_MODE_AUTO;
        full_ = EINK_UPDATE_MODE_PARTIAL;
        waiting_ = EINK_WAIT_MODE_NOWAIT;
        update_mode_ = EINK_DEFAULT_MODE;
    }

    void clearGCInterval()
    {
        gc_interval_ = -1;
        gu_count_ = 0;
    }

    int priority(int index)
    {
        // init, du, gc16, gc4, auto
        static const int data [] = {4, 1, 3, 2, 0};
        return data[index];
    }

    // check waveform.
    //#define EINK_WAVEFORM_MODE_INIT      0x00000000
    //#define EINK_WAVEFORM_MODE_DU        0x00000001
    //#define EINK_WAVEFORM_MODE_GC16      0x00000002
    //#define EINK_WAVEFORM_MODE_GC4       0x00000003
    //#define EINK_WAVEFORM_MODE_AUTO      0x00000004
    void checkWaveform(int mode)
    {
        int index = (mode & EINK_WAVEFORM_MODE_MASK);
        int current = (waveform_ & EINK_WAVEFORM_MODE_MASK);
        int pn = priority(index);
        int pc = priority(current);
        if (pn > pc)
        {
            waveform_ = (mode & EINK_WAVEFORM_MODE_MASK);
        }
    }

    // full or partial update.
    void checkType(int mode)
    {
        int value = mode & EINK_UPDATE_MODE_MASK;
        if (value > full_)
        {
            full_ = value;
        }
    }

    void checkWaiting(int mode)
    {
        int value = mode & EINK_WAIT_MODE_MASK;
        if (value > waiting_)
        {
            waiting_ = value;
        }
    }

    void checkGCInterval(int mode)
    {
        if (mode & ONYX_GC_MASK)
        {
            gc_interval_ = ((mode & 0x00ff0000) >> 16);
            gu_count_ = 0;
        }
        else if (mode & ONYX_AUTO_MASK)
        {
            clearGCInterval();
        }
    }

private:
    int gu_count_;
    int gc_interval_;
    int left_, top_, width_, height_;
    int waveform_;
    int full_;
    int waiting_;
    int update_mode_;
};

// Merge all display update requests
// Merge display region
// Merge update mode
// 
static void onyx_display_update(int left, int top, int width, int height, int updatemode, int fb_dev, bool update)
{
    static UpdateHelper helper;

    helper.merge(left, top, width, height, updatemode);
    if (update)
    {
        helper.updateScreen(fb_dev);
    }
}

#endif


sem_t * fslwatermark_sem_open()
{
    int fd;
    int ret;
    sem_t *pSem = NULL;
    char *shm_path, shm_file[256];

    shm_path = getenv("CODEC_SHM_PATH");      /*the CODEC_SHM_PATH is on a memory map the fs */ 

    if (shm_path == NULL)
        strcpy(shm_file, "/dev/shm");   /* default path */
    else
        strcpy(shm_file, shm_path);

    strcat(shm_file, "/"); 
    strcat(shm_file, "codec.shm");

    fd = open(shm_file, O_RDWR, 0666);
    if (fd < 0)
    {
        /* first thread/process need codec protection come here */
        fd = open(shm_file, O_RDWR | O_CREAT | O_EXCL, 0666);
        if(fd < 0)
        {
            return NULL;
        }
        ftruncate(fd, sizeof(sem_t));

        /* map the semaphore variant in the file */
        pSem = (sem_t *)mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if((void *)(-1) == pSem)
        {
            return NULL;
        }
        /* do the semaphore initialization */
        ret = sem_init(pSem, 0, 1);
        if(-1 == ret)
        {
            return NULL;
        }
    }
    else
    {
        pSem = (sem_t *)mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    }
    close(fd);
    return pSem;
}


/*****************************************************************************/
static int fb_setSwapInterval(struct framebuffer_device_t* dev,
                              int interval)
{
    fb_context_t* ctx = (fb_context_t*)dev;
    if (interval < dev->minSwapInterval || interval > dev->maxSwapInterval)
        return -EINVAL;
    // FIXME: implement fb_setSwapInterval
    return 0;
}

#ifdef FSL_EPDC_FB
static int fb_setUpdateRect(struct framebuffer_device_t* dev,
                            int* left, int* top, int* width, int* height, int* updatemode, int count)
{
    fb_context_t* ctx = (fb_context_t*)dev;
    if(count > MAX_RECT_NUM)
    {
        LOGE("count > MAX_RECT_NUM in fb_setUpdateRect\n");
        return -EINVAL;
    }

    ctx->rect_update      = true;
    ctx->count            = 0;
    for(int i=0; i < count; i++)
    {
        if (((width[i]|height[i]) <= 0) || ((left[i]|top[i])<0))  return -EINVAL;
        ctx->updatemode[i]       = updatemode[i];
        ctx->partial_left[i]     = left[i];
        ctx->partial_top[i]      = top[i];
        ctx->partial_width[i]    = width[i];
        ctx->partial_height[i]   = height[i];
    }
    ctx->count            = count;
    return 0;
}
#else
static int fb_setUpdateRect(struct framebuffer_device_t* dev,
                            int l, int t, int w, int h)
{
    if (((w|h) <= 0) || ((l|t)<0))
        return -EINVAL;
    return 0;
}

#endif



static int fb_post(struct framebuffer_device_t* dev, buffer_handle_t buffer)
{
    if (private_handle_t::validate(buffer) < 0)
        return -EINVAL;

    fb_context_t* ctx = (fb_context_t*)dev;

    private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(buffer);
    private_module_t* m = reinterpret_cast<private_module_t*>(
        dev->common.module);
    if (m->currentBuffer)
    {
        m->base.unlock(&m->base, m->currentBuffer);
        m->currentBuffer = 0;
    }

    if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER)
    {

        m->base.lock(&m->base, buffer, 
            private_module_t::PRIV_USAGE_LOCKED_FOR_POST, 
            0, 0, ALIGN_PIXEL(m->info.xres), ALIGN_PIXEL_128(m->info.yres), NULL);

        const size_t offset = hnd->base - m->framebuffer->base;
        m->info.activate = FB_ACTIVATE_VBL;
        m->info.yoffset = offset / m->finfo.line_length;

        if (ioctl(m->framebuffer->fd, FBIOPAN_DISPLAY, &m->info) == -1) {
            LOGE("FBIOPAN_DISPLAY failed");
            m->base.unlock(&m->base, buffer);
            return -errno;
        }

#ifdef FSL_EPDC_FB
        if(ctx->rect_update)
        {
            for(int i=0; i < ctx->count; i++)
            {
                onyx_display_update(ctx->partial_left[i],ctx->partial_top[i],
                    ctx->partial_width[i],ctx->partial_height[i],
                    ctx->updatemode[i], m->framebuffer->fd, (i >= ctx->count - 1));
            }
            ctx->rect_update = false;
        }
        else
        {
            onyx_display_update(0, 0, m->info.xres, m->info.yres, EINK_DEFAULT_MODE, m->framebuffer->fd, true);
        }
#endif

        m->currentBuffer = buffer;

    }
    else
    {
        // If we can't do the page_flip, just copy the buffer to the front 
        // FIXME: use copybit HAL instead of memcpy

        void* fb_vaddr;
        void* buffer_vaddr;

        m->base.lock(&m->base, m->framebuffer,
            GRALLOC_USAGE_SW_WRITE_RARELY,
            0, 0, ALIGN_PIXEL(m->info.xres), ALIGN_PIXEL_128(m->info.yres),
            &fb_vaddr);

        m->base.lock(&m->base, buffer,
            GRALLOC_USAGE_SW_READ_RARELY,
            0, 0, ALIGN_PIXEL(m->info.xres), ALIGN_PIXEL_128(m->info.yres),
            &buffer_vaddr);

        memcpy(fb_vaddr, buffer_vaddr, m->finfo.line_length * ALIGN_PIXEL_128(m->info.yres));

#ifdef FSL_EPDC_FB
        if(ctx->rect_update)
        {
            for(int i=0; i < ctx->count; i++)
            {
                onyx_display_update(ctx->partial_left[i],ctx->partial_top[i],
                    ctx->partial_width[i],ctx->partial_height[i],
                    ctx->updatemode[i],m->framebuffer->fd, (i >= ctx->count - 1));
            }
            ctx->rect_update = false;
        }
        else
        {
            onyx_display_update(0, 0, m->info.xres, m->info.yres, EINK_DEFAULT_MODE, m->framebuffer->fd, true);
        }
#endif

        m->base.unlock(&m->base, buffer); 
        m->base.unlock(&m->base, m->framebuffer); 
    }

    return 0;
}

static int fb_compositionComplete(struct framebuffer_device_t* dev)
{
    glFinish();
    return 0;
}

/*****************************************************************************/
#define SINGLE_DISPLAY_CAPABILITY  (1920 * 1080 * 60)
#define DUAL_DISPLAY_CAPABILITY    (1920 * 1080 * 30)

typedef struct
{
    char* start;
    char* end;
    int width;
    int height;
    int freq;
}
disp_mode;

static int str2int(char *p)
{
    int val = 0;
    if(!p) return -1;

    while(p[0] >= '0' && p[0] <= '9')
    {
        val = val * 10 + p[0] - '0';
        p++;
    }

    return val;
}

typedef enum
{
    CHECK_NEXT_STATE,
    FIND_WIDTH_STATE,
    FIND_JOINT_STATE,
    FIND_HEIGHT_STATE,
    PREFIX_FREQ_STATE,
    FREQUENCY_STATE,
    FIND_NEWLINE_STATE
}
read_state;

static disp_mode disp_mode_list[128];
static int disp_mode_compare( const void *arg1, const void *arg2)
{
    disp_mode *dm1 = (disp_mode *)arg1;
    disp_mode *dm2 = (disp_mode *)arg2;

    if(dm1->width * dm1->height > dm2->width * dm2->height) return -1;
    if(dm1->width * dm1->height == dm2->width * dm2->height)
    {
        return dm1->freq > dm2->freq ? -1 : 1;
    }

    return 1;
}

static char* find_available_mode(const char *mode_list, int dual_disp)
{
    int disp_threshold = 0;
    int i,disp_mode_count = 0;
    read_state state = CHECK_NEXT_STATE;
    char *p = (char *)mode_list;

    if(!p) return NULL;

    while(p[0])
    {
        switch(state)
        {
        case CHECK_NEXT_STATE:
            if(!strncmp(p, "D:", 2)
                || !strncmp(p, "S:", 2)
                || !strncmp(p, "U:", 2)
                || !strncmp(p, "V:", 2))
            {
                disp_mode_list[disp_mode_count].start = p;
                state = FIND_WIDTH_STATE;
                p+=2;
            }
            else p++;
            break;
        case FIND_WIDTH_STATE:
            if(p[0]>='0' && p[0]<='9')
            {
                disp_mode_list[disp_mode_count].width = str2int(p);
                state = FIND_JOINT_STATE;
            }
            p++;
            break;
        case FIND_JOINT_STATE:
            if(p[0] == 'x' || p[0] == 'X')
            {
                state = FIND_HEIGHT_STATE;
            }
            p++;
            break;
        case FIND_HEIGHT_STATE:
            if(p[0]>='0' && p[0]<='9')
            {
                disp_mode_list[disp_mode_count].height = str2int(p);
                state = PREFIX_FREQ_STATE;
            }
            p++;
            break;
        case PREFIX_FREQ_STATE:
            if(!strncmp(p, "p-", 2) || !strncmp(p, "i-", 2))
            {
                state = FREQUENCY_STATE;
                p+=2;
            }
            else p++;
            break;
        case  FREQUENCY_STATE:
            if(p[0]>='0' && p[0]<='9')
            {
                disp_mode_list[disp_mode_count].freq = str2int(p);
                state = FIND_NEWLINE_STATE;
            }
            p++;
            break;
        case FIND_NEWLINE_STATE:
            if(p[0] == '\n')
            {
                disp_mode_list[disp_mode_count++].end = p+1;
                state = CHECK_NEXT_STATE;
                if(disp_mode_count >= sizeof(disp_mode_list)/sizeof(disp_mode_list[0])) goto check_mode_end;
            }
            p++;
            break;
        default:
            p++;
            break;
        }
    }

check_mode_end:

    qsort(&disp_mode_list[0], disp_mode_count, sizeof(disp_mode), disp_mode_compare);

    disp_threshold = dual_disp ? DUAL_DISPLAY_CAPABILITY : SINGLE_DISPLAY_CAPABILITY;

    for(i=0; i<disp_mode_count; i++)
    {
        if(disp_mode_list[i].width * disp_mode_list[i].height * disp_mode_list[i].freq <= disp_threshold)
            break;
    }

    if(disp_mode_list[i].end) disp_mode_list[i].end[0] = 0;

    return disp_mode_list[i].start;
}

static int set_graphics_fb_mode(int fb, int dual_disp)
{
    int size=0;
    int fp_cmd=0;
    int fp_mode=0;
    int fp_modes=0;
    char fb_mode[256];
    char fb_modes[1024];
    char cmd_line[1024];
    char temp_name[256];
    char *disp_mode=NULL;

    char value[PROPERTY_VALUE_MAX];
    property_get("ro.AUTO_CONFIG_DISPLAY", value, "0");
    if (strcmp(value, "1") != 0)  return 0;

    fp_cmd = open("/proc/cmdline",O_RDONLY, 0);
    if(fp_cmd < 0)
    {
        LOGI("Error! Cannot open /proc/cmdline");
        goto set_graphics_fb_mode_error;
    }

    memset(cmd_line, 0, sizeof(cmd_line));
    size = read(fp_cmd, cmd_line, sizeof(cmd_line));
    if(size <= 0)
    {
        LOGI("Error! Cannot read /proc/cmdline");
        goto set_graphics_fb_mode_error;
    }

    close(fp_cmd); fp_cmd = 0;

    if(fb==0 && strstr(cmd_line, "di1_primary")) return 0;//XGA detected

    sprintf(temp_name, "/sys/class/graphics/fb%d/modes", fb);
    fp_modes = open(temp_name,O_RDONLY, 0);
    if(fp_modes < 0)
    {
        LOGI("Error! Cannot open %s", temp_name);
        goto set_graphics_fb_mode_error;
    }

    memset(fb_modes, 0, sizeof(fb_modes));
    size = read(fp_modes, fb_modes, sizeof(fb_modes));
    if(size <= 0)
    {
        LOGI("Error! Cannot read %s", temp_name);
        goto set_graphics_fb_mode_error;
    }

    close(fp_modes); fp_modes = 0;

    if(size == sizeof(fb_modes)) fb_modes[size -1] = 0;

    disp_mode = find_available_mode(fb_modes, dual_disp);
    if(!disp_mode)
    {
        LOGI("Error! Cannot find available mode for fb%d", fb);
        goto set_graphics_fb_mode_error;
    }

    LOGI("find fb%d available mode %s", fb,disp_mode);

    sprintf(temp_name, "/sys/class/graphics/fb%d/mode", fb);
    fp_mode = open(temp_name,O_RDWR, 0);
    if(fp_mode < 0) {
        LOGI("Error! Cannot open %s", temp_name);
        goto set_graphics_fb_mode_error;
    }

    memset(fb_mode, 0, sizeof(fb_mode));
    size = read(fp_mode, fb_mode, sizeof(fb_mode));
    if(size <= 0)
    {
        LOGI("Error! Cannot read %s", temp_name);
        goto set_graphics_fb_mode_error;
    }

    if(strncmp(fb_mode, disp_mode, strlen(fb_mode)))
    {
        write(fp_mode, disp_mode, strlen(disp_mode)+1);
    }

    close(fp_mode); fp_mode = 0;

    return 0;

set_graphics_fb_mode_error:

    if(fp_modes > 0) close(fp_modes);
    if(fp_mode > 0) close(fp_mode);
    if(fp_cmd > 0) close(fp_cmd);

    return -1;

}

int mapFrameBufferLocked(struct private_module_t* module)
{
    // already initialized...
    if (module->framebuffer)
    {
        return 0;
    }

    char const * const device_template[] =
    {
        "/dev/graphics/fb%u",
        "/dev/fb%u",
        0
    };

    int fd = -1;
    int i=0;
    char name[64];

    char value[PROPERTY_VALUE_MAX];
    property_get("ro.UI_TVOUT_DISPLAY", value, "");
    if (strcmp(value, "1") != 0)
    {
        set_graphics_fb_mode(0, 0);
        while ((fd==-1) && device_template[i])
        {
            snprintf(name, 64, device_template[i], 0);
            fd = open(name, O_RDWR, 0);
            i++;
        }
    }
    else
    {
        set_graphics_fb_mode(1, 0);
        while ((fd==-1) && device_template[i])
        {
            snprintf(name, 64, device_template[i], 1);
            fd = open(name, O_RDWR, 0);
            i++;
        }
    }

    if (fd < 0)
        return -errno;

    struct fb_fix_screeninfo finfo;
    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1)
        return -errno;

    struct fb_var_screeninfo info;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &info) == -1)
        return -errno;

    info.reserved[0] = 0;
    info.reserved[1] = 0;
    info.reserved[2] = 0;
    info.xoffset = 0;
    info.yoffset = 0;
    info.activate = FB_ACTIVATE_NOW;

    if(info.bits_per_pixel == 32)
    {
        LOGW("32bpp setting of Framebuffer catched!");
        /*
        * Explicitly request BGRA 8/8/8
        */
        info.bits_per_pixel = 32;
        info.red.offset     = 8;
        info.red.length     = 8;
        info.green.offset   = 16;
        info.green.length   = 8;
        info.blue.offset    = 24;
        info.blue.length    = 8;
        info.transp.offset  = 0;
        info.transp.length  = 0;

#ifndef FSL_EPDC_FB
        /*
        *  set the alpha in pixel
        *  only when the fb set to 32bit
        */
        struct mxcfb_loc_alpha l_alpha;
        l_alpha.enable = true;
        l_alpha.alpha_in_pixel = true;
        if (ioctl(fd, MXCFB_SET_LOC_ALPHA,            &l_alpha) < 0)
        {
            printf("Set local alpha failed\n");
            close(fd);
            return -errno;
        }
#endif
    }
    else
    {
        /*
        * Explicitly request 5/6/5
        */
        info.bits_per_pixel = 16;
        info.red.offset     = 11;
        info.red.length     = 5;
        info.green.offset   = 5;
        info.green.length   = 6;
        info.blue.offset    = 0;
        info.blue.length    = 5;
        info.transp.offset  = 0;
        info.transp.length  = 0;

        if (!no_ipu)
        {
            /* for the 16bit case, only involke the glb alpha */
            struct mxcfb_gbl_alpha gbl_alpha;

            gbl_alpha.alpha = 255;
            gbl_alpha.enable = 1;
            int ret = ioctl(fd, MXCFB_SET_GBL_ALPHA, &gbl_alpha);
            if(ret <0)
            {
                LOGE("Error!MXCFB_SET_GBL_ALPHA failed!");
                return -1;
            }

            struct mxcfb_color_key key;
            key.enable = 1;
            key.color_key = 0x00000000; // Black
            ret = ioctl(fd, MXCFB_SET_CLR_KEY, &key);
            if(ret <0)
            {
                LOGE("Error!Colorkey setting failed for dev ");
                return -1;
            }
        }
    }

    /*
    * Request nr_framebuffers screens (at lest 2 for page flipping)
    */
    info.yres_virtual = ALIGN_PIXEL_128(info.yres) * nr_framebuffers;
    info.xres_virtual = ALIGN_PIXEL(info.xres);

#ifdef FSL_EPDC_FB
    info.bits_per_pixel = 16;
    info.grayscale = 0;
    info.yoffset = 0;
#endif

    uint32_t flags = PAGE_FLIP;
    if (ioctl(fd, FBIOPUT_VSCREENINFO, &info) == -1)
    {
        info.yres_virtual = ALIGN_PIXEL_128(info.yres);
        flags &= ~PAGE_FLIP;
        LOGW("FBIOPUT_VSCREENINFO failed, page flipping not supported");
    }

    if (info.yres_virtual < ALIGN_PIXEL_128(info.yres) * 2)
    {
        // we need at least 2 for page-flipping
        info.yres_virtual = ALIGN_PIXEL_128(info.yres);
        flags &= ~PAGE_FLIP;
        LOGW("page flipping not supported (yres_virtual=%d, requested=%d)",
            info.yres_virtual, ALIGN_PIXEL_128(info.yres)*2);
    }

    if (ioctl(fd, FBIOGET_VSCREENINFO, &info) == -1)
        return -errno;

#ifdef FSL_EPDC_FB
    int auto_update_mode = AUTO_UPDATE_MODE_REGION_MODE;
    int retval = ioctl(fd, MXCFB_SET_AUTO_UPDATE_MODE, &auto_update_mode);
    if (retval < 0)
    {
        LOGE("Error! set auto update mode error!\n");
        return -errno;
    }

    int scheme_mode = UPDATE_SCHEME_QUEUE_AND_MERGE;
    retval = ioctl(fd, MXCFB_SET_UPDATE_SCHEME, &scheme_mode);
    if (retval < 0)
    {
        LOGE("Error! set update scheme error!\n");
        return -errno;
    }
#endif
    int refreshRate = 1000000000000000LLU /
        (
        uint64_t( info.upper_margin + info.lower_margin + info.yres )
        * ( info.left_margin  + info.right_margin + info.xres )
        * info.pixclock
        );

    if (refreshRate == 0)
    {
        // bleagh, bad info from the driver
        refreshRate = 60*1000;  // 60 Hz
    }

    if (int(info.width) <= 0 || int(info.height) <= 0)
    {
        // the driver doesn't return that information
        // default to 160 dpi
        info.width  = ((info.xres * 25.4f)/160.0f + 0.5f);
        info.height = ((info.yres * 25.4f)/160.0f + 0.5f);
    }

    float xdpi = (info.xres * 25.4f) / info.width;
    float ydpi = (info.yres * 25.4f) / info.height;
    float fps  = refreshRate / 1000.0f;

    LOGI(   "using (fd=%d)\n"
        "id           = %s\n"
        "xres         = %d px\n"
        "yres         = %d px\n"
        "xres_virtual = %d px\n"
        "yres_virtual = %d px\n"
        "bpp          = %d\n"
        "r            = %2u:%u\n"
        "g            = %2u:%u\n"
        "b            = %2u:%u\n",
        fd,
        finfo.id,
        info.xres,
        info.yres,
        info.xres_virtual,
        info.yres_virtual,
        info.bits_per_pixel,
        info.red.offset, info.red.length,
        info.green.offset, info.green.length,
        info.blue.offset, info.blue.length
        );

    LOGI(   "width        = %d mm (%f dpi)\n"
        "height       = %d mm (%f dpi)\n"
        "refresh rate = %.2f Hz\n",
        info.width,  xdpi,
        info.height, ydpi,
        fps
        );


    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1)
        return -errno;

    if (finfo.smem_len <= 0)
        return -errno;


    module->flags = flags;
    module->info = info;
    module->finfo = finfo;
    module->xdpi = xdpi;
    module->ydpi = ydpi;
    module->fps = fps;

    /*
    * map the framebuffer
    */

    int err;
    size_t fbSize = roundUpToPageSize(finfo.line_length * info.yres_virtual);
    module->framebuffer = new private_handle_t(dup(fd), fbSize,
        private_handle_t::PRIV_FLAGS_USES_PMEM);

    module->numBuffers = info.yres_virtual / ALIGN_PIXEL_128(info.yres);
    module->bufferMask = 0;

    void* vaddr = mmap(0, fbSize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (vaddr == MAP_FAILED)
    {
        LOGE("Error mapping the framebuffer (%s)", strerror(errno));
        return -errno;
    }
    module->framebuffer->base = intptr_t(vaddr);
    module->framebuffer->phys = intptr_t(finfo.smem_start);
    memset(vaddr, 0, fbSize);
    return 0;
}

static int mapFrameBuffer(struct private_module_t* module)
{
    pthread_mutex_lock(&module->lock);
    int err = mapFrameBufferLocked(module);
    pthread_mutex_unlock(&module->lock);
    return err;
}


/*****************************************************************************/
static int fb_close(struct hw_device_t *dev)
{
    fb_context_t* ctx = (fb_context_t*)dev;
    if (ctx)
    {
        free(ctx);
    }
    return 0;
}

int fb_device_open(hw_module_t const* module,
                   const char* name,
                   hw_device_t** device)
{
    int status = -EINVAL;
    char value[PROPERTY_VALUE_MAX];

    if (!strcmp(name, GRALLOC_HARDWARE_FB0))
    {
        alloc_device_t* gralloc_device;
        framebuffer_device_t *fbdev;

        nr_framebuffers = NUM_BUFFERS;
        property_get("ro.product.device", value, "");
        if (0 == strcmp(value, "imx50_rdp"))
        {
            nr_framebuffers = 2;
            no_ipu = 1;
        }

        status = gralloc_open(module, &gralloc_device);
        if (status < 0)
            return status;

        /* initialize our state here */
        fb_context_t *dev = (fb_context_t*)malloc(sizeof(*dev));
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = 0;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = fb_close;
        dev->device.setSwapInterval = fb_setSwapInterval;
        dev->device.post            = fb_post;
#ifndef FSL_EPDC_FB
        dev->device.setUpdateRect = 0;
#else
        dev->device.setUpdateRect = fb_setUpdateRect; // used by native window
#endif
        dev->device.compositionComplete = fb_compositionComplete;

        private_module_t* m = (private_module_t*)module;
        status = mapFrameBuffer(m);
        if (status >= 0)
        {
            int stride = m->finfo.line_length / (m->info.bits_per_pixel >> 3);
            const_cast<uint32_t&>(dev->device.flags) = 0xfb0;
            const_cast<uint32_t&>(dev->device.width) = m->info.xres;
            const_cast<uint32_t&>(dev->device.height) = m->info.yres;
            const_cast<int&>(dev->device.stride) = stride;
            if(m->info.bits_per_pixel != 32)
            {
                const_cast<int&>(dev->device.format) = HAL_PIXEL_FORMAT_RGB_565;
            }
            else
            {
                const_cast<int&>(dev->device.format) = HAL_PIXEL_FORMAT_BGRA_8888;
            }
            const_cast<float&>(dev->device.xdpi) = m->xdpi;
            const_cast<float&>(dev->device.ydpi) = m->ydpi;
            const_cast<float&>(dev->device.fps) = m->fps;
            const_cast<int&>(dev->device.minSwapInterval) = 1;
            const_cast<int&>(dev->device.maxSwapInterval) = 1;
            *device = &dev->device.common;
            fbdev = (framebuffer_device_t*) *device;
            fbdev->reserved[0] = nr_framebuffers;
        }

        /* initialize the IPU lib IPC */
        if (!no_ipu)
            mxc_ipu_lib_ipc_init();

        fslwatermark_sem_open();

    }
    return status;
}
