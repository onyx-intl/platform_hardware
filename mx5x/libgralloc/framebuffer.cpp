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

extern "C" {
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
/*****************************************************************************/

// numbers of buffers for page flipping
#define NUM_BUFFERS 3

enum {
    PAGE_FLIP = 0x00000001,
    LOCKED = 0x00000002
};

enum {
    SIN_VIDEO_DUAL_UI,
    DUAL_VIDEO_SIN_UI
};

struct fb_context_t {
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
#ifdef SECOND_DISPLAY_SUPPORT
    bool sec_display_inited;
    int sec_fp;
    int sec_disp_w;
    int sec_disp_h;
    int sec_disp_base;
    int sec_disp_phys;
    int sec_frame_size;
    int sec_disp_next_buf;
    struct fb_var_screeninfo sec_info;
    struct fb_fix_screeninfo sec_finfo;
    struct framebuffer_device_t* dev;
    buffer_handle_t buffer;
    sem_t sec_display_begin;
    sem_t sec_display_end;
    pthread_t thread_id;
    C2D_CONTEXT c2dctx;
    int sec_rotation;
    int cleancount;
    int mRotate;
    int video_play_mode;
#endif
};

static int nr_framebuffers;
static int no_ipu = 0;

#ifdef SECOND_DISPLAY_SUPPORT
#define MAX_SEC_DISP_WIDTH (1024)
#define MAX_SEC_DISP_HEIGHT (1024)
static int mapSecFrameBuffer(fb_context_t* ctx);
static int resizeToSecFrameBuffer(int base,int phys,fb_context_t* ctx);
static int resizeToSecFrameBuffer_c2d(int base,int phys,fb_context_t* ctx);
void * secDispShowFrames(void * arg);
#endif

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

__u32 marker_val = 1;
static void update_to_display(int left, int top, int width, int height, int updatemode, int fb_dev)
{
	struct mxcfb_update_data upd_data;
	int retval;
	bool wait_for_complete;
	int auto_update_mode = AUTO_UPDATE_MODE_REGION_MODE;
	memset(&upd_data, 0, sizeof(mxcfb_update_data));

    LOGI("update_to_display:left=%d, top=%d, width=%d, height=%d updatemode=%d\n", left, top, width, height,updatemode);


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
	if (retval < 0) {
		LOGI("set auto update mode failed.  Error = 0x%x", retval);
	}    
    
    upd_data.temp = 24; //the temperature is get from linux team
	upd_data.update_region.left = left;
	upd_data.update_region.width = width;
	upd_data.update_region.top = top;
	upd_data.update_region.height = height;

	if (wait_for_complete) {
		/* Get unique marker value */
		upd_data.update_marker = marker_val++;
	} else {
		upd_data.update_marker = 0;
	}

	retval = ioctl(fb_dev, MXCFB_SEND_UPDATE, &upd_data);
	while (retval < 0) {
		/* We have limited memory available for updates, so wait and
		 * then try again after some updates have completed */
		usleep(300000);
		retval = ioctl(fb_dev, MXCFB_SEND_UPDATE, &upd_data);
        LOGI("MXCFB_SEND_UPDATE  retval = 0x%x try again maybe", retval);
	}

	if (wait_for_complete) {
		/* Wait for update to complete */
		retval = ioctl(fb_dev, MXCFB_WAIT_FOR_UPDATE_COMPLETE, &upd_data.update_marker);
		if (retval < 0) {
			LOGI("Wait for update complete failed.  Error = 0x%x", retval);
		}
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
    if (fd < 0) { 
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
      pSem = (sem_t *)mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

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

#ifdef SECOND_DISPLAY_SUPPORT
static int fb_setSecRotation(struct framebuffer_device_t* dev,int secRotation)
{
    fb_context_t* ctx = (fb_context_t*)dev;
    //LOGI("fb_setSecRotation %d",secRotation);
    if((ctx->sec_rotation != secRotation)&&(ctx->sec_disp_base != 0))      
       memset((void *)ctx->sec_disp_base, 0, ctx->sec_frame_size*nr_framebuffers);
    ctx->sec_rotation = secRotation;
    switch(secRotation)
    {
    case 4:
       ctx->mRotate = 90;
       break;
    case 3:
       ctx->mRotate = 180;
       break;
    case 7:
       ctx->mRotate = 270;
       break;
    case 0:
    default:
       ctx->mRotate = 0;
       break;
    }
    return 0;
}
#endif
#endif



static int fb_post(struct framebuffer_device_t* dev, buffer_handle_t buffer)
{
    if (private_handle_t::validate(buffer) < 0)
        return -EINVAL;

    fb_context_t* ctx = (fb_context_t*)dev;

    private_handle_t const* hnd = reinterpret_cast<private_handle_t const*>(buffer);
    private_module_t* m = reinterpret_cast<private_module_t*>(
            dev->common.module);
    if (m->currentBuffer) {
        m->base.unlock(&m->base, m->currentBuffer);
        m->currentBuffer = 0;
    }

    if (hnd->flags & private_handle_t::PRIV_FLAGS_FRAMEBUFFER) {

        m->base.lock(&m->base, buffer, 
                private_module_t::PRIV_USAGE_LOCKED_FOR_POST, 
                0, 0, ALIGN_PIXEL(m->info.xres), ALIGN_PIXEL_128(m->info.yres), NULL);

        const size_t offset = hnd->base - m->framebuffer->base;
        m->info.activate = FB_ACTIVATE_VBL;
        m->info.yoffset = offset / m->finfo.line_length;

        #ifdef SECOND_DISPLAY_SUPPORT
        //Check the prop rw.SECOND_DISPLAY_CONNECTED
        char value[PROPERTY_VALUE_MAX];
     
        property_get("rw.SECOND_DISPLAY_CONNECTED", value, "");
        if (strcmp(value, "1") == 0) {
            if(!ctx->sec_display_inited) {
                //Init the second display
                if(mapSecFrameBuffer(ctx)== 0)
                {    
                    ctx->sec_display_inited = true;
                    c2dCreateContext(&ctx->c2dctx); 

                    sem_init(&ctx->sec_display_begin, 0, 0);
                    sem_init(&ctx->sec_display_end, 0, 0);
                     
                    pthread_create(&ctx->thread_id, NULL, &secDispShowFrames, (void *)ctx);
                                        
                    //Set the prop rw.SECOND_DISPLAY_ENABLED to 1
                    LOGI("sys.SECOND_DISPLAY_ENABLED Set to 1");
                    property_set("sys.SECOND_DISPLAY_ENABLED", "1");
                }
            }

            if(ctx->sec_display_inited) {
                //Resize the primary display to the second display
                ctx->dev = dev;
                ctx->buffer = buffer;
                sem_post(&ctx->sec_display_begin);
            }
        }
        else{
            if(ctx->sec_display_inited) {
                
                ctx->sec_display_inited = false;
                
                sem_post(&ctx->sec_display_begin);
                sem_wait(&ctx->sec_display_end);
                
                sem_destroy(&ctx->sec_display_begin);
                sem_destroy(&ctx->sec_display_end);
                
                if (ctx->c2dctx != NULL)c2dDestroyContext(ctx->c2dctx);
                
                //Set the prop rw.SECOND_DISPLAY_ENABLED to 0
                LOGI("Switch back to display 0");
                LOGI("sys.SECOND_DISPLAY_ENABLED Set to 0");
                property_set("sys.SECOND_DISPLAY_ENABLED", "0");
                memset((void *)ctx->sec_disp_base, 0, ctx->sec_frame_size*nr_framebuffers);
                //unmap the sec_disp_base
                munmap((void *)ctx->sec_disp_base, ctx->sec_frame_size*nr_framebuffers);
                ctx->sec_disp_base = 0;
                //DeInit the second display
                if(ctx->sec_fp && ctx->video_play_mode == SIN_VIDEO_DUAL_UI) {
                    int fp_property = open("/sys/class/graphics/fb1/fsl_disp_property",O_RDWR, 0); 
                    if(fp_property >= 0) {
                        char overlayStr[32];
                        int blank;
                        int fb2_fp;
			struct fb_var_screeninfo fb0_var;

                        blank = 1;

                        fb2_fp = open("/dev/graphics/fb2",O_RDWR, 0);
                        if (fb2_fp < 0){
                            LOGE("Error!Cannot open the /dev/graphics/fb2");
                        }
                        else{
                            if(ioctl(fb2_fp, FBIOBLANK, blank) < 0) {
                        		LOGI("Error!BLANK FB2 failed!\n");
                        	}
                            close(fb2_fp);
                        }

                    	if(ioctl(ctx->sec_fp, FBIOBLANK, blank) < 0) {
                    		LOGI("Error!BLANK FB1 failed!\n");
                    	}
                    
                        if(ioctl(m->framebuffer->fd, FBIOBLANK, blank) < 0) {
                    		LOGI("Error!BLANK FB0 failed!\n");
                    	}

                        memset(overlayStr, 0 ,32);
                        strcpy(overlayStr, "1-layer-fb\n");
                        LOGI("WRITE 1-layer-fb to fb1/fsl_disp_property");
                        write(fp_property, overlayStr, strlen(overlayStr)+1);
                        close(fp_property);

                        blank = FB_BLANK_POWERDOWN;
                    	if(ioctl(ctx->sec_fp, FBIOBLANK, blank) < 0) {
                    		LOGI("Error!BLANK FB1 failed!\n");
                    	}
                        blank = FB_BLANK_UNBLANK;
                    	if(ioctl(m->framebuffer->fd, FBIOBLANK, blank) < 0) {
                    		LOGI("Error!UNBLANK FB0 failed!\n");
                    	}

			if (ioctl(m->framebuffer->fd, FBIOGET_VSCREENINFO,
				  &fb0_var) < 0) {
                            LOGE("Error!Cannot get var info for fb0");
			}

			if (fb0_var.bits_per_pixel == 32) {
				struct mxcfb_loc_alpha l_alpha;

				l_alpha.enable = true;
				l_alpha.alpha_in_pixel = true;
				if (ioctl(m->framebuffer->fd, MXCFB_SET_LOC_ALPHA,
				            &l_alpha) < 0) {
				    LOGE("Error!MXCFB_SET_LOC_ALPHA failed!");
				}
			} else {
				struct mxcfb_gbl_alpha gbl_alpha;
	                        struct mxcfb_color_key key;

				gbl_alpha.alpha = 255;
				gbl_alpha.enable = 1;
				if (ioctl(m->framebuffer->fd, MXCFB_SET_GBL_ALPHA,
					  &gbl_alpha) < 0) {
				    LOGE("Error!MXCFB_SET_GBL_ALPHA failed!");
				}

	                        key.enable = 1;
	                        key.color_key = 0x00000000; // Black
	                        LOGI("MXCFB_SET_CLR_KEY");
	                        if( ioctl(m->framebuffer->fd, MXCFB_SET_CLR_KEY,
					  &key) < 0)
	                        {
	                            LOGE("Error!MXCFB_SET_CLR_KEY for fb0");
	                        }
			}
                    }
                    close(ctx->sec_fp);
                    ctx->sec_fp = 0;
                }
            }
        }

        #endif

        if (ioctl(m->framebuffer->fd, FBIOPAN_DISPLAY, &m->info) == -1) {
            LOGE("FBIOPAN_DISPLAY failed");
            m->base.unlock(&m->base, buffer); 
            return -errno;
        }

    #ifdef SECOND_DISPLAY_SUPPORT
        if(ctx->sec_display_inited) sem_wait(&ctx->sec_display_end);
    #endif

#ifdef FSL_EPDC_FB
        if(ctx->rect_update) {
            for(int i=0; i < ctx->count; i++)
            {
                update_to_display(ctx->partial_left[i],ctx->partial_top[i],
                              ctx->partial_width[i],ctx->partial_height[i],
                              ctx->updatemode[i],m->framebuffer->fd);
            }

            ctx->rect_update = false;
        }
        else{
            update_to_display(0,0,m->info.xres,m->info.yres,EINK_DEFAULT_MODE,m->framebuffer->fd);
        }
#endif

        m->currentBuffer = buffer;
        
    } else {
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
        if(ctx->rect_update) {
            for(int i=0; i < ctx->count; i++)
            {
                update_to_display(ctx->partial_left[i],ctx->partial_top[i],
                              ctx->partial_width[i],ctx->partial_height[i],
                              ctx->updatemode[i],m->framebuffer->fd);
            }

            ctx->rect_update = false;
        }
        else{
            update_to_display(0,0,m->info.xres,m->info.yres, EINK_DEFAULT_MODE ,m->framebuffer->fd);
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

typedef enum {
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
    if(fp_cmd < 0) {
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
    if(fp_modes < 0) {
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
    if (module->framebuffer) {
        return 0;
    }
        
    char const * const device_template[] = {
            "/dev/graphics/fb%u",
            "/dev/fb%u",
            0 };

    int fd = -1;
    int i=0;
    char name[64];

    char value[PROPERTY_VALUE_MAX];
    property_get("ro.UI_TVOUT_DISPLAY", value, "");
    if (strcmp(value, "1") != 0) {
        set_graphics_fb_mode(0, 0);
        while ((fd==-1) && device_template[i]) {
            snprintf(name, 64, device_template[i], 0);
            fd = open(name, O_RDWR, 0);
            i++;
        }
    }
    else{
        set_graphics_fb_mode(1, 0);
        while ((fd==-1) && device_template[i]) {
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

    if(info.bits_per_pixel == 32){
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
        if (ioctl(fd, MXCFB_SET_LOC_ALPHA,
                    &l_alpha) < 0) {
            printf("Set local alpha failed\n");
            close(fd);
            return -errno;
        }
#endif
    }
    else{
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

        if (!no_ipu) {
            /* for the 16bit case, only involke the glb alpha */
            struct mxcfb_gbl_alpha gbl_alpha;

            gbl_alpha.alpha = 255;
            gbl_alpha.enable = 1;
            int ret = ioctl(fd, MXCFB_SET_GBL_ALPHA, &gbl_alpha);
            if(ret <0) {
	        LOGE("Error!MXCFB_SET_GBL_ALPHA failed!");
	        return -1;
            }

            struct mxcfb_color_key key;
            key.enable = 1;
            key.color_key = 0x00000000; // Black
            ret = ioctl(fd, MXCFB_SET_CLR_KEY, &key);
            if(ret <0) {
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
    if (ioctl(fd, FBIOPUT_VSCREENINFO, &info) == -1) {
        info.yres_virtual = ALIGN_PIXEL_128(info.yres);
        flags &= ~PAGE_FLIP;
        LOGW("FBIOPUT_VSCREENINFO failed, page flipping not supported");
    }

    if (info.yres_virtual < ALIGN_PIXEL_128(info.yres) * 2) {
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
    if (retval < 0) {
	LOGE("Error! set auto update mode error!\n");
	return -errno;
    }

    int scheme_mode = UPDATE_SCHEME_QUEUE_AND_MERGE;
    retval = ioctl(fd, MXCFB_SET_UPDATE_SCHEME, &scheme_mode);
    if (retval < 0) {
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

    if (refreshRate == 0) {
        // bleagh, bad info from the driver
        refreshRate = 60*1000;  // 60 Hz
    }

    if (int(info.width) <= 0 || int(info.height) <= 0) {
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
    if (vaddr == MAP_FAILED) {
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

#ifdef SECOND_DISPLAY_SUPPORT
static int mapSecFrameBuffer(fb_context_t* ctx)
{
    int retCode = 0;
    int sec_fp = 0,fp_property = 0;
    size_t fbSize = 0;
    int blank;
    void* vaddr = NULL;
    struct mxcfb_gbl_alpha gbl_alpha;
    struct mxcfb_color_key key; 
    char overlayStr[32], value[PROPERTY_VALUE_MAX];
    int fb2_fp;
    private_module_t* m = reinterpret_cast<private_module_t*>(
            ctx->device.common.module);

    set_graphics_fb_mode(1,1);

    sec_fp = open("/dev/graphics/fb1",O_RDWR, 0);
    if (sec_fp < 0){
        LOGE("Error!Cannot open the /dev/graphics/fb1 for second display");
        goto disp_init_error;
    }

    /*
     * If we play video on the secondary display device only,
     * switch overlay(IPU DP) to the secondary display FB
     * after all FBs are blanked.
     */
    property_get("ro.SIN_VIDEO_DUAL_UI", value, "");
    if (!strcmp(value, "1")) {
        ctx->video_play_mode = SIN_VIDEO_DUAL_UI;

        blank = FB_BLANK_NORMAL;

        fb2_fp = open("/dev/graphics/fb2",O_RDWR, 0);
        if (fb2_fp < 0){
            LOGE("Error!Cannot open the /dev/graphics/fb2");
            goto disp_init_error;
        }
        if(ioctl(fb2_fp, FBIOBLANK, blank) < 0) {
            LOGI("Error!BLANK FB0 failed!\n");
            goto disp_init_error;
        }
        close(fb2_fp);

        if(ioctl(sec_fp, FBIOBLANK, blank) < 0) {
            LOGI("Error!BLANK FB1 failed!\n");
            goto disp_init_error;
        }

        if(ioctl(m->framebuffer->fd, FBIOBLANK, blank) < 0) {
            LOGI("Error!BLANK FB0 failed!\n");
            goto disp_init_error;
        }

        LOGI("Open fb0/fsl_disp_property");
        fp_property = open("/sys/class/graphics/fb0/fsl_disp_property", O_RDWR, 0);
        if(fp_property < 0) {
             LOGI("Error!Cannot switch the overlay to second disp");
             goto disp_init_error;
        }

        memset(overlayStr, 0 ,32);
        strcpy(overlayStr, "1-layer-fb\n");
        LOGI("WRITE 1-layer-fb to fb0/fsl_disp_property");
        write(fp_property, overlayStr, strlen(overlayStr)+1);
        close(fp_property);

        blank = FB_BLANK_UNBLANK;
        if(ioctl(m->framebuffer->fd, FBIOBLANK, blank) < 0) {
             LOGI("Error!UNBLANK FB0 failed!\n");
             goto disp_init_error;
        }

        key.enable = 1;
        key.color_key = 0x00000000; // Black
        LOGI("MXCFB_SET_CLR_KEY");
        if( ioctl(sec_fp, MXCFB_SET_CLR_KEY, &key) < 0)
        {
            LOGE("Error!MXCFB_SET_CLR_KEY");
            goto disp_init_error;
        }

        gbl_alpha.alpha = 255;
        gbl_alpha.enable = 1;
        LOGI("MXCFB_SET_GBL_ALPHA");
        if(ioctl(sec_fp, MXCFB_SET_GBL_ALPHA, &gbl_alpha) <0)
        {
            LOGI("Error!MXCFB_SET_GBL_ALPHA error");
            goto disp_init_error;
        }
    } else
        ctx->video_play_mode = DUAL_VIDEO_SIN_UI;

    blank = FB_BLANK_UNBLANK;
    if(ioctl(sec_fp, FBIOBLANK, blank) < 0) {
         LOGI("Error!UNBLANK FB1 failed!\n");
         goto disp_init_error;
    }

    struct fb_fix_screeninfo finfo;
    if (ioctl(sec_fp, FBIOGET_FSCREENINFO, &finfo) == -1)
       goto disp_init_error;
                
    struct fb_var_screeninfo info;
    if (ioctl(sec_fp, FBIOGET_VSCREENINFO, &info) == -1)
        goto disp_init_error;
                
    LOGI("Second display: xres %d,yres %d, xres_virtual %d, yres_virtual %d",
         info.xres,info.xres_virtual,info.yres,info.yres_virtual);

    info.reserved[0] = 0;
    info.reserved[1] = 0;
    info.reserved[2] = 0;
    info.xoffset = 0;
    info.yoffset = 0;
    info.activate = FB_ACTIVATE_NOW;
                
    /*
    * Explicitly request 5/6/5
    */
    info.bits_per_pixel = 16;
    info.nonstd = 0;
    info.red.offset     = 11;
    info.red.length     = 5;
    info.green.offset   = 5;
    info.green.length   = 6;
    info.blue.offset    = 0;
    info.blue.length    = 5;
    info.transp.offset  = 0;
    info.transp.length  = 0;
    info.yres_virtual = ALIGN_PIXEL_128(info.yres) * nr_framebuffers;
    info.xres_virtual = ALIGN_PIXEL(info.xres);
                        
    if (ioctl(sec_fp, FBIOPUT_VSCREENINFO, &info) == -1) {
        LOGE("Error!Second display FBIOPUT_VSCREENINFO");
        goto disp_init_error;
    }
                    
    if (ioctl(sec_fp, FBIOGET_VSCREENINFO, &info) == -1){
        LOGE("Error!Second display FBIOGET_VSCREENINFO");
        goto disp_init_error;
    }
                    
    if (ioctl(sec_fp, FBIOGET_FSCREENINFO, &finfo) == -1){
        LOGE("Error!Second display FBIOGET_FSCREENINFO");
        goto disp_init_error;
    }
                    
    if(finfo.smem_len <= 0)
        goto disp_init_error;

    fbSize = roundUpToPageSize(finfo.line_length * info.yres_virtual);  
                   
    vaddr = mmap(0, fbSize, PROT_READ|PROT_WRITE, MAP_SHARED, sec_fp, 0);
    if (vaddr == MAP_FAILED) {
        LOGE("Error!mapping the framebuffer (%s)", strerror(errno));
        goto disp_init_error;
    }

    ctx->sec_disp_base = intptr_t(vaddr);
    ctx->sec_disp_phys = intptr_t(finfo.smem_start);
    memset(vaddr, 0, fbSize);
    ctx->sec_fp = sec_fp;
    ctx->sec_disp_w = info.xres;
    ctx->sec_disp_h = info.yres;
    ctx->sec_frame_size = fbSize/nr_framebuffers;
    ctx->sec_disp_next_buf = ((info.yoffset * nr_framebuffers / info.yres_virtual) + 1) % nr_framebuffers;
    ctx->sec_info = info;
    ctx->sec_finfo = finfo;
#if 0
    struct fb_fix_screeninfo fb0_finfo;
    if (ioctl(m->framebuffer->fd, FBIOGET_FSCREENINFO, &fb0_finfo) == -1)
       goto disp_init_error;
                
    struct fb_var_screeninfo fb0_info;
    if (ioctl(m->framebuffer->fd, FBIOGET_VSCREENINFO, &fb0_info) == -1)
        goto disp_init_error;
                
    LOGI("fb0_info display: xres %d,yres %d, xres_virtual %d, yres_virtual %d",
         fb0_info.xres,fb0_info.xres_virtual,
         fb0_info.yres,fb0_info.yres_virtual);

    fb0_info.reserved[0] = 0;
    fb0_info.reserved[1] = 0;
    fb0_info.reserved[2] = 0;
    fb0_info.xoffset = 0;
    fb0_info.yoffset = 0;
    fb0_info.activate = FB_ACTIVATE_NOW;
                
    /*
    * Explicitly request 5/6/5
    */
    fb0_info.bits_per_pixel = 16;
    fb0_info.nonstd = 0;
    fb0_info.red.offset     = 11;
    fb0_info.red.length     = 5;
    fb0_info.green.offset   = 5;
    fb0_info.green.length   = 6;
    fb0_info.blue.offset    = 0;
    fb0_info.blue.length    = 5;
    fb0_info.transp.offset  = 0;
    fb0_info.transp.length  = 0;
    fb0_info.yres_virtual = ALIGN_PIXEL_128(fb0_info.yres) * nr_framebuffers;
	fb0_info.xres_virtual = fb0_info.xres;
#endif
    if (ioctl(m->framebuffer->fd, FBIOPUT_VSCREENINFO, &m->info) == -1) {
        LOGE("Error!Second display FBIOPUT_VSCREENINFO");
        goto disp_init_error;
    }

    return 0;

 disp_init_error:
    if(sec_fp) {
        close(sec_fp);
        sec_fp = 0;
        ctx->sec_fp = 0;
    }
    return -1;
}

static int resizeToSecFrameBuffer(int base,int phys,fb_context_t* ctx)
{
    ipu_lib_input_param_t sIPUInputParam;   
    ipu_lib_output_param_t sIPUOutputParam; 
    ipu_lib_handle_t            sIPUHandle;
    int iIPURet = 0;
    memset(&sIPUInputParam,0,sizeof(sIPUInputParam));
    memset(&sIPUOutputParam,0,sizeof(sIPUOutputParam));
    memset(&sIPUHandle,0,sizeof(sIPUHandle));

    //Setting input format
    sIPUInputParam.width = ctx->device.width;
    sIPUInputParam.height = ctx->device.height;

    sIPUInputParam.input_crop_win.pos.x = 0;
    sIPUInputParam.input_crop_win.pos.y = 0;  
    sIPUInputParam.input_crop_win.win_w = ctx->device.width;
    sIPUInputParam.input_crop_win.win_h = ctx->device.height;
    sIPUInputParam.fmt = v4l2_fourcc('R', 'G', 'B', 'P');
    sIPUInputParam.user_def_paddr[0] = phys;
        
    //Setting output format
    //Should align with v4l
    sIPUOutputParam.fmt = v4l2_fourcc('R', 'G', 'B', 'P');
    sIPUOutputParam.width = ctx->sec_disp_w;
    sIPUOutputParam.height = ctx->sec_disp_h;   
    sIPUOutputParam.show_to_fb = 0;
    //Output param should be same as input, since no resize,crop
    sIPUOutputParam.output_win.pos.x = 0;
    sIPUOutputParam.output_win.pos.y = 0;
    sIPUOutputParam.output_win.win_w = ctx->sec_disp_w;
    sIPUOutputParam.output_win.win_h = ctx->sec_disp_h;
    
    int output_w = 0;
    int output_h = 0;
    //Make sure the output w/h proportion is align with the primary display
    if((ctx->sec_rotation == 0x0)||(ctx->sec_rotation == 0x3))
    {
        if(ctx->sec_disp_w/ctx->sec_disp_h >= ctx->device.width/ctx->device.height){
            sIPUOutputParam.output_win.win_h = ctx->sec_disp_h > MAX_SEC_DISP_HEIGHT?MAX_SEC_DISP_HEIGHT:ctx->sec_disp_h;
            sIPUOutputParam.output_win.win_w = ctx->sec_disp_h*ctx->device.width/ctx->device.height;
        }
        else{
            sIPUOutputParam.output_win.win_w = ctx->sec_disp_w > MAX_SEC_DISP_WIDTH?MAX_SEC_DISP_WIDTH:ctx->sec_disp_w;
            sIPUOutputParam.output_win.win_h = ctx->sec_disp_w*ctx->device.height/ctx->device.width;
        }
    }
    else{
        if(ctx->sec_disp_w/ctx->sec_disp_h >= ctx->device.height/ctx->device.width){
            sIPUOutputParam.output_win.win_h = ctx->sec_disp_h > MAX_SEC_DISP_HEIGHT?MAX_SEC_DISP_HEIGHT:ctx->sec_disp_h;
            sIPUOutputParam.output_win.win_w = ctx->sec_disp_h*ctx->device.height/ctx->device.width;
        }
        else{
            sIPUOutputParam.output_win.win_w = ctx->sec_disp_w > MAX_SEC_DISP_WIDTH?MAX_SEC_DISP_WIDTH:ctx->sec_disp_w;
            sIPUOutputParam.output_win.win_h = ctx->sec_disp_w*ctx->device.width/ctx->device.height;
        }
    }
    sIPUOutputParam.output_win.pos.x = (ctx->sec_disp_w - sIPUOutputParam.output_win.win_w)/2;
    sIPUOutputParam.output_win.pos.y = (ctx->sec_disp_h - sIPUOutputParam.output_win.win_h)/2;


    //sIPUOutputParam.rot = 0;
    sIPUOutputParam.rot = ctx->sec_rotation;
    //LOGI("Sec Rotation %d",ctx->sec_rotation);

    sIPUOutputParam.user_def_paddr[0] = ctx->sec_disp_phys + ctx->sec_disp_next_buf*ctx->sec_frame_size;
    //LOGI("Output param: width %d,height %d, pos.x %d, pos.y %d,win_w %d,win_h %d,rot %d",
    //sIPUOutputParam.width,
    //sIPUOutputParam.height,
    //sIPUOutputParam.output_win.pos.x,
    //sIPUOutputParam.output_win.pos.y,
    //sIPUOutputParam.output_win.win_w,
    //sIPUOutputParam.output_win.win_h,
    //sIPUOutputParam.rot);
                                         
    //LOGI("Input param: width %d, height %d, fmt %d, crop_win pos x %d, crop_win pos y %d, crop_win win_w %d,crop_win win_h %d",
    //sIPUInputParam.width,
    //sIPUInputParam.height,
    //sIPUInputParam.fmt,
    //sIPUInputParam.input_crop_win.pos.x,
    //sIPUInputParam.input_crop_win.pos.y,
    //sIPUInputParam.input_crop_win.win_w,
    //sIPUInputParam.input_crop_win.win_h);     
        
    iIPURet =  mxc_ipu_lib_task_init(&sIPUInputParam,NULL,&sIPUOutputParam,OP_NORMAL_MODE|TASK_VF_MODE,&sIPUHandle);
    if (iIPURet < 0) {
        LOGE("Error!mxc_ipu_lib_task_init failed mIPURet %d!",iIPURet);
        return -1;
    }  
    //LOGI("mxc_ipu_lib_task_init success");
    iIPURet = mxc_ipu_lib_task_buf_update(&sIPUHandle,phys,sIPUOutputParam.user_def_paddr[0],NULL,NULL,NULL);
    if (iIPURet < 0) {
        LOGE("Error!mxc_ipu_lib_task_buf_update failed mIPURet %d!",iIPURet);
        mxc_ipu_lib_task_uninit(&sIPUHandle);
        return -1;
    }
    //LOGI("mxc_ipu_lib_task_buf_update success");
    mxc_ipu_lib_task_uninit(&sIPUHandle);

    return 0;
}

/** convert HAL_PIXEL_FORMAT to C2D format */
static C2D_COLORFORMAT get_format(int format) {
    switch (format) {
	case HAL_PIXEL_FORMAT_BGRA_8888:   return C2D_COLOR_8888;
    case HAL_PIXEL_FORMAT_RGB_565:     return C2D_COLOR_0565;
    default:                           return C2D_COLOR_0565;
    }
}

/** get  pixelbit from HAL_PIXEL_FORMAT format */
static int get_pixelbit(int format) {
    switch (format) {
    case HAL_PIXEL_FORMAT_BGRA_8888:   return 32;
    case HAL_PIXEL_FORMAT_RGB_565:     return 16;
    default:                           return 16;
    }
}

static int resizeToSecFrameBuffer_c2d(int base,int phys,fb_context_t* ctx)
{
    C2D_SURFACE_DEF srcSurfaceDef;
    C2D_SURFACE_DEF dstSurfaceDef;
    C2D_SURFACE srcSurface;
    C2D_SURFACE dstSurface; 
    C2D_RECT dstRect;

    if(!ctx || !ctx->c2dctx) return -1;

    srcSurfaceDef.format = get_format(ctx->device.format);
    srcSurfaceDef.width =  ctx->device.width;
    srcSurfaceDef.height = ctx->device.height;

	//make sure stride is 32 pixel aligned
    srcSurfaceDef.stride = ((ctx->device.width + 31) & ~31)*get_pixelbit(ctx->device.format)>>3;

    srcSurfaceDef.buffer = (void *)phys;
    srcSurfaceDef.host = (void *)base;
    srcSurfaceDef.flags = C2D_SURFACE_NO_BUFFER_ALLOC;

    if (c2dSurfAlloc(ctx->c2dctx, &srcSurface, &srcSurfaceDef) != C2D_STATUS_OK)
    {
        LOGE("srcSurface c2dSurfAlloc fail");
        return -EINVAL;
    }

    dstSurfaceDef.format = get_format(HAL_PIXEL_FORMAT_RGB_565);
    dstSurfaceDef.width =  ctx->sec_disp_w;
    dstSurfaceDef.height = ctx->sec_disp_h;

    //make sure stride is 32 pixel aligned
    dstSurfaceDef.stride = ((ctx->sec_disp_w + 31) & ~31)*get_pixelbit(HAL_PIXEL_FORMAT_RGB_565)>>3;

    dstSurfaceDef.buffer = (void *)ctx->sec_disp_phys + ctx->sec_disp_next_buf*ctx->sec_frame_size;
    dstSurfaceDef.host = (void *)ctx->sec_disp_base + ctx->sec_disp_next_buf*ctx->sec_frame_size;
    dstSurfaceDef.flags = C2D_SURFACE_NO_BUFFER_ALLOC;

    dstRect.x = dstRect.y = 0;
    dstRect.width = dstSurfaceDef.width;
    dstRect.height = dstSurfaceDef.height;
                
    if((ctx->mRotate == 0)||(ctx->mRotate == 180))
    {
        if(ctx->sec_disp_w >= ctx->sec_disp_h*ctx->device.width/ctx->device.height){
            dstRect.width = ctx->sec_disp_h*ctx->device.width/ctx->device.height;
        }
        else{
            dstRect.height = ctx->sec_disp_w*ctx->device.height/ctx->device.width;
        }
    }
    else{
        if(ctx->sec_disp_w >= ctx->sec_disp_h*ctx->device.height/ctx->device.width){
            dstRect.width = ctx->sec_disp_h*ctx->device.height/ctx->device.width;
        }
        else{
            dstRect.height = ctx->sec_disp_w*ctx->device.width/ctx->device.height;
        }
    }

    dstRect.x = (ctx->sec_disp_w - dstRect.width)/2;
    dstRect.y = (ctx->sec_disp_h - dstRect.height)/2;
            
    if (c2dSurfAlloc(ctx->c2dctx, &dstSurface, &dstSurfaceDef) != C2D_STATUS_OK)
    {
        LOGE("dstSurface c2dSurfAlloc fail");
        c2dSurfFree(ctx->c2dctx, srcSurface);
        return -EINVAL;
    }

    c2dSetSrcSurface(ctx->c2dctx, srcSurface);
    c2dSetDstSurface(ctx->c2dctx, dstSurface); 
    c2dSetSrcRotate(ctx->c2dctx, ctx->mRotate);

    c2dSetStretchMode(ctx->c2dctx, C2D_STRETCH_BILINEAR_SAMPLING);
    c2dSetBlendMode(ctx->c2dctx, C2D_ALPHA_BLEND_NONE);         
    c2dSetDither(ctx->c2dctx, 0); 
 
    c2dSetDstRectangle(ctx->c2dctx, &dstRect);
    c2dDrawBlit(ctx->c2dctx); 
        
    c2dFinish(ctx->c2dctx);
    c2dSurfFree(ctx->c2dctx, srcSurface);
    c2dSurfFree(ctx->c2dctx, dstSurface);

    return 0;
}

void * secDispShowFrames(void * arg)
{
    private_module_t* m = NULL;
    private_handle_t const* hnd = NULL;
    fb_context_t* ctx = (fb_context_t*)arg;
    char value[PROPERTY_VALUE_MAX];
    bool update_sec_fb = true;

    while(1)
    {
        sem_wait(&ctx->sec_display_begin);

        if(!ctx->sec_display_inited)
        {
            sem_post(&ctx->sec_display_end);
            break;
        }

        property_get("sys.VIDEO_PLAYING", value, "0");
        if (!strcmp(value, "1"))
        {
            property_get("ro.secfb.disable-overlay", value, "0");
            if (!strcmp(value, "1"))
		    update_sec_fb = false;
            if (ctx->video_play_mode == DUAL_VIDEO_SIN_UI)
		    update_sec_fb = false;

	    if (!update_sec_fb) {
		    sem_post(&ctx->sec_display_end);
		    continue;
	    }
        } else
            update_sec_fb = true;

        if (!update_sec_fb)
        {
            if(ctx->cleancount)
            {
                sem_post(&ctx->sec_display_end);
                continue;
            }

            ctx->cleancount++;
            memset((void *)ctx->sec_disp_base, 0, ctx->sec_frame_size*nr_framebuffers);
        }
        else
        {
           ctx->cleancount = 0;
        }

        if(!ctx->cleancount)
        {
            hnd = reinterpret_cast<private_handle_t const*>(ctx->buffer);
            m = reinterpret_cast<private_module_t*>(ctx->dev->common.module);

            if(ctx->c2dctx != NULL)
            {
                resizeToSecFrameBuffer_c2d(hnd->base,
                           m->framebuffer->phys + hnd->base - m->framebuffer->base,
                           ctx);
            }
            else
            {
                resizeToSecFrameBuffer(hnd->base,
                                   m->framebuffer->phys + hnd->base - m->framebuffer->base,
                                   ctx);
            }
        }

        sem_post(&ctx->sec_display_end);
        
        ctx->sec_info.yoffset = (ctx->sec_info.yres_virtual/nr_framebuffers) * ctx->sec_disp_next_buf;
        ctx->sec_disp_next_buf = (ctx->sec_disp_next_buf + 1) % nr_framebuffers;
        ctx->sec_info.activate = FB_ACTIVATE_VBL;

        ioctl(ctx->sec_fp, FBIOPAN_DISPLAY, &ctx->sec_info);
    }

    return NULL;
}

#endif

/*****************************************************************************/

static int fb_close(struct hw_device_t *dev)
{
    fb_context_t* ctx = (fb_context_t*)dev;
    if (ctx) {
        free(ctx);
    }
    return 0;
}

int fb_device_open(hw_module_t const* module, const char* name,
        hw_device_t** device)
{
    int status = -EINVAL;
    char value[PROPERTY_VALUE_MAX];

    if (!strcmp(name, GRALLOC_HARDWARE_FB0)) {
        alloc_device_t* gralloc_device;
        framebuffer_device_t *fbdev;

        nr_framebuffers = NUM_BUFFERS;
        property_get("ro.product.device", value, "");
        if (0 == strcmp(value, "imx50_rdp")) {
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
        dev->device.setUpdateRect = fb_setUpdateRect;
        #endif
        dev->device.compositionComplete = fb_compositionComplete;
        #ifdef SECOND_DISPLAY_SUPPORT
        dev->device.setSecRotation = fb_setSecRotation;
        #endif

        private_module_t* m = (private_module_t*)module;
        status = mapFrameBuffer(m);
        if (status >= 0) {
            int stride = m->finfo.line_length / (m->info.bits_per_pixel >> 3);
            const_cast<uint32_t&>(dev->device.flags) = 0xfb0;
            const_cast<uint32_t&>(dev->device.width) = m->info.xres;
            const_cast<uint32_t&>(dev->device.height) = m->info.yres;
            const_cast<int&>(dev->device.stride) = stride;
            if(m->info.bits_per_pixel != 32) {
                const_cast<int&>(dev->device.format) = HAL_PIXEL_FORMAT_RGB_565;
            }
            else{
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
