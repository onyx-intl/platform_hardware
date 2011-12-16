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

#include <binder/IPCThreadState.h>
#include <binder/ProcessState.h>
#include <binder/IServiceManager.h>
#include <utils/Log.h>

#include <ui/DisplayInfo.h>
#include <ui/Overlay.h>
#include <surfaceflinger/Surface.h>
#include <surfaceflinger/ISurface.h>
#include <surfaceflinger/ISurfaceComposer.h>
#include <surfaceflinger/SurfaceComposerClient.h>

#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/fb.h>
#include <linux/videodev.h>

#define LOG_TAG "overlay_test"

using namespace android;
/*
       Y = R *  .299 + G *  .587 + B *  .114;
       U = R * -.169 + G * -.332 + B *  .500 + 128.;
       V = R *  .500 + G * -.419 + B * -.0813 + 128.;*/

#define red(x) (((x & 0xE0) >> 5) * 0x24)
#define green(x) (((x & 0x1C) >> 2) * 0x24)
#define blue(x) ((x & 0x3) * 0x55)
#define y(rgb) ((red(rgb)*299L + green(rgb)*587L + blue(rgb)*114L) / 1000)
#define u(rgb) ((((blue(rgb)*500L) - (red(rgb)*169L) - (green(rgb)*332L)) / 1000))
#define v(rgb) (((red(rgb)*500L - green(rgb)*419L - blue(rgb)*81L) / 1000))

#define red_rgb565 (0b11111000000000000)
#define green_rgb565 (0b00000111111000000)
#define blue_rgb565 (0b00000000000011111)

void gen_fill_pattern_rgb(char * buf, int in_width, int in_height,int h_step, int w_step)
{
	int h, w;
	static unsigned short rgb = 0;

    unsigned short  *rgb565 = (unsigned short *)buf;
	for (h = 0; h < in_height; h++) {
		unsigned short rgb_temp = rgb;

		for (w = 0; w < in_width; w++) {
			if (w % w_step == 0) {
                rgb_temp += 0x004;//inc r,g,b by one
			}
			buf[(h*in_width) + w] = rgb_temp;
		}
		if ((h > 0) && (h % h_step == 0)) {
			rgb_temp += 0x20;
		}
	}

}
void gen_fill_pattern_yuv(char * buf, int in_width, int in_height,int h_step, int w_step)
{
	int y_size = in_width * in_height;
	//int h_step = in_height / 16;
	//int w_step = in_width / 16;
	int h, w;
	uint32_t y_color = 0;
	int32_t u_color = 0;
	int32_t v_color = 0;
	uint32_t rgb = 0;
	static int32_t alpha = 0;
	static int inc_alpha = 1;

	for (h = 0; h < in_height; h++) {
		int32_t rgb_temp = rgb;

		for (w = 0; w < in_width; w++) {
			if (w % w_step == 0) {
				y_color = y(rgb_temp);
				y_color = (y_color * alpha) / 255;

				u_color = u(rgb_temp);
				u_color = (u_color * alpha) / 255;
				u_color += 128;

				v_color = v(rgb_temp);
				v_color = (v_color * alpha) / 255;
				v_color += 128;

				rgb_temp++;
				if (rgb_temp > 255)
					rgb_temp = 0;
			}
			buf[(h*in_width) + w] = y_color;
			if (!(h & 0x1) && !(w & 0x1)) {
				buf[y_size + (((h*in_width)/4) + (w/2)) ] = u_color;
				buf[y_size + y_size/4 + (((h*in_width)/4) + (w/2))] = v_color;
			}
		}
		if ((h > 0) && (h % h_step == 0)) {
			rgb += 16;
			if (rgb > 255)
				rgb = 0;
		}

	}
	if (inc_alpha) {
		alpha+=4;
		if (alpha >= 255) {
			inc_alpha = 0;
		}
	} else {
		alpha-=4;
		if (alpha <= 0) {
			inc_alpha = 1;
		}
	}
}

void gen_fill_pattern(char * buf, int in_width, int in_height,int h_step, int w_step,int32_t format)
{
    if(format == HAL_PIXEL_FORMAT_RGB_565) {
        gen_fill_pattern_rgb(buf,in_width,in_height,h_step,w_step);
    }
    else if(format == HAL_PIXEL_FORMAT_YCbCr_420_SP) {
        gen_fill_pattern_yuv(buf,in_width,in_height,h_step,w_step);
    }
}


