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

/*Copyright 2009-2012 Freescale Semiconductor, Inc. All Rights Reserved.*/

#ifndef _HWC_FSL_H_
#define _HWC_FSL_H_

#include <hardware/hardware.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <hardware/hwcomposer.h>

#include <utils/threads.h>
#include <EGL/egl.h>
#include "gralloc_priv.h"
#include <asm/page.h>
#include <ui/Rect.h>
#include <ui/Region.h>

#undef LOG_TAG
#define LOG_TAG "FslHwcomposer"
#include <utils/Log.h>

//#define HWCOMPOSER_DEBUG_LOG

#ifdef HWCOMPOSER_DEBUG_LOG
#define HWCOMPOSER_LOG_RUNTIME(format, ...) LOGI((format), ## __VA_ARGS__)
#define HWCOMPOSER_LOG_FUNC LOGI("%s is excuting...",  __FUNCTION__)
#else
#define HWCOMPOSER_LOG_RUNTIME(format, ...)
#define HWCOMPOSER_LOG_FUNC
#endif

#define HWCOMPOSER_LOG_TRACE   LOGI("%s : %d", __FUNCTION__,__LINE__)
#define HWCOMPOSER_LOG_INFO(format, ...) LOGI((format), ## __VA_ARGS__)

#define HWCOMPOSER_LOG_ERR(format, ...) LOGE((format), ##__VA_ARGS__)
/*****************************************************************************/
#define DEFAULT_FB_DEV_NAME "/dev/graphics/fb0"
#define FB1_DEV_NAME "/dev/graphics/fb1"
#define FB2_DEV_NAME "/dev/graphics/fb2"
#define FB3_DEV_NAME "/dev/graphics/fb3"
#define V4L_DEV_NAME "/dev/video16"
#define MAX_OUTPUT_DISPLAY  10

#define BLIT_IPU "blt_ipu"
#define BLIT_GPU "blt_gpu"

#define DEFAULT_BUFFERS  3 

using namespace android;

//typedef unsigned long __u32;
#define fourcc(a, b, c, d)\
	 (((__u32)(a)<<0)|((__u32)(b)<<8)|((__u32)(c)<<16)|((__u32)(d)<<24))

#define OUT_PIX_FMT_RGB565  fourcc('R', 'G', 'B', 'P')	/*!< 1 6  RGB-5-6-5   */
#define OUT_PIX_FMT_BGR24   fourcc('B', 'G', 'R', '3')	/*!< 24  BGR-8-8-8    */
#define OUT_PIX_FMT_RGB24   fourcc('R', 'G', 'B', '3')	/*!< 24  RGB-8-8-8    */
#define OUT_PIX_FMT_BGR32   fourcc('B', 'G', 'R', '4')	/*!< 32  BGR-8-8-8-8  */
#define OUT_PIX_FMT_BGRA32  fourcc('B', 'G', 'R', 'A')	/*!< 32  BGR-8-8-8-8  */
#define OUT_PIX_FMT_RGB32   fourcc('R', 'G', 'B', '4')	/*!< 32  RGB-8-8-8-8  */
#define OUT_PIX_FMT_RGBA32  fourcc('R', 'G', 'B', 'A')	/*!< 32  RGB-8-8-8-8  */
#define OUT_PIX_FMT_ABGR32  fourcc('A', 'B', 'G', 'R')	/*!< 32  ABGR-8-8-8-8 */

#define OUT_PIX_FMT_YUYV    fourcc('Y', 'U', 'Y', 'V')	/*!< 16 YUV 4:2:2 */
#define OUT_PIX_FMT_UYVY    fourcc('U', 'Y', 'V', 'Y')	/*!< 16 YUV 4:2:2 */
#define OUT_PIX_FMT_YUV422P fourcc('4', '2', '2', 'P')	/*!< 16 YUV 4:2:2 */
#define OUT_PIX_FMT_YVU422P fourcc('Y', 'V', '1', '6')	/*!< 16 YVU 4:2:2 */
#define OUT_PIX_FMT_YUV444  fourcc('Y', '4', '4', '4')	/*!< 24 YUV 4:4:4 */
#define OUT_PIX_FMT_YUV420P fourcc('I', '4', '2', '0')	/*!< 12 YUV 4:2:0 */
#define OUT_PIX_FMT_YVU420P fourcc('Y', 'V', '1', '2')	/*!< 12 YVU 4:2:0 */
#define OUT_PIX_FMT_YUV420P2 fourcc('Y', 'U', '1', '2')	/*!< 12 YUV 4:2:0 */
#define OUT_PIX_FMT_NV12    fourcc('N', 'V', '1', '2') /* 12  Y/CbCr 4:2:0  */
#define OUT_PIX_FMT_YUV420  fourcc('Y', 'U', '1', '2') /* 12  YUV 4:2:0     */
#define OUT_PIX_FMT_YVU420  fourcc('Y', 'V', '1', '2') /* 12  YVU 4:2:0     */

