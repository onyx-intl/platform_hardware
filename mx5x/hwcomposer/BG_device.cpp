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

/* Copyright 2009-2011 Freescale Semiconductor, Inc. All Rights Reserved.*/

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <hardware/hardware.h>
//#include <hardware/overlay.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <hardware/hwcomposer.h>

#include <EGL/egl.h>
#include "gralloc_priv.h"
#include "hwc_common.h"
/*****************************************************************************/
using namespace android;

//note: the fb1 in mx5x is hdmi port and should use 1080p=1920_1080. 
BG_device::BG_device(const char *dev_name, int usage) 
			: output_device(dev_name, usage)
{
		init();
}

BG_device::~BG_device()
{
		uninit();
}

int BG_device::init()
{
	  int status = -EINVAL;
	  int fbSize = 0;
	  void *vaddr = NULL;

    if(m_dev <= 0) {
    	  HWCOMPOSER_LOG_ERR("Error! BG_init invalid parameter!");
    	  return -1;       	
    }
    
    struct fb_var_screeninfo info;
    if(ioctl(m_dev, FBIOGET_VSCREENINFO, &info) < 0) {
    	  HWCOMPOSER_LOG_ERR("Error! BG_device::init VSCREENINFO getting failed!");
    	  return -1;    	  
    }
    
    struct fb_fix_screeninfo finfo;
    if(ioctl(m_dev, FBIOGET_FSCREENINFO, &finfo) < 0) {
    	  HWCOMPOSER_LOG_ERR("Error! BG_device::init FSCREENINFO getting failed!");
    	  return -1;       	
    }
    
   // m_left = 0;
   // m_top = 0;
    m_width = info.xres;
    m_height = info.yres;
    m_format = fourcc('R', 'G', 'B', 'P');//('U', 'Y', 'V', 'Y');
  	
  	info.reserved[0] = 0;
  	info.reserved[1] = 0;
  	info.reserved[2] = 0;  
  	info.xoffset = 0;
  	info.yoffset = 0;
  	info.activate = FB_ACTIVATE_NOW;
  	
  	info.bits_per_pixel = fmt_to_bpp(m_format);
  	info.nonstd = m_format;
  	info.red.offset = 0;
  	info.red.length = 0;
  	info.green.offset = 0;
  	info.green.length = 0;
  	info.blue.offset = 0;
  	info.blue.length = 0;
  	info.transp.offset = 0;
  	info.transp.length = 0;	 
  	
  	info.xres = m_width;
  	info.yres = m_height;
  	info.yres_virtual = ALIGN_PIXEL_128(info.yres) * DEFAULT_BUFFERS;
  	info.xres_virtual = ALIGN_PIXEL(info.xres);
  	
    if(ioctl(m_dev, FBIOPUT_VSCREENINFO, &info) < 0) {
    	  HWCOMPOSER_LOG_ERR("Error! BG_device::init-2 VSCREENINFO setting failed!");
    	  return -1;    	  
    }

    if(ioctl(m_dev, FBIOGET_VSCREENINFO, &info) < 0) {
    	  HWCOMPOSER_LOG_ERR("Error! BG_device::init-2 VSCREENINFO getting failed!");
    	  return -1;    	  
    }

    if(ioctl(m_dev, FBIOGET_FSCREENINFO, &finfo) < 0) {
    	  HWCOMPOSER_LOG_ERR("Error! BG_device::init-2 FSCREENINFO getting failed!");
    	  return -1;       	
    }
  	
  	if(finfo.smem_len <= 0) {
    	  HWCOMPOSER_LOG_ERR("Error! BG_device::init finfo.smem_len < 0!");
    	  return -1;      		
  	}
  	
  	fbSize = roundUpToPageSize(finfo.line_length * info.yres_virtual);
  	vaddr = mmap(0, fbSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_dev, 0);
  	if(vaddr == MAP_FAILED) {
    	  HWCOMPOSER_LOG_ERR("Error! BG_device::init mapping the framebuffer error(%s)!", strerror(errno));
    	  return -1;    		
  	}

        hwc_fill_frame_back((char *)vaddr, fbSize, m_width, m_height, m_format); 
        int blank = FB_BLANK_UNBLANK;
	if(ioctl(m_dev, FBIOBLANK, blank) < 0) {
		HWCOMPOSER_LOG_ERR("Error!BG_device::init UNBLANK FB1 failed!\n");
        return -1;
	} 	
//  	key.enable = 1;
//  	key.color_key = 0x00000000; //black
//  	if(ioctl(m_dev, MXCFB_SET_CLR_KEY, &key) < 0) {
//    	  HWCOMPOSER_LOG_ERR("Error! MXCFB_SET_CLR_KEY failed!");
//    	  return -1;    		
//  	}
//  	
//  	gbl_alpha.alpha = 255;
//  	gbl_alpha.enable = 1;
//  	if(ioctl(m_dev, MXCFB_SET_GBL_ALPHA, &key) < 0) {
//    	  HWCOMPOSER_LOG_ERR("Error! MXCFB_SET_CLR_KEY failed!");
//    	  return -1;    		
//  	}  	
  	
  	mbuffer_count = DEFAULT_BUFFERS;
  	mbuffer_cur = 0;
  	for(int i = 0; i < DEFAULT_BUFFERS; i++){
  			(mbuffers[i]).size = fbSize/DEFAULT_BUFFERS;
  			(mbuffers[i]).virt_addr = (void *)((unsigned long)vaddr + i * (mbuffers[i]).size);
  			(mbuffers[i]).phy_addr = finfo.smem_start + i * (mbuffers[i]).size;
  			(mbuffers[i]).format = m_format;
  	}
	
    status = 0;
    return status;
}

int BG_device::uninit()
{
	  //int status = -EINVAL;    
    int blank = 1;
    HWCOMPOSER_LOG_RUNTIME("---------------BG_device::uninit()------------");

    if(ioctl(m_dev, FBIOBLANK, blank) < 0) {
	    HWCOMPOSER_LOG_ERR("Error!BG_device::uninit BLANK FB2 failed!\n");
        //return -1;
    }	  
    munmap((mbuffers[0]).virt_addr, (mbuffers[0]).size * DEFAULT_BUFFERS);
    close(m_dev);

    return 0;
}