namespace android {
class Test {
public:
    static const sp<ISurface>& getISurface(const sp<SurfaceControl>& s) {
        return s->getISurface();
    }
};
};
typedef struct {
    int w;
    int h;
    int32_t format;
    int frame_num;
    int crop_x;
    int crop_y;
    int crop_w;
    int crop_h;
    int layer;
    int pos_x;
    int pos_y;
    int win_w;
    int win_h;
    int show_num;
    int step_line;
    int performance_test;
}OVERLAY_PARAM;


void *overlay_instance_test(void *arg);
bool performance_run = 0;
int main(int argc, char** argv)
{
    int fd_fb = 0;
    struct fb_var_screeninfo fb_var;
    pthread_t thread0,thread1;
    OVERLAY_PARAM overlay_param0;
    OVERLAY_PARAM overlay_param1;
    unsigned int panel_x,panel_y;
    void *ret;
    sp<SurfaceControl>			backgroundControl;
    sp<Surface> 				background;
    sp<SurfaceComposerClient> session;
    memset(&overlay_param0, 0 ,sizeof(OVERLAY_PARAM));
    memset(&overlay_param1, 0 ,sizeof(OVERLAY_PARAM));
 
    DisplayInfo dinfo;
    session = new SurfaceComposerClient();
    status_t status = session->getDisplayInfo(0, &dinfo);
    if (status)
        return -1;

	panel_x=dinfo.w;
	panel_y=dinfo.h;


    backgroundControl = session->createSurface(getpid(), 0, panel_x, panel_y, PIXEL_FORMAT_RGB_565);
	// set front surface to top
    session->openTransaction();
    backgroundControl->setLayer(0x40000000);
    session->closeTransaction();
	//get the rendering surface
    background = backgroundControl->getSurface();
	Surface::SurfaceInfo		sinfo_background;

	background->lock(&sinfo_background);
    //Draw to white
	memset(sinfo_background.bits, 0xff, sinfo_background.w*sinfo_background.h*2);
	background->unlockAndPost();


    overlay_param0.w = 720;
    overlay_param0.h = 576;
    overlay_param0.format = HAL_PIXEL_FORMAT_YCbCr_420_SP;
    overlay_param0.frame_num = 8;
    overlay_param0.crop_x = 0;
    overlay_param0.crop_y = 0;
    overlay_param0.crop_w = 720;
    overlay_param0.crop_h = 576;
    overlay_param0.layer = 0x40000000;
    overlay_param0.pos_x = 0;
    overlay_param0.pos_y = 0;
    overlay_param0.win_w = panel_x;
    overlay_param0.win_h = panel_y;
    overlay_param0.show_num = 200;
    overlay_param0.step_line = 16;

    overlay_param1.w = 320;
    overlay_param1.h = 240;
    overlay_param1.format = HAL_PIXEL_FORMAT_YCbCr_420_SP;
    overlay_param1.frame_num = 6;
    overlay_param1.crop_x = 0;
    overlay_param1.crop_y = 0;
    overlay_param1.crop_w = 320;
    overlay_param1.crop_h = 240;
    overlay_param1.layer = 0x40000001;
    overlay_param1.pos_x = panel_x/4;
    overlay_param1.pos_y = panel_y/4;
    overlay_param1.win_w = (panel_x/4+8)/8*8;
    overlay_param1.win_h = (panel_y/4+8)/8*8;
    overlay_param1.show_num = 200;
    overlay_param1.step_line = 8;
    //pthread_attr_t attr0,attr1;



    LOGI("*******start main thread*******");
    if(argc == 1) {
        LOGI("****Run two overlay instances 720/576 & 320/240 test*****");
        LOGI("Display info: panel_x %d panel_y %d",panel_x,panel_y);
        LOGI("Overlay paramater: w %d,h %d, pos_x %d, pos_y %d, win_h %d win_w %d",
             overlay_param0.w,overlay_param0.h,
             overlay_param0.pos_x,overlay_param0.pos_y,
             overlay_param0.win_w,overlay_param0.win_h);
        LOGI("Overlay paramater: w %d,h %d, pos_x %d, pos_y %d, win_h %d win_w %d",
             overlay_param1.w,overlay_param1.h,
             overlay_param1.pos_x,overlay_param1.pos_y,
             overlay_param1.win_w,overlay_param1.win_h);

        //draw the background to black
        LOGI("Draw background to black");
        background->lock(&sinfo_background);
        memset(sinfo_background.bits, 0xff, sinfo_background.w*sinfo_background.h*2);
        char *prgb565 = (char *)sinfo_background.bits+(overlay_param0.pos_y*sinfo_background.w+overlay_param0.pos_x)*2;
        for(int height = overlay_param0.pos_y; height < (overlay_param0.pos_y+overlay_param0.win_h); height++) {
            memset(prgb565, 0, overlay_param0.win_w*2);
            prgb565 += (sinfo_background.w*2);
        }
        prgb565 = (char *)sinfo_background.bits+(overlay_param1.pos_y*sinfo_background.w+overlay_param1.pos_x)*2;
        for(int height = overlay_param1.pos_y; height < (overlay_param1.pos_y+overlay_param1.win_h); height++) {
            memset(prgb565, 0, overlay_param1.win_w*2);
            prgb565 += (sinfo_background.w*2);
        }
    	background->unlockAndPost();
        LOGI("Overlay running");
        pthread_create(&thread0, NULL, overlay_instance_test, (void *)(&overlay_param0));
        pthread_create(&thread1, NULL, overlay_instance_test, (void *)(&overlay_param1));
    
    
        pthread_join(thread0, &ret);
        pthread_join(thread1, &ret);
    }
    else if((argc >= 2)&&(argc <=3)) {
        if(!strcmp(argv[1],"-0")) {
            LOGI("****Run one overlay instances 720/576 test*****");
            //pthread_attr_init(&attr0);
            //pthread_attr_setdetachstate(&attr0, PTHREAD_CREATE_DETACHED);
            if(argc==3) {
                overlay_param0.show_num = atoi(argv[2]);
            }

            LOGI("Display info: panel_x %d panel_y %d",panel_x,panel_y);
            LOGI("Overlay paramater: w %d,h %d, pos_x %d, pos_y %d, win_h %d win_w %d",
                 overlay_param0.w,overlay_param0.h,
                 overlay_param0.pos_x,overlay_param0.pos_y,
                 overlay_param0.win_w,overlay_param0.win_h);

            //draw the background to black
            LOGI("Draw background to black");
            background->lock(&sinfo_background);
            memset(sinfo_background.bits, 0xff, sinfo_background.w*sinfo_background.h*2);
            char *prgb565 = (char *)sinfo_background.bits+(overlay_param0.pos_y*sinfo_background.w+overlay_param0.pos_x)*2;
            for(int height = overlay_param0.pos_y; height < (overlay_param0.pos_y+overlay_param0.win_h); height++) {
                memset(prgb565, 0, overlay_param0.win_w*2);
                prgb565 += (sinfo_background.w*2);
            }
        	background->unlockAndPost();
            LOGI("Overlay running");
            pthread_create(&thread0, NULL, overlay_instance_test, (void *)(&overlay_param0));
            pthread_join(thread0, &ret);
        }
        else if(!strcmp(argv[1],"-1")){
            LOGI("****Run one overlay instances 320/240 test*****");
            //pthread_attr_init(&attr1);
            //pthread_attr_setdetachstate(&attr1, PTHREAD_CREATE_DETACHED);
            if(argc==3) {
                overlay_param1.show_num = atoi(argv[2]);
            }

            LOGI("Display info: panel_x %d panel_y %d",panel_x,panel_y);
            LOGI("Overlay paramater: w %d,h %d, pos_x %d, pos_y %d, win_h %d win_w %d",
                 overlay_param1.w,overlay_param1.h,
                 overlay_param1.pos_x,overlay_param1.pos_y,
                 overlay_param1.win_w,overlay_param1.win_h);

            //draw the background to black
            LOGI("Draw background to black");
            background->lock(&sinfo_background);
            memset(sinfo_background.bits, 0xff, sinfo_background.w*sinfo_background.h*2);
            char *prgb565 = (char *)sinfo_background.bits+(overlay_param1.pos_y*sinfo_background.w+overlay_param1.pos_x)*2;
            for(int height = overlay_param1.pos_y; height < (overlay_param1.pos_y+overlay_param1.win_h); height++) {
                memset(prgb565, 0, overlay_param1.win_w*2);
                prgb565 += (sinfo_background.w*2);
            }
        	background->unlockAndPost();
            LOGI("Overlay running");
            pthread_create(&thread1, NULL, overlay_instance_test, (void *)(&overlay_param1));
            pthread_join(thread1, &ret);
        }
        else if(!strcmp(argv[1],"-2")){
            LOGI("****Run two overlay instances 720/576 & 320/240 test*****");
            //pthread_attr_init(&attr1);
            //pthread_attr_setdetachstate(&attr1, PTHREAD_CREATE_DETACHED);
            if(argc==3) {
                overlay_param0.show_num = atoi(argv[2]);
                overlay_param1.show_num = atoi(argv[2]);
            }

            overlay_param0.pos_x = 0;
            overlay_param0.pos_y = panel_y/2;
            overlay_param0.win_w = panel_x;
            overlay_param0.win_h = (panel_y/2)/8*8;
        
            overlay_param1.pos_x = 0;
            overlay_param1.pos_y = 0;
            overlay_param1.win_w = (panel_x/4+8)/8*8;
            overlay_param1.win_h = (panel_y/4+8)/8*8;


            LOGI("Display info: panel_x %d panel_y %d",panel_x,panel_y);
            LOGI("Overlay paramater: w %d,h %d, pos_x %d, pos_y %d, win_h %d win_w %d",
                 overlay_param0.w,overlay_param0.h,
                 overlay_param0.pos_x,overlay_param0.pos_y,
                 overlay_param0.win_w,overlay_param0.win_h);
            LOGI("Overlay paramater: w %d,h %d, pos_x %d, pos_y %d, win_h %d win_w %d",
                 overlay_param1.w,overlay_param1.h,
                 overlay_param1.pos_x,overlay_param1.pos_y,
                 overlay_param1.win_w,overlay_param1.win_h);


            //draw the background to black
            LOGI("Draw background to black");
            background->lock(&sinfo_background);
            memset(sinfo_background.bits, 0xff, sinfo_background.w*sinfo_background.h*2);
            char *prgb565 = (char *)sinfo_background.bits+(overlay_param0.pos_y*sinfo_background.w+overlay_param0.pos_x)*2;
            for(int height = overlay_param0.pos_y; height < (overlay_param0.pos_y+overlay_param0.win_h); height++) {
                memset(prgb565, 0, overlay_param0.win_w*2);
                prgb565 += (sinfo_background.w*2);
            }
            prgb565 = (char *)sinfo_background.bits+(overlay_param1.pos_y*sinfo_background.w+overlay_param1.pos_x)*2;
            for(int height = overlay_param1.pos_y; height < (overlay_param1.pos_y+overlay_param1.win_h); height++) {
                memset(prgb565, 0, overlay_param1.win_w*2);
                prgb565 += (sinfo_background.w*2);
            }
        	background->unlockAndPost();

            LOGI("Overlay running");
            pthread_create(&thread0, NULL, overlay_instance_test, (void *)(&overlay_param0));
            pthread_create(&thread1, NULL, overlay_instance_test, (void *)(&overlay_param1));
        
            pthread_join(thread0, &ret);
            pthread_join(thread1, &ret);
        }
        else if(!strcmp(argv[1],"-3")){
            LOGI("****Run two overlay instances 720/576 & 320/240 performance test*****");
            if(argc==3) {
                overlay_param0.show_num = atoi(argv[2]);
                overlay_param1.show_num = atoi(argv[2]);
            }
            overlay_param0.performance_test = 1;
            overlay_param1.performance_test = 1;
            performance_run = 1;

            LOGI("Display info: panel_x %d panel_y %d",panel_x,panel_y);
            LOGI("Overlay paramater: w %d,h %d, pos_x %d, pos_y %d, win_h %d win_w %d",
                 overlay_param0.w,overlay_param0.h,
                 overlay_param0.pos_x,overlay_param0.pos_y,
                 overlay_param0.win_w,overlay_param0.win_h);
            LOGI("Overlay paramater: w %d,h %d, pos_x %d, pos_y %d, win_h %d win_w %d",
                 overlay_param1.w,overlay_param1.h,
                 overlay_param1.pos_x,overlay_param1.pos_y,
                 overlay_param1.win_w,overlay_param1.win_h);

            //draw the background to black
            LOGI("Draw background to black");
            background->lock(&sinfo_background);
            memset(sinfo_background.bits, 0xff, sinfo_background.w*sinfo_background.h*2);
            char *prgb565 = (char *)sinfo_background.bits+(overlay_param0.pos_y*sinfo_background.w+overlay_param0.pos_x)*2;
            for(int height = overlay_param0.pos_y; height < (overlay_param0.pos_y+overlay_param0.win_h); height++) {
                memset(prgb565, 0, overlay_param0.win_w*2);
                prgb565 += (sinfo_background.w*2);
            }
            prgb565 = (char *)sinfo_background.bits+(overlay_param1.pos_y*sinfo_background.w+overlay_param1.pos_x)*2;
            for(int height = overlay_param1.pos_y; height < (overlay_param1.pos_y+overlay_param1.win_h); height++) {
                memset(prgb565, 0, overlay_param1.win_w*2);
                prgb565 += (sinfo_background.w*2);
            }
        	background->unlockAndPost();

            LOGI("Overlay running");
            pthread_create(&thread0, NULL, overlay_instance_test, (void *)(&overlay_param0));
            pthread_create(&thread1, NULL, overlay_instance_test, (void *)(&overlay_param1));
        
            pthread_join(thread0, &ret);
            pthread_join(thread1, &ret);
        }
        else if(!strcmp(argv[1],"-4")){
            LOGI("****Run two overlay instances 720/576 performance test*****");
            if(argc==3) {
                overlay_param0.show_num = atoi(argv[2]);
            }
            overlay_param0.performance_test = 1;

            performance_run = 1;

            LOGI("Display info: panel_x %d panel_y %d",panel_x,panel_y);
            LOGI("Overlay paramater: w %d,h %d, pos_x %d, pos_y %d, win_h %d win_w %d",
                 overlay_param0.w,overlay_param0.h,
                 overlay_param0.pos_x,overlay_param0.pos_y,
                 overlay_param0.win_w,overlay_param0.win_h);


            //draw the background to black
            LOGI("Draw background to black");
            background->lock(&sinfo_background);
            memset(sinfo_background.bits, 0xff, sinfo_background.w*sinfo_background.h*2);
            char *prgb565 = (char *)sinfo_background.bits+(overlay_param0.pos_y*sinfo_background.w+overlay_param0.pos_x)*2;
            for(int height = overlay_param0.pos_y; height < (overlay_param0.pos_y+overlay_param0.win_h); height++) {
                memset(prgb565, 0, overlay_param0.win_w*2);
                prgb565 += (sinfo_background.w*2);
            }
        	background->unlockAndPost();

            LOGI("Overlay running");
            pthread_create(&thread0, NULL, overlay_instance_test, (void *)(&overlay_param0));
            pthread_join(thread0, &ret);
        }
        else if(!strcmp(argv[1],"-5")){
            LOGI("****Run two overlay instances 320/240 performance test*****");
            if(argc==3) {
                overlay_param1.show_num = atoi(argv[2]);
            }
            overlay_param1.performance_test = 1;
            performance_run = 1;

            LOGI("Display info: panel_x %d panel_y %d",panel_x,panel_y);
            LOGI("Overlay paramater: w %d,h %d, pos_x %d, pos_y %d, win_h %d win_w %d",
                 overlay_param1.w,overlay_param1.h,
                 overlay_param1.pos_x,overlay_param1.pos_y,
                 overlay_param1.win_w,overlay_param1.win_h);

            //draw the background to black
            LOGI("Draw background to black");
            background->lock(&sinfo_background);
            memset(sinfo_background.bits, 0xff, sinfo_background.w*sinfo_background.h*2);
            char *prgb565 = (char *)sinfo_background.bits+(overlay_param1.pos_y*sinfo_background.w+overlay_param1.pos_x)*2;
            for(int height = overlay_param1.pos_y; height < (overlay_param1.pos_y+overlay_param1.win_h); height++) {
                memset(prgb565, 0, overlay_param1.win_w*2);
                prgb565 += (sinfo_background.w*2);
            }
        	background->unlockAndPost();
            LOGI("Overlay running");
            pthread_create(&thread1, NULL, overlay_instance_test, (void *)(&overlay_param1));
            pthread_join(thread1, &ret);
        }
        else if(!strcmp(argv[1],"-6")){
            LOGI("****Run one overlay instances 720/576 RGB test*****");
            //pthread_attr_init(&attr0);
            //pthread_attr_setdetachstate(&attr0, PTHREAD_CREATE_DETACHED);
            overlay_param0.format = PIXEL_FORMAT_RGB_565;
            if(argc==3) {
                overlay_param0.show_num = atoi(argv[2]);
            }

            LOGI("Display info: panel_x %d panel_y %d",panel_x,panel_y);
            LOGI("Overlay paramater: w %d,h %d, pos_x %d, pos_y %d, win_h %d win_w %d",
                 overlay_param0.w,overlay_param0.h,
                 overlay_param0.pos_x,overlay_param0.pos_y,
                 overlay_param0.win_w,overlay_param0.win_h);

            //draw the background to black
            LOGI("Draw background to black");
            background->lock(&sinfo_background);
            memset(sinfo_background.bits, 0xff, sinfo_background.w*sinfo_background.h*2);
            char *prgb565 = (char *)sinfo_background.bits+(overlay_param0.pos_y*sinfo_background.w+overlay_param0.pos_x)*2;
            for(int height = overlay_param0.pos_y; height < (overlay_param0.pos_y+overlay_param0.win_h); height++) {
                memset(prgb565, 0, overlay_param0.win_w*2);
                prgb565 += (sinfo_background.w*2);
            }
        	background->unlockAndPost();
            LOGI("Overlay running");
            pthread_create(&thread0, NULL, overlay_instance_test, (void *)(&overlay_param0));
            pthread_join(thread0, &ret);
        }
        else{
            LOGE("Error!Not support parameters");
            LOGE("%s [-0] [-1] [-2] [-3] [-4] [-5] [-6] [frame num]",argv[0]);
            LOGE("  [-0] verlay instances 720/576 YUV test");
            LOGE("  [-1] verlay instances 320/240 YUV test");
            LOGE("  [-2] verlay instances 720/576&320/240 YUV test");
            LOGE("  [-3] verlay instances 720/576&320/240 YUV performance test");
            LOGE("  [-4] verlay instances 720/576 YUV test");
            LOGE("  [-5] verlay instances 320/240 YUV test");
            LOGE("  [-6] verlay instances 720/576 RGB565 test");
            LOGE("  [frame num] frame count number");
        }
    }
    else{
        LOGE("Error!Not support parameters");
        LOGE("%s [-0] [-1] [-2] [-3] [-4] [-5] [-6] [frame num]",argv[0]);
        LOGE("  [-0] verlay instances 720/576 YUV test");
        LOGE("  [-1] verlay instances 320/240 YUV test");
        LOGE("  [-2] verlay instances 720/576&320/240 YUV test");
        LOGE("  [-3] verlay instances 720/576&320/240 YUV performance test");
        LOGE("  [-4] verlay instances 720/576 YUV test");
        LOGE("  [-5] verlay instances 320/240 YUV test");
        LOGE("  [-6] verlay instances 720/576 RGB565 test");
        LOGE("  [frame num] frame count number");
    }

    LOGI("*******Exit main thread*******");
    close(fd_fb);
    return 0;
}