inline size_t roundUpToPageSize(size_t x) {
    return (x + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);
}

typedef enum {
    DISPLAY_MODE_OVERLAY_DISP0 = 0x00000001,
    DISPLAY_MODE_OVERLAY_DISP1 = 0x00000002,
    DISPLAY_MODE_OVERLAY_DISP2 = 0x00000004,
    DISPLAY_MODE_OVERLAY_DISP3 = 0x00000008,
    DISPLAY_MODE_DISP1 = 0x00000010,
    DISPLAY_MODE_DISP2 = 0x00000020,
    DISPLAY_MODE_DISP3 = 0x00000040,
}DISPLAY_MODE;

//seperate into three groups. one group member can be or with other group member.
//but the group member can not be or with that in the same group except display group.
#define GRALLOC_USAGE_OVERLAY0_MASK   0x00300000
#define GRALLOC_USAGE_OVERLAY1_MASK   0x00C00000
#define GRALLOC_USAGE_DISPLAY_MASK    0x07000000
#define GRALLOC_USAGE_OVERLAY_DISPLAY_MASK 0x07F00000

#define LAYER_RECORD_NUM      8
typedef struct {
    void* handle;
    hwc_rect_t outRect;
    int outDev;
}layer_record;

typedef struct{
    void *virt_addr;
    unsigned long phy_addr;
    unsigned long size;
    int format;
    int width;
    int height;
    int usage;
    Region disp_region;
}hwc_buffer;

class output_device
{
public:
		virtual int post(hwc_buffer *);
		virtual int fetch(hwc_buffer *);

		void setUsage(int usage);
		int getUsage();
		int getWidth();
		int getHeight();
        void setDisplayFrame(hwc_rect_t *disFrame);
        int needFillBlack(hwc_buffer *buf);
        void fillBlack(hwc_buffer *buf);

		output_device(const char *dev_name, int usage);
		virtual ~output_device();

		static int isFGDevice(const char *dev_name);

private:
		output_device& operator = (output_device& out);
		output_device(const output_device& out);

protected:
		int m_dev;
		//int m_left;
		//int m_top;
		int m_usage;
		int m_width;
		int m_height;
		int m_format;
//		int is_overlay;

        //Region orignRegion;
        Region currenRegion;

		mutable Mutex mLock;
		hwc_buffer mbuffers[DEFAULT_BUFFERS];
		unsigned long mbuffer_count;
		unsigned long mbuffer_cur;

};

//the normal display device
class BG_device : public output_device
{
public:
//		virtual int post(hwc_buffer *);
//		virtual int fetch(hwc_buffer *);

		BG_device(const char *dev_name, int usage);
		virtual ~BG_device();

private:
		BG_device& operator = (BG_device& out);
		BG_device(const BG_device& out);

		int init();
		int uninit();

public:
		//add private data
};

//the overlay display device
class FG_device : public output_device
{
public:
//		virtual int post(hwc_buffer *);
//		virtual int fetch(hwc_buffer *);

		FG_device(const char *dev_name, int usage);
		virtual ~FG_device();

private:
		FG_device& operator = (FG_device& out);
		FG_device(const FG_device& out);

		int init();
		int uninit();

private:
		//add private data
		//int m_flag; //for display number flag.
};

class blit_device{
public:
		static int isIPUDevice(const char *dev_name);
		static int isGPUDevice(const char *dev_name);
    		virtual int blit(hwc_layer_t *layer, hwc_buffer *out_buf) = 0;
		blit_device();
		virtual ~blit_device(){}

                int m_def_disp_w;
                int m_def_disp_h;
};

//int FG_init(struct output_device *dev);
//int FG_uninit(struct output_device *dev);
//int FG_fetch(struct output_device *dev, hwc_buffer *buf);
//int FG_post(struct output_device *dev);
//
//int BG_init(struct output_device *dev);
//int BG_uninit(struct output_device *dev);
//int BG_fetch(struct output_device *dev, hwc_buffer *buf);
//int BG_post(struct output_device *dev);
unsigned long fmt_to_bpp(unsigned long pixelformat);
int hwc_fill_frame_back(char * frame,int frame_size, int xres,
                           int yres, unsigned int pixelformat);
int blit_dev_open(const char *dev_name, blit_device **);
int blit_dev_close(blit_device *);

int output_dev_open(const char *dev_name, output_device **, int);
int output_dev_close(output_device *);

#endif
