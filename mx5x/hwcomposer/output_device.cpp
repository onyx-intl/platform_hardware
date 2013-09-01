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

#include <hardware/hwcomposer.h>

#include <EGL/egl.h>
#include "gralloc_priv.h"
#include "hwc_common.h"

void output_device::setUsage(int usage)
{
    m_usage = usage;
}

int output_device::getUsage()
{
    return m_usage;
}

int output_device::getWidth()
{
    return m_width;
}

int output_device::getHeight()
{
    return m_height;
}

output_device::output_device(const char *dev_name, int usage)
{
    m_dev = open(dev_name, O_RDWR | O_NONBLOCK, 0);
    if(m_dev < 0) {
        HWCOMPOSER_LOG_ERR("Error! output_device Open fb device %s failed!", dev_name);
    }
    m_usage = usage;
}

output_device::~output_device()
{
	if(m_dev > 0) {
        close(m_dev);
	}
}

int output_device::isFGDevice(const char *dev_name)
{
    int status = -EINVAL;
    int fd = -1;
    char fb_usage[32];
    char fb_name[32];
    int fd_n = 0;
    int size = 0;
    int is_overlay = 0;
    char *psname;

    memset(fb_name, 0, sizeof(fb_name));
    psname = (char *)dev_name;
    psname += (strlen(dev_name) - 1);
    strcpy(fb_name, "/sys/class/graphics/fb");
    strcat(fb_name, psname);
    strcat(fb_name, "/name");
    fd_n = open(fb_name, O_RDONLY, 0);
    //fd_n = open("/sys/class/graphics/fb0/name", O_RDONLY, 0);
    if(fd_n < 0) {
		HWCOMPOSER_LOG_ERR("Error! output_device::isFGDevice  open %s failed!", fb_name);
		return -1;
    }
    memset(fb_usage, 0, sizeof(fb_usage));
    size = read(fd_n, fb_usage, sizeof(fb_usage));
    if(size < 0) {
		HWCOMPOSER_LOG_ERR("Error! output_device::isFGDevice read /sys/class/graphics/fb0/name failed!");
		return -1;
    }
    close(fd_n);
HWCOMPOSER_LOG_INFO("output_device::isFGDevice===%s, %s, %s", dev_name, fb_name, fb_usage);
    if(strstr(fb_usage, "FG"))
    	return 1;
 	return 0;
}

void output_device::setDisplayFrame(hwc_rect_t *disFrame)
{
    if(disFrame == NULL) {
        HWCOMPOSER_LOG_ERR("Error! output_device::setDisplayFrame invalid parameter!");
    }
    Rect disRect(disFrame->left, disFrame->top, disFrame->right, disFrame->bottom);
    currenRegion.orSelf(disRect);
}

int output_device::needFillBlack(hwc_buffer *buf)
{
    Rect orignBound(buf->disp_region.getBounds());
    Rect currentBound(currenRegion.getBounds());
    return currentBound != orignBound;
}

void output_device::fillBlack(hwc_buffer *buf)
{
    if(buf == NULL) {
        HWCOMPOSER_LOG_ERR("Error! output_device::fillBlack invalid parameter!");
        return;
    }

    hwc_fill_frame_back((char *)buf->virt_addr, buf->size, buf->width, buf->height, buf->format);
}

int output_device::fetch(hwc_buffer *buf)
{
	  //int status = -EINVAL;
    if(m_dev <= 0 || buf == NULL) {
        HWCOMPOSER_LOG_ERR("Error! output_device::fetch invalid parameter! usage=%x", m_usage);
        return -1;
    }

	  Mutex::Autolock _l(mLock);
	  buf->size = (mbuffers[mbuffer_cur]).size;
	  buf->virt_addr = (mbuffers[mbuffer_cur]).virt_addr;
	  buf->phy_addr = (mbuffers[mbuffer_cur]).phy_addr;
	  buf->width = m_width;
	  buf->height = m_height;
	  buf->usage = m_usage;
	  buf->format = m_format;
	  //dev->buffer_cur = (dev->buffer_cur + 1) % DEFAULT_BUFFERS;
      if((m_usage & (GRALLOC_USAGE_OVERLAY0_MASK | GRALLOC_USAGE_OVERLAY1_MASK)) && needFillBlack(&mbuffers[mbuffer_cur])) {
          fillBlack(&mbuffers[mbuffer_cur]);
          mbuffers[mbuffer_cur].disp_region = currenRegion;
      }
      //orignRegion = currenRegion;
      currenRegion.clear();

	  return 0;
}

int output_device::post(hwc_buffer *buf)
{
	  //int status = -EINVAL;
    if(m_dev <= 0) {
        HWCOMPOSER_LOG_ERR("Error! FG_device::FG_post() invalid parameter! usage=%x", m_usage);
        return -1;
    }
HWCOMPOSER_LOG_RUNTIME("#######output_device::post()############");

	Mutex::Autolock _l(mLock);
    struct fb_var_screeninfo info;
    if(ioctl(m_dev, FBIOGET_VSCREENINFO, &info) < 0) {
        HWCOMPOSER_LOG_ERR("Error! output_device::post VSCREENINFO getting failed! usage=%x", m_usage);
        return -1;
    }

    struct fb_fix_screeninfo finfo;
    if(ioctl(m_dev, FBIOGET_FSCREENINFO, &finfo) < 0) {
        HWCOMPOSER_LOG_ERR("Error! output_device::post FSCREENINFO getting failed! usage=%x", m_usage);
        return -1;
    }

    info.yoffset = ((unsigned long)buf->virt_addr - (unsigned long)(mbuffers[0]).virt_addr) / finfo.line_length;
    //info.yoffset = ((info.yres_virtual * finfo.line_length)/ DEFAULT_BUFFERS) * mbuffer_cur;
    mbuffer_cur = (mbuffer_cur + 1) % DEFAULT_BUFFERS;
    info.activate = FB_ACTIVATE_VBL;
//HWCOMPOSER_LOG_RUNTIME("#######yoffset=%d, mbuffer_cur=%d######", info.yoffset, mbuffer_cur);
    ioctl(m_dev, FBIOPAN_DISPLAY, &info);

HWCOMPOSER_LOG_RUNTIME("#######output_device::post()##end##########");
    return 0;
}