void *overlay_instance_test(void *arg)
{
    // set up the thread-pool
    OVERLAY_PARAM *overlay_param = (OVERLAY_PARAM *)arg;

    LOGI("FSL OVERLAY test: current pid %d uid %d",getpid(),getuid());
    sp<ProcessState> proc(ProcessState::self());
    ProcessState::self()->startThreadPool();

    // create a client to surfaceflinger
    sp<SurfaceComposerClient> client = new SurfaceComposerClient();
    

    // create pushbuffer surface
    sp<SurfaceControl> surface = client->createSurface(getpid(), 0, overlay_param->win_w, overlay_param->win_h, 
            PIXEL_FORMAT_UNKNOWN, ISurfaceComposer::ePushBuffers);
    client->openTransaction();
    LOGI("Set layer 0x%x, posx %d, posy %d",
         overlay_param->layer,overlay_param->pos_x,overlay_param->pos_y);
    surface->setLayer(overlay_param->layer);
    surface->setPosition(overlay_param->pos_x, overlay_param->pos_y);
    client->closeTransaction();

    // get to the isurface
    sp<ISurface> isurface = Test::getISurface(surface);
    LOGI("isurface = %p", isurface.get());
    
    // now request an overlay
    LOGI("createOverlay()");
    sp<OverlayRef> ref = isurface->createOverlay(overlay_param->w, overlay_param->h,
                                                 overlay_param->format, 0);
    LOGI("createOverlay() AFTER");
    LOGI("new Overlay(ref) start");
    sp<Overlay> overlay = new Overlay(ref);
    LOGI("Overlay(ref)()");
    //Just to give a chance the other instance creation
    usleep(30000);
    int bufcount = overlay_param->frame_num;
    int ret;
    if(overlay->getBufferCount() != bufcount) {
        ret = overlay->setParameter(OVERLAY_BUFNUM,bufcount);
        if(ret < 0) {
            LOGE("Error! Not support parameter setting for overlay");
            return NULL;
        }
    }

    ret = overlay->setCrop(overlay_param->crop_x,overlay_param->crop_y,
                           overlay_param->crop_w,overlay_param->crop_h);
    if(ret < 0) {
        LOGE("Error! Not support parameter setting for overlay");
        return NULL;
    }

    /*
     * here we can use the overlay API 
     */
    if(!overlay_param->performance_test) {
        overlay_buffer_t buffer; 
        LOGI("Start queue/dequeue test");
        for(int i = 0; i < overlay_param->show_num; i++) {
            overlay->dequeueBuffer(&buffer);
            //LOGI("dequeueBuffer buffer = 0x%x\n", buffer);

            void* address = overlay->getBufferAddress(buffer);
            //LOGI("getBufferAddress = 0x%x\n", address);
            unsigned int phy_addr = overlay->getBufferAddressPhy(buffer);
            //LOGI("getBufferAddressPhy = 0x%x\n", phy_addr);

            //Draw the data to the buffer address(YUV420 format as setting)
            gen_fill_pattern((char *)address,overlay_param->w,overlay_param->h,
                             overlay_param->w/overlay_param->step_line,
                             overlay_param->w/overlay_param->step_line,
                             overlay_param->format);
            overlay->queueBuffer(buffer);
        }
    }
    else{
        overlay_buffer_t buffer[32];
        LOGI("Start performance queue/dequeue test");
        memset(buffer,0,sizeof(overlay_buffer_t)*32);
        for(int i = 0; i < overlay_param->frame_num; i++) {
            overlay->dequeueBuffer(&buffer[i]);
            void* address = overlay->getBufferAddress(buffer[i]);
            gen_fill_pattern((char *)address,overlay_param->w,overlay_param->h,
                             overlay_param->w/overlay_param->step_line,
                             overlay_param->w/overlay_param->step_line,
                             overlay_param->format);
        }
        struct timeval startTime,lastTime;
        int frameshow = 0;
        gettimeofday(&startTime, 0);
        for(int i = 0; i < overlay_param->frame_num; i++) {
            overlay->queueBuffer(buffer[i]);
            frameshow++;
        }
        for(int i = 0; i < overlay_param->show_num; i++){
            overlay->dequeueBuffer(&buffer[0]);
            overlay->queueBuffer(buffer[0]);
            frameshow++;
            //Exit performance test if one instance finished testing.
            if(!performance_run) {
                break;
            }
        }
        performance_run = 0;
        gettimeofday(&lastTime, 0);
        int timeInMs = (lastTime.tv_sec - startTime.tv_sec)*1000 +(lastTime.tv_usec - startTime.tv_usec)/1000;
        float fps = 1000.0*frameshow/timeInMs;
        LOGI("Performance test for Input %d %d, Output %d %d",
             overlay_param->w,overlay_param->h,
             overlay_param->win_w,overlay_param->win_h);
        LOGI("Total frame %d, total time %d ms, fps %f",
             frameshow,timeInMs,fps);
    }

    
    return NULL;
}
