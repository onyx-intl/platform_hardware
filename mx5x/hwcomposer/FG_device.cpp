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

#include <hardware/hardware.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <linux/fb.h>
#include <linux/mxcfb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <hardware/hwcomposer.h>

#include <EGL/egl.h>
#include "gralloc_priv.h"
#include "hwc_common.h"
/*****************************************************************************/
using namespace android;

FG_device::FG_device(const char *dev_name, int usage)
			: output_device(dev_name, usage)
{
		init();
}

FG_device::~FG_device()
{
		uninit();
}

static int switch_set(int fd0, int fd1, int flag)
{
    struct mxcfb_gbl_alpha gbl_alpha;
    struct mxcfb_color_key key;
  	if(flag & GRALLOC_USAGE_HWC_OVERLAY_DISP0) {
		  	key.enable = 1;
		  	key.color_key = 0x00000000; //black
		  	if(ioctl(fd0, MXCFB_SET_CLR_KEY, &key) < 0) {
		    	  HWCOMPOSER_LOG_ERR("Error! MXCFB_SET_CLR_KEY failed!");
		    	  return -1;
		  	}

		  	gbl_alpha.alpha = 128;
		  	gbl_alpha.enable = 1;
		  	if(ioctl(fd0, MXCFB_SET_GBL_ALPHA, &key) < 0) {
		    	  HWCOMPOSER_LOG_ERR("Error! MXCFB_SET_CLR_KEY failed!");
		    	  return -1;
		  	}
  	}

  	if(flag & GRALLOC_USAGE_HWC_OVERLAY_DISP1) {
		  	key.enable = 1;
		  	key.color_key = 0x00000000; //black
		  	if(ioctl(fd1, MXCFB_SET_CLR_KEY, &key) < 0) {
		    	  HWCOMPOSER_LOG_ERR("Error! MXCFB_SET_CLR_KEY failed!");
		    	  return -1;
		  	}

		  	gbl_alpha.alpha = 255;
		  	gbl_alpha.enable = 1;
		  	if(ioctl(fd1, MXCFB_SET_GBL_ALPHA, &key) < 0) {
		    	  HWCOMPOSER_LOG_ERR("Error! MXCFB_SET_CLR_KEY failed!");
		    	  return -1;
		  	}
  	}

		return 0;
}

static int overlay_switch(int fd0, int fd1, int fd2, int flag)
{
		int blank = 1;
  	int fp_property;
  	char overlayStr[32];
  	// it may be modified in mx6x.

		if(ioctl(fd2, FBIOBLANK, blank) < 0) {
				HWCOMPOSER_LOG_ERR("Error!BLANK FB0 failed!\n");
        return -1;
		}

		if(ioctl(fd1, FBIOBLANK, blank) < 0) {
				HWCOMPOSER_LOG_ERR("Error!BLANK FB1 failed!\n");
	      return -1;
		}

	  if(ioctl(fd0, FBIOBLANK, blank) < 0) {
				HWCOMPOSER_LOG_ERR("Error!BLANK FB0 failed!\n");
	      return -1;
		}

  	if(flag & GRALLOC_USAGE_HWC_OVERLAY_DISP1) {
  			//fp_property;

		    HWCOMPOSER_LOG_ERR("Open fb0/fsl_disp_property");
		    fp_property = open("/sys/class/graphics/fb0/fsl_disp_property",O_RDWR, 0);
		    if(fp_property < 0) {
		         HWCOMPOSER_LOG_ERR("Error!Cannot switch the overlay to second disp");
		         return -1;
		    }

		    memset(overlayStr, 0 ,32);
		    strcpy(overlayStr, "1-layer-fb\n");
		    HWCOMPOSER_LOG_ERR("WRITE 1-layer-fb to fb0/fsl_disp_property");
		    write(fp_property, overlayStr, strlen(overlayStr)+1);
		    close(fp_property);

  	}
  	if(flag & GRALLOC_USAGE_HWC_OVERLAY_DISP0) {
		    HWCOMPOSER_LOG_ERR("Open fb1/fsl_disp_property");
		    fp_property = open("/sys/class/graphics/fb1/fsl_disp_property",O_RDWR, 0);
		    if(fp_property < 0) {
		         HWCOMPOSER_LOG_ERR("Error!Cannot switch the overlay to second disp");
		         return -1;
		    }

		    memset(overlayStr, 0 ,32);
		    strcpy(overlayStr, "1-layer-fb\n");
		    HWCOMPOSER_LOG_ERR("WRITE 1-layer-fb to fb1/fsl_disp_property");
		    write(fp_property, overlayStr, strlen(overlayStr)+1);
		    close(fp_property);
  	}

    blank = FB_BLANK_UNBLANK;
		if(ioctl(fd1, FBIOBLANK, blank) < 0) {
				HWCOMPOSER_LOG_ERR("Error!UNBLANK FB1 failed!\n");
	      return -1;
		}

		if(ioctl(fd0, FBIOBLANK, blank) < 0) {
				HWCOMPOSER_LOG_ERR("Error!UNBLANK FB0 failed!\n");
	      return -1;
		}

		return 0;
}

