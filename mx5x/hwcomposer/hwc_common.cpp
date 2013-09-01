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
#include "blit_ipu.h"
#include <linux/ipu.h>
//extern "C" {
//#include "mxc_ipu_hl_lib.h" 
//}
/*****************************************************************************/
using namespace android;
//int hwc_check_property(hwc_context_t *dev)
//{
//    bool bValue = false;
//    char value[10];
//    property_get("rw.VIDEO_TVOUT_DISPLAY", value, "");
//    if (strcmp(value, "1") == 0)
//        bValue = true;
//
//    if((dev->display_mode == DISPLAY_MODE_TV)  !=  bValue){
//        dev->display_mode = bValue ? DISPLAY_MODE_TV : DISPLAY_MODE_NORMAL;
//        switchTvOut(dev);
//        *mode_changed = true;
//        return 0;
//    }
//
//    bValue = false;
//    property_get("sys.SECOND_DISPLAY_ENABLED", value, "");
//    if (strcmp(value, "1") == 0)
//        bValue = true;
//
//    if((dev->display_mode == DISPLAY_MODE_DUAL_DISP)  !=  bValue){
//        dev->display_mode = bValue ? DISPLAY_MODE_DUAL_DISP : DISPLAY_MODE_NORMAL;
//        switchDualDisp(dev);
//        *mode_changed = true;
//    }    	
//		return 0;
//}


unsigned long fmt_to_bpp(unsigned long pixelformat)
{
	unsigned long bpp;

	switch (pixelformat)
	{
		case OUT_PIX_FMT_RGB565:
		/*interleaved 422*/
		case OUT_PIX_FMT_YUYV:
		case OUT_PIX_FMT_UYVY:
		/*non-interleaved 422*/
		case OUT_PIX_FMT_YUV422P:
		case OUT_PIX_FMT_YVU422P:
			bpp = 16;
			break;
		case OUT_PIX_FMT_BGR24:
		case OUT_PIX_FMT_RGB24:
		case OUT_PIX_FMT_YUV444:
			bpp = 24;
			break;
		case OUT_PIX_FMT_BGR32:
		case OUT_PIX_FMT_BGRA32:
		case OUT_PIX_FMT_RGB32:
		case OUT_PIX_FMT_RGBA32:
		case OUT_PIX_FMT_ABGR32:
			bpp = 32;
			break;
		/*non-interleaved 420*/
		case OUT_PIX_FMT_YUV420P:
		case OUT_PIX_FMT_YVU420P:
		case OUT_PIX_FMT_YUV420P2:
		case OUT_PIX_FMT_NV12:
			bpp = 12;
			break;
		default:
			bpp = 8;
			break;
	}
	return bpp;
}

int hwc_fill_frame_back(char * frame,int frame_size, int xres,
                           int yres, unsigned int pixelformat)
{
    int ret = 0;
    char * base;
    int j, screen_size;
    short * tmp;
    short color;
    if((xres<=0)||(yres<=0)||(!frame)) {
        HWCOMPOSER_LOG_ERR("Error!Not valid parameters in fill_frame_back");
        return -1;
    }
    switch(pixelformat) {
        case OUT_PIX_FMT_RGB565:
            memset(frame, 0, frame_size);
            break;
        case OUT_PIX_FMT_YUYV:
        case OUT_PIX_FMT_UYVY:
            tmp = (short *) frame;
            if(pixelformat == OUT_PIX_FMT_YUYV)
               color = 0x8000;
            else
               color = 0x80;
            for(int i = 0; i < frame_size/2;i++, tmp++)
                *tmp = color;
            break;
        case OUT_PIX_FMT_YUV422P:
            base = (char *)frame;
            screen_size = xres * yres;
            memset(base, 0, frame_size);
            base += screen_size;
            for (int i = 0; i < screen_size; i++, base++)
                *base = 0x80;
            break;
        case OUT_PIX_FMT_YUV420:
        case OUT_PIX_FMT_YVU420:
        case OUT_PIX_FMT_NV12:
            base = (char *)frame;
            screen_size = xres * yres;
            memset(base, 0, frame_size);
            base += screen_size;
            for (int i = 0; i < screen_size/2; i++, base++)
                 *base = 0x80;
            break;
        defaule:
            HWCOMPOSER_LOG_ERR("Error!Not supported pixel format");
            ret = -1;
            break;
    }
    return ret;
}

int blit_dev_open(const char *dev_name, blit_device **device)
{
	  int status = -EINVAL;
	  
	  int isIPU = blit_device::isIPUDevice(dev_name);	  
	  if(isIPU) {
	  	  blit_ipu *dev;
	  	  dev = new blit_ipu();
	  	  if(dev == NULL)
	  	      return status;
	  	  
	  	  *device = (blit_device *)dev;
	  	  return 0;
	  }
	  
	  int isGPU = blit_device::isGPUDevice(dev_name);
	  if(isGPU) {
	  	  blit_gpu *dev;
	  	  dev = new blit_gpu();
	  	  if(dev == NULL)
	  	      return status;
	  	      	  	  
	  	  *device = (blit_device *)dev;
	  	  return 0;	  	  	  
	  }	  
	  
	  return status;
}

int blit_dev_close(blit_device *dev)
{
		delete(dev);
		return 0;
}

int output_dev_open(const char *dev_name, output_device **device, int flag)
{
   	int is_overlay = output_device::isFGDevice(dev_name);
HWCOMPOSER_LOG_INFO("!!!!!!!!!!!!!!!!!!!!!!!!!output_dev_open: %s", dev_name);   	
   	if(is_overlay < 0) {
   			return HWC_EGL_ERROR;
   	}
   	
    if(is_overlay == 1) {
HWCOMPOSER_LOG_RUNTIME("******output_dev_open() is_overlay =1");    	
			  FG_device *dev;
			  dev = new FG_device(dev_name, flag);
			  	  if(dev == NULL)
			  	      return HWC_EGL_ERROR;
			
			  //dev->setUsage(flag);					   	
    		*device = (output_device *)dev;
    }
		else {
			  BG_device *dev;
			  dev = new BG_device(dev_name, flag);
			  	  if(dev == NULL)
			  	      return HWC_EGL_ERROR;
			
			  //dev->setUsage(flag);	  	
    		*device = (output_device *)dev;
		}
       
    return 0;
}

int output_dev_close(output_device *dev)
{
  	delete(dev); 
  	
  	return 0;
}

blit_device::blit_device()
{
        int fd_def;

        m_def_disp_w = 0;
        m_def_disp_h = 0;

        fd_def = open(DEFAULT_FB_DEV_NAME, O_RDWR | O_NONBLOCK, 0);

        if(fd_def < 0) {
          HWCOMPOSER_LOG_ERR("Error! Open fb device %s failed!", DEFAULT_FB_DEV_NAME);
          return;
        }

        struct fb_var_screeninfo def_info;
        if(ioctl(fd_def, FBIOGET_VSCREENINFO, &def_info) < 0) {
          HWCOMPOSER_LOG_ERR("Error! FG_device::init VSCREENINFO def getting failed!");
          return;
        }

        m_def_disp_w = def_info.xres;
        m_def_disp_h = def_info.yres;

        close(fd_def);

        return;
}