int FG_device::init()
{
    int status = -EINVAL;
    int fbSize = 0;
    void *vaddr = NULL;
    HWCOMPOSER_LOG_RUNTIME("---------------FG_device::init()------------");
    if(m_dev <= 0) {
        HWCOMPOSER_LOG_ERR("Error! FG_device::FG_init() invalid parameter!");
        return -1;
    }
#if 1
    //fist open fb0 device that it is binded to.
    //it may be modified in mx6x
    int fd_def = -1;
    if(m_usage & GRALLOC_USAGE_HWC_OVERLAY_DISP0) {
            HWCOMPOSER_LOG_RUNTIME("-------------FG_device::init()---open fb0-------------");
	    fd_def = open(DEFAULT_FB_DEV_NAME, O_RDWR | O_NONBLOCK, 0);
	    if(fd_def < 0) {
	    	  HWCOMPOSER_LOG_ERR("Error! Open fb device %s failed!", DEFAULT_FB_DEV_NAME);
	    	  return -1;
	    }
    }
    else if(m_usage & GRALLOC_USAGE_HWC_OVERLAY_DISP2) {
            HWCOMPOSER_LOG_RUNTIME("-------------FG_device::init()---open fb2-------------");
	    fd_def = open(FB2_DEV_NAME, O_RDWR | O_NONBLOCK, 0);
	    if(fd_def < 0) {
	    	  HWCOMPOSER_LOG_ERR("Error! Open fb device %s failed!", FB2_DEV_NAME);
	    	  return -1;
	    }
    }
    else {
         HWCOMPOSER_LOG_ERR("Error! %s does not support usage=0x%x!", __FUNCTION__, m_usage);
         return -1;
    }
  	//it may be modified in mx6x

//    status = overlay_switch(fd_def, fd_fb1, m_dev, m_usage);

    struct fb_var_screeninfo def_info;
    if(ioctl(fd_def, FBIOGET_VSCREENINFO, &def_info) < 0) {
    	  HWCOMPOSER_LOG_ERR("Error! FG_device::init VSCREENINFO def getting failed!");
    	  return -1;
    }

    struct fb_fix_screeninfo def_finfo;
    if(ioctl(fd_def, FBIOGET_FSCREENINFO, &def_finfo) < 0) {
    	  HWCOMPOSER_LOG_ERR("Error! FG_device::init FSCREENINFO def getting failed!");
    	  return -1;
    }

    struct fb_var_screeninfo info;
    if(ioctl(m_dev, FBIOGET_VSCREENINFO, &info) < 0) {
    	  HWCOMPOSER_LOG_ERR("Error! FG_device::init VSCREENINFO getting failed!");
    	  return -1;
    }

    struct fb_fix_screeninfo finfo;
    if(ioctl(m_dev, FBIOGET_FSCREENINFO, &finfo) < 0) {
    	  HWCOMPOSER_LOG_ERR("Error! FG_device::init FSCREENINFO getting failed!");
    	  return -1;
    }

    m_width = def_info.xres;//info.xres;
    m_height = def_info.yres;//info.yres;
    m_format = fourcc('U', 'Y', 'V', 'Y');

  	info.reserved[0] = def_info.reserved[0];
  	info.reserved[1] = def_info.reserved[0];
  	info.reserved[2] = def_info.reserved[0];
  	info.xoffset = 0;
  	info.yoffset = 0;
  	info.activate = FB_ACTIVATE_NOW;

  	info.bits_per_pixel = fmt_to_bpp(m_format);//def_info.bits_per_pixel;
  	info.nonstd = m_format;
  	info.red.offset = 0;//def_info.red.offset;
  	info.red.length = 0;//def_info.red.length;
  	info.green.offset = 0;//def_info.green.offset;
  	info.green.length = 0;//def_info.green.length;
  	info.blue.offset = 0;//def_info.blue.offset;
  	info.blue.length = 0;//def_info.blue.length;
  	info.transp.offset = 0;//def_info.transp.offset;
  	info.transp.length = 0;//def_info.transp.length;

  	info.xres = m_width;
  	info.yres = m_height;
  	info.yres_virtual = ALIGN_PIXEL_128(info.yres) * DEFAULT_BUFFERS;
  	info.xres_virtual = ALIGN_PIXEL(info.xres);

    if(ioctl(m_dev, FBIOPUT_VSCREENINFO, &info) < 0) {
    	  HWCOMPOSER_LOG_ERR("Error! FG_device::init-2 VSCREENINFO setting failed!");
    	  return -1;
    }

    if(ioctl(m_dev, FBIOGET_VSCREENINFO, &info) < 0) {
    	  HWCOMPOSER_LOG_ERR("Error! FG_device::init-2 VSCREENINFO getting failed!");
    	  return -1;
    }

    if(ioctl(m_dev, FBIOGET_FSCREENINFO, &finfo) < 0) {
    	  HWCOMPOSER_LOG_ERR("Error! FG_device::init-2 FSCREENINFO getting failed!");
    	  return -1;
    }

  	if(finfo.smem_len <= 0) {
    	  HWCOMPOSER_LOG_ERR("Error! FG_device::init finfo.smem_len < 0!");
    	  return -1;
  	}

  	fbSize = roundUpToPageSize(finfo.line_length * info.yres_virtual);
  	vaddr = mmap(0, fbSize, PROT_READ | PROT_WRITE, MAP_SHARED, m_dev, 0);
  	if(vaddr == MAP_FAILED) {
    	  HWCOMPOSER_LOG_ERR("Error! FG_device::init mapping the framebuffer error(%s)!", strerror(errno));
    	  return -1;
  	}
    //memset(vaddr, 0, fbSize);
    hwc_fill_frame_back((char *)vaddr, fbSize, m_width, m_height, m_format);
    int blank = FB_BLANK_UNBLANK;
	if(ioctl(m_dev, FBIOBLANK, blank) < 0) {
		HWCOMPOSER_LOG_ERR("Error!FG_device::init UNBLANK FB1 failed!\n");
        return -1;
	}
  	//do it after switch fb2 to fb1 or fb0
  	//status = switch_set(fd_def, fd_fb1, m_usage);
  	close(fd_def);

  	mbuffer_count = DEFAULT_BUFFERS;
  	mbuffer_cur = 0;
  	for(int i = 0; i < DEFAULT_BUFFERS; i++){
		(mbuffers[i]).size = fbSize/DEFAULT_BUFFERS;
		(mbuffers[i]).virt_addr = (void *)((unsigned long)vaddr + i * (mbuffers[i]).size);
		(mbuffers[i]).phy_addr = finfo.smem_start + i * (mbuffers[i]).size;
		(mbuffers[i]).width = m_width;
        (mbuffers[i]).height = m_height;
        (mbuffers[i]).format = m_format;
  	}

  	//pthread_mutex_init(&dev->buf_mutex, NULL);
#endif
    status = 0;
    return status;
}

int FG_device::uninit()
{
	  //int status = -EINVAL;
    int blank = 1;
    HWCOMPOSER_LOG_RUNTIME("---------------FG_device::uninit()------------");

    if(ioctl(m_dev, FBIOBLANK, blank) < 0) {
		HWCOMPOSER_LOG_ERR("Error!FG_device::uninit BLANK FB2 failed!\n");
        //return -1;
	}
	munmap((mbuffers[0]).virt_addr, (mbuffers[0]).size * DEFAULT_BUFFERS);
    close(m_dev);
    return 0;
}

