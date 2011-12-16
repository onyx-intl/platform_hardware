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


#include <fcntl.h>
#include <cutils/properties.h>
#include "fsl_overlay.h"
#include "overlay_thread.h"


/*
  *  calculate display window in new display border ,keep relative position and size of original window in original border,
  *  but do not keep aspect ratio of original window
  *  original_border:    input
  *  original_win:      input
  *  new_border:      input
  *  new_win:           output
  */

int OverlayThread::calcDispWin(RECTTYPE *original_border, RECTTYPE *original_win, RECTTYPE *new_border, RECTTYPE *new_win)
{
    if(!original_border || !original_win || !new_border || !new_win){
        OVERLAY_LOG_ERR("need check paramter to %s, %d, %p/%p/%p/%p", __FUNCTION__, __LINE__, original_border, original_win, new_border, new_win);
        return -1;
    }
    if(original_border->nWidth <= 0 || original_border->nHeight <= 0){
        OVERLAY_LOG_ERR("need check paramter to %s, %d, original_border %d/%d", __FUNCTION__, __LINE__, original_border->nWidth, original_border->nHeight);
        return -1;
    }

    new_win->nLeft = original_win->nLeft * new_border->nWidth / original_border->nWidth + new_border->nLeft;
    new_win->nTop = original_win->nTop * new_border->nHeight / original_border->nHeight + new_border->nTop;
    new_win->nWidth = original_win->nWidth * new_border->nWidth/ original_border->nWidth;
    new_win->nHeight = original_win->nHeight * new_border->nHeight / original_border->nHeight;

    OVERLAY_LOG_RUNTIME("calcDispWin: old border %d/%d, win %d/%d/%d/%d", original_border->nWidth, original_border->nHeight,
                        original_win->nLeft, original_win->nTop, original_win->nWidth, original_win->nHeight);
    OVERLAY_LOG_RUNTIME("calcDispWin: new border %d/%d, win %d/%d/%d/%d", new_border->nWidth, new_border->nHeight,
                        new_win->nLeft, new_win->nTop, new_win->nWidth, new_win->nHeight);
    
    return 0;
}


/*
  *  calculate display window in new display border ,keep aspect ratio of original window,
  *  original_win:      input
  *  new_border:      input
  *  new_win:           output
  *  example: in dual display, a stream of 640x480 in 800x480 fb0(WVGA) will be displayed on 1024x768 fb1(LCD), UI is resized to 1024x614,
  */

int OverlayThread::calcDispWinWithAR(
                                                            RECTTYPE *original_win, 
                                                            RECTTYPE *new_border, 
                                                            RECTTYPE *new_win)
{
    int nWidth, nHeight;

    if(!original_win || !new_border || !new_win){
        OVERLAY_LOG_ERR("need check paramter to %s, %d, %p/%p/%p", __FUNCTION__, __LINE__, original_win, new_border, new_win);
        return -1;
    }

    if(original_win->nWidth <= 0 || original_win->nHeight <= 0){
        OVERLAY_LOG_ERR("check parameter to %s! original_win %d/%d", __FUNCTION__, original_win->nWidth, original_win->nHeight);
        return -1;
    }
    if(original_win->nWidth * new_border->nHeight >= original_win->nHeight * new_border->nWidth) {
        /* Crop display height */
        nHeight = original_win->nHeight * new_border->nWidth / original_win->nWidth;
        new_win->nTop = (new_border->nHeight - nHeight) / 2 + new_border->nTop;
        new_win->nLeft = new_border->nLeft;
        new_win->nWidth = new_border->nWidth;
        new_win->nHeight = nHeight;
    }
    else {
        /* Crop display width */
        nWidth = original_win->nWidth * new_border->nHeight / original_win->nHeight;
        new_win->nLeft = (new_border->nWidth - nWidth) / 2 + new_border->nLeft;
        new_win->nTop = new_border->nTop;
        new_win->nWidth = nWidth;
        new_win->nHeight = new_border->nHeight;
    }

    OVERLAY_LOG_RUNTIME("output of %s: %d/%d/%d/%d", __FUNCTION__, new_win->nLeft, new_win->nTop, new_win->nWidth, new_win->nHeight);
    return 0;

}

void OverlayThread::switchTvOut(struct overlay_control_context_t * dev)
{
    OVERLAY_LOG_FUNC;

    if(dev->stream_on){
        int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        ioctl(dev->v4l_id, VIDIOC_STREAMOFF, &type);
        OVERLAY_LOG_INFO("V4L STREAMON OFF");
        dev->video_frames = 0;
        dev->stream_on = false;
        //refill the back color
        for(int i = 0;i < dev->v4l_bufcount;i++) {
            fill_frame_back(dev->v4lbuf_addr[i],dev->v4l_buffers[i].length,
                            dev->xres,dev->yres,dev->outpixelformat);
        }
    }
    overlay_deinit_v4l(dev);

    overlay_deinit_fbdev(dev);


    if(dev->display_mode == DISPLAY_MODE_TV) { 
        OVERLAY_LOG_INFO(" *** switch into TV mode ***");
        char value[PROPERTY_VALUE_MAX];
        char * end;

        property_get("rw.VIDEO_TVOUT_WIDTH", value, "");
        dev->user_property.nWidth = strtol(value, &end, 10);
        property_get("rw.VIDEO_TVOUT_HEIGHT", value, "");
        dev->user_property.nHeight= strtol(value, &end, 10);
        property_get("rw.VIDEO_TVOUT_POS_X", value, "");
        dev->user_property.nLeft = strtol(value, &end, 10);
        property_get("rw.VIDEO_TVOUT_POS_Y", value, "");
        dev->user_property.nTop = strtol(value, &end, 10);

        OVERLAY_LOG_INFO("get propty %d/%d/%d/%d", dev->user_property.nLeft, dev->user_property.nTop, dev->user_property.nWidth, dev->user_property.nHeight);

        if(overlay_init_fbdev(dev, FB1_DEV_NAME)<0){
            OVERLAY_LOG_ERR("Error!init fbdev %s failed", FB1_DEV_NAME);
             return;
        }
        
         if(overlay_init_v4l(dev, TV_V4L_LAYER)<0){
             OVERLAY_LOG_ERR("Error!init v4l failed");
             return;
         }
    }
    else{
        OVERLAY_LOG_INFO(" *** switch out of TV mode ***");

        if(overlay_init_fbdev(dev, DEFAULT_FB_DEV_NAME)<0)
            OVERLAY_LOG_ERR("Error!init fbdev %s failed", DEFAULT_FB_DEV_NAME);

         if(overlay_init_v4l(dev, DEFAULT_V4L_LAYER)<0){
             OVERLAY_LOG_ERR("Error!init v4l failed");
             return;
         }
    }

    return;
}

void OverlayThread::switchDualDisp(struct overlay_control_context_t * dev)
{
    OVERLAY_LOG_FUNC;

    if(dev->stream_on){
        int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        ioctl(dev->v4l_id, VIDIOC_STREAMOFF, &type);
        OVERLAY_LOG_INFO("V4L STREAMON OFF");
        dev->video_frames = 0;
        dev->stream_on = false;
        //refill the back color
        for(int i = 0;i < dev->v4l_bufcount;i++) {
            fill_frame_back(dev->v4lbuf_addr[i],dev->v4l_buffers[i].length,
                            dev->xres,dev->yres,dev->outpixelformat);
        }
    }
    overlay_deinit_v4l(dev);

    if(dev->display_mode == DISPLAY_MODE_DUAL_DISP) { 
        OVERLAY_LOG_INFO(" *** switch into dual disp mode ***");

        char value[PROPERTY_VALUE_MAX];
        char * end;

        property_get("rw.VIDEO_SECOND_WIDTH", value, "");
        dev->user_property.nWidth = strtol(value, &end, 10);
        property_get("rw.VIDEO_SECOND_HEIGHT", value, "");
        dev->user_property.nHeight= strtol(value, &end, 10);
        property_get("rw.VIDEO_SECOND_POS_X", value, "");
        dev->user_property.nLeft = strtol(value, &end, 10);
        property_get("rw.VIDEO_SECOND_POS_Y", value, "");
        dev->user_property.nTop = strtol(value, &end, 10);

        OVERLAY_LOG_INFO("get propty %d/%d/%d/%d", dev->user_property.nLeft, dev->user_property.nTop, dev->user_property.nWidth, dev->user_property.nHeight);

        int fb_dev;
        fb_dev  = open(FB1_DEV_NAME, O_RDWR | O_NONBLOCK, 0);
        if(fb_dev < 0) {
            OVERLAY_LOG_ERR("Error!Open fb device %s failed",FB1_DEV_NAME);
            return ;
        }
        
        struct fb_var_screeninfo fb_var;
        if ( ioctl(fb_dev, FBIOGET_VSCREENINFO, &fb_var) < 0) {
            OVERLAY_LOG_ERR("Error!VSCREENINFO getting failed for dev %s",FB1_DEV_NAME);
            close(fb_dev);
            return;
        }

        dev->fb1_resolution.nWidth = fb_var.xres;
        dev->fb1_resolution.nHeight = fb_var.yres;

        OVERLAY_LOG_INFO(" FB1 resolution %d,%d", fb_var.xres, fb_var.yres);

        close(fb_dev);

        // use fb1 resolution to initialize v4l
        dev->xres = dev->fb1_resolution.nWidth;
        dev->yres = dev->fb1_resolution.nHeight;

         if(overlay_init_v4l(dev, DEFAULT_V4L_LAYER)<0){
             OVERLAY_LOG_ERR("Error!init v4l failed");
             return;
         }
    }
    else{
        OVERLAY_LOG_INFO(" *** switch out of dual disp mode ***");

        dev->xres = dev->default_fb_resolution.nWidth;
        dev->yres = dev->default_fb_resolution.nHeight;

         if(overlay_init_v4l(dev, DEFAULT_V4L_LAYER)<0){
             OVERLAY_LOG_ERR("Error!init v4l failed");
             return;
         }
    }

    return;

}

static int get_v4l2_fmt(int format) {
    switch (format) {
    case HAL_PIXEL_FORMAT_YCbCr_420_SP:
        format = v4l2_fourcc('N', 'V', '1', '2');
        break;
    case HAL_PIXEL_FORMAT_YCbCr_420_I:
        format = v4l2_fourcc('I', '4', '2', '0');
        break;
    case HAL_PIXEL_FORMAT_YCbCr_422_I:
        format = v4l2_fourcc('4', '2', '2', 'P');
        break;
    case HAL_PIXEL_FORMAT_RGB_565:
        format = v4l2_fourcc('R', 'G', 'B', 'P');
        break;
    default:
        OVERLAY_LOG_ERR("Error!Not supported format %d", format);
	break;
    }
    return format;
}

int OverlayThread::checkDispMode(struct overlay_control_context_t * dev, bool *mode_changed)
{
    bool bValue = false, tv_video_only_mode = false;
    char value[10];
    OVERLAY_LOG_FUNC;

    property_get("rw.VIDEO_TVOUT_DISPLAY", value, "");
    if (strcmp(value, "1") == 0) {
        bValue = true;
        tv_video_only_mode = true;
    }

    if((dev->display_mode == DISPLAY_MODE_TV)  !=  bValue){
        dev->display_mode = bValue ? DISPLAY_MODE_TV : DISPLAY_MODE_NORMAL;
        switchTvOut(dev);
        *mode_changed = true;
        return 0;
    }

    bValue = false;
    property_get("sys.SECOND_DISPLAY_ENABLED", value, "");
    if (strcmp(value, "1") == 0) {
        bValue = true;
	dev->sec_disp_enable = true;
    } else
	dev->sec_disp_enable = false;

    if((dev->display_mode == DISPLAY_MODE_DUAL_DISP)  !=  bValue){
        dev->display_mode = bValue ? DISPLAY_MODE_DUAL_DISP : DISPLAY_MODE_NORMAL;
        if (dev->video_play_mode == SIN_VIDEO_DUAL_UI)
            switchDualDisp(dev);
	else if (dev->sec_disp_enable) {
            int fb_dev;
            fb_dev  = open(FB1_DEV_NAME, O_RDWR | O_NONBLOCK, 0);
            if(fb_dev < 0) {
                OVERLAY_LOG_ERR("Error!Open fb device %s failed",FB1_DEV_NAME);
            }

            struct fb_var_screeninfo fb_var;
            if ( ioctl(fb_dev, FBIOGET_VSCREENINFO, &fb_var) < 0) {
                OVERLAY_LOG_ERR("Error!VSCREENINFO getting failed for dev %s",FB1_DEV_NAME);
                close(fb_dev);
            }

            memset(&sec_video_mIPUHandle, 0, sizeof(ipu_lib_handle_t));
            sec_video_mIPUInputParam.width = m_dev->overlay_intances[0]->mHandle.width;
            sec_video_mIPUInputParam.height = m_dev->overlay_intances[0]->mHandle.height;
            sec_video_mIPUInputParam.input_crop_win.pos.x = m_dev->overlay_intances[0]->mDataShared->crop_x;
            sec_video_mIPUInputParam.input_crop_win.pos.y = m_dev->overlay_intances[0]->mDataShared->crop_y;
            sec_video_mIPUInputParam.input_crop_win.win_w = m_dev->overlay_intances[0]->mDataShared->crop_w;
            sec_video_mIPUInputParam.input_crop_win.win_h = m_dev->overlay_intances[0]->mDataShared->crop_h;
            if(m_dev->overlay_intances[0]->mHandle.format == HAL_PIXEL_FORMAT_YCbCr_420_SP) {
                sec_video_mIPUInputParam.fmt = v4l2_fourcc('N', 'V', '1', '2');
            }
            else if(m_dev->overlay_intances[0]->mHandle.format == HAL_PIXEL_FORMAT_YCbCr_420_I) {
                sec_video_mIPUInputParam.fmt = v4l2_fourcc('I', '4', '2', '0');
            }
            else if(m_dev->overlay_intances[0]->mHandle.format == HAL_PIXEL_FORMAT_YCbCr_422_I) {
                sec_video_mIPUInputParam.fmt = v4l2_fourcc('4', '2', '2', 'P');
            }
            else if(m_dev->overlay_intances[0]->mHandle.format == HAL_PIXEL_FORMAT_RGB_565) {
                sec_video_mIPUInputParam.fmt = v4l2_fourcc('R', 'G', 'B', 'P');
            }else{
                OVERLAY_LOG_ERR("Error!Not supported input format %d",m_dev->overlay_intances[0]->mHandle.format);
            }
            sec_video_mIPUInputParam.user_def_paddr[0] = m_dev->overlay_intances[0]->mDataShared->queued_bufs[m_dev->overlay_intances[0]->mDataShared->queued_head];
            if (tv_video_only_mode)
                sec_video_mIPUOutputParam.fmt = v4l2_fourcc('U', 'Y', 'V', 'Y');
	    else {
                switch (fb_var.bits_per_pixel) {
                case 24:
                   sec_video_mIPUOutputParam.fmt = IPU_PIX_FMT_BGR24;
                   OVERLAY_LOG_ERR("output fmt BGR24");
                   break;
                case 32:
                   sec_video_mIPUOutputParam.fmt = IPU_PIX_FMT_BGR32;
                   OVERLAY_LOG_ERR("output fmt BGR32");
                   break;
                case 16:
                   sec_video_mIPUOutputParam.fmt = IPU_PIX_FMT_RGB565;
                   OVERLAY_LOG_ERR("output fmt RGB565");
                   break;
                }
	    }
            sec_video_mIPUOutputParam.width = fb_var.xres;
            sec_video_mIPUOutputParam.height = fb_var.yres;
            sec_video_mIPUOutputParam.show_to_fb = 1;
            sec_video_mIPUOutputParam.fb_disp.fb_num = 1;
            sec_video_mIPUOutputParam.fb_disp.pos.x = 0;
            sec_video_mIPUOutputParam.fb_disp.pos.y = 0;
            sec_video_mIPUOutputParam.output_win.pos.x = 0;
            sec_video_mIPUOutputParam.output_win.pos.y = 0;
            sec_video_mIPUOutputParam.output_win.win_w = fb_var.xres;
            sec_video_mIPUOutputParam.output_win.win_h = fb_var.yres;
            sec_video_mIPUOutputParam.rot = 0;
            sec_video_mIPUOutputParam.user_def_paddr[0] = 0;
            sec_video_mIPURet = mxc_ipu_lib_task_init(&sec_video_mIPUInputParam,NULL,&sec_video_mIPUOutputParam,OP_NORMAL_MODE|TASK_VF_MODE,&sec_video_mIPUHandle);
            if (sec_video_mIPURet < 0) {
               OVERLAY_LOG_ERR("Error!Obj0 second video mxc_ipu_lib_task_init failed mIPURet %d!",sec_video_mIPURet);
            }
            memcpy(&(dev->sec_video_IPUHandle), &sec_video_mIPUHandle, sizeof(ipu_lib_handle_t));
	} else {
            mxc_ipu_lib_task_uninit(&sec_video_mIPUHandle);
            OVERLAY_LOG_INFO("Second video IPU task uninit");
            memset(&sec_video_mIPUHandle, 0, sizeof(ipu_lib_handle_t));
            memcpy(&(dev->sec_video_IPUHandle), &sec_video_mIPUHandle, sizeof(ipu_lib_handle_t));
        }
        *mode_changed = true;
        return 0;
    }

    if (dev->video_play_mode == DUAL_VIDEO_SIN_UI && dev->sec_disp_enable &&
	(sec_video_mIPUInputParam.width != m_dev->overlay_intances[0]->mHandle.width ||
         sec_video_mIPUInputParam.height != m_dev->overlay_intances[0]->mHandle.height ||
         sec_video_mIPUInputParam.input_crop_win.pos.x != m_dev->overlay_intances[0]->mDataShared->crop_x ||
         sec_video_mIPUInputParam.input_crop_win.pos.y != m_dev->overlay_intances[0]->mDataShared->crop_y ||
         sec_video_mIPUInputParam.input_crop_win.win_w != m_dev->overlay_intances[0]->mDataShared->crop_w ||
	 sec_video_mIPUInputParam.input_crop_win.win_h != m_dev->overlay_intances[0]->mDataShared->crop_h ||
	 sec_video_mIPUInputParam.fmt != get_v4l2_fmt(m_dev->overlay_intances[0]->mHandle.format))) {
        mxc_ipu_lib_task_uninit(&sec_video_mIPUHandle);

        int fb_dev;
        fb_dev  = open(FB1_DEV_NAME, O_RDWR | O_NONBLOCK, 0);
        if(fb_dev < 0) {
            OVERLAY_LOG_ERR("Error!Open fb device %s failed",FB1_DEV_NAME);
        }

        struct fb_var_screeninfo fb_var;
        if ( ioctl(fb_dev, FBIOGET_VSCREENINFO, &fb_var) < 0) {
            OVERLAY_LOG_ERR("Error!VSCREENINFO getting failed for dev %s",FB1_DEV_NAME);
            close(fb_dev);
        }

        memset(&sec_video_mIPUHandle, 0, sizeof(ipu_lib_handle_t));
        sec_video_mIPUInputParam.width = m_dev->overlay_intances[0]->mHandle.width;
        sec_video_mIPUInputParam.height = m_dev->overlay_intances[0]->mHandle.height;
        sec_video_mIPUInputParam.input_crop_win.pos.x = m_dev->overlay_intances[0]->mDataShared->crop_x;
        sec_video_mIPUInputParam.input_crop_win.pos.y = m_dev->overlay_intances[0]->mDataShared->crop_y;
        sec_video_mIPUInputParam.input_crop_win.win_w = m_dev->overlay_intances[0]->mDataShared->crop_w;
        sec_video_mIPUInputParam.input_crop_win.win_h = m_dev->overlay_intances[0]->mDataShared->crop_h;
        if(m_dev->overlay_intances[0]->mHandle.format == HAL_PIXEL_FORMAT_YCbCr_420_SP) {
            sec_video_mIPUInputParam.fmt = v4l2_fourcc('N', 'V', '1', '2');
        }
        else if(m_dev->overlay_intances[0]->mHandle.format == HAL_PIXEL_FORMAT_YCbCr_420_I) {
            sec_video_mIPUInputParam.fmt = v4l2_fourcc('I', '4', '2', '0');
        }
        else if(m_dev->overlay_intances[0]->mHandle.format == HAL_PIXEL_FORMAT_YCbCr_422_I) {
            sec_video_mIPUInputParam.fmt = v4l2_fourcc('4', '2', '2', 'P');
        }
        else if(m_dev->overlay_intances[0]->mHandle.format == HAL_PIXEL_FORMAT_RGB_565) {
            sec_video_mIPUInputParam.fmt = v4l2_fourcc('R', 'G', 'B', 'P');
        }else{
            OVERLAY_LOG_ERR("Error!Not supported input format %d",m_dev->overlay_intances[0]->mHandle.format);
        }
        sec_video_mIPUInputParam.user_def_paddr[0] = m_dev->overlay_intances[0]->mDataShared->queued_bufs[m_dev->overlay_intances[0]->mDataShared->queued_head];
        if (tv_video_only_mode)
            sec_video_mIPUOutputParam.fmt = v4l2_fourcc('U', 'Y', 'V', 'Y');
        else {
            switch (fb_var.bits_per_pixel) {
            case 24:
               sec_video_mIPUOutputParam.fmt = IPU_PIX_FMT_BGR24;
               OVERLAY_LOG_ERR("output fmt BGR24");
               break;
            case 32:
               sec_video_mIPUOutputParam.fmt = IPU_PIX_FMT_BGR32;
               OVERLAY_LOG_ERR("output fmt BGR32");
               break;
            case 16:
               sec_video_mIPUOutputParam.fmt = IPU_PIX_FMT_RGB565;
               OVERLAY_LOG_ERR("output fmt RGB565");
               break;
            }
        }
        sec_video_mIPUOutputParam.width = fb_var.xres;
        sec_video_mIPUOutputParam.height = fb_var.yres;
        sec_video_mIPUOutputParam.show_to_fb = 1;
        sec_video_mIPUOutputParam.fb_disp.fb_num = 1;
        sec_video_mIPUOutputParam.fb_disp.pos.x = 0;
        sec_video_mIPUOutputParam.fb_disp.pos.y = 0;
        sec_video_mIPUOutputParam.output_win.pos.x = 0;
        sec_video_mIPUOutputParam.output_win.pos.y = 0;
        sec_video_mIPUOutputParam.output_win.win_w = fb_var.xres;
        sec_video_mIPUOutputParam.output_win.win_h = fb_var.yres;
        sec_video_mIPUOutputParam.rot = 0;
        sec_video_mIPUOutputParam.user_def_paddr[0] = 0;
        sec_video_mIPURet = mxc_ipu_lib_task_init(&sec_video_mIPUInputParam,NULL,&sec_video_mIPUOutputParam,OP_NORMAL_MODE|TASK_VF_MODE,&sec_video_mIPUHandle);
        if (sec_video_mIPURet < 0) {
           OVERLAY_LOG_ERR("Error!Obj0 second video mxc_ipu_lib_task_init failed mIPURet %d!",sec_video_mIPURet);
        }
        memcpy(&(dev->sec_video_IPUHandle), &sec_video_mIPUHandle, sizeof(ipu_lib_handle_t));
    }
    return 0;
}

int OverlayThread::calcOutputParam(struct overlay_control_context_t * dev, overlay_object *overlayObj, RECTTYPE *new_win,  int * rotation)
{
    OVERLAY_LOG_FUNC;
    
    if(!dev || !overlayObj || !new_win || !rotation){
        OVERLAY_LOG_ERR("need check paramter to %s, %d, %p/%p/%p/%p", __FUNCTION__, __LINE__, dev, overlayObj, new_win, rotation);
        return -1;
    }

    switch(dev->display_mode){
        case DISPLAY_MODE_TV:
        case DISPLAY_MODE_DUAL_DISP:
            {
                if (dev->video_play_mode == DUAL_VIDEO_SIN_UI) {
                   new_win->nLeft = overlayObj->outX;
                   new_win->nTop = overlayObj->outY;
                   new_win->nWidth = overlayObj->outW;
                   new_win->nHeight = overlayObj->outH;
                   *rotation = overlayObj->rotation;
                } else {
                   RECTTYPE original_win, new_border;
                   original_win.nLeft = overlayObj->outX;
                   original_win.nTop = overlayObj->outY;
                   original_win.nWidth = overlayObj->outW;
                   original_win.nHeight = overlayObj->outH;

                   // when there is just on overlay object and it's playing a stream, output window will fit fb1
                   if(dev->overlay_number == 1 && overlayObj->mDataShared->for_playback){
                       memcpy(&new_border, &dev->fb1_resolution, sizeof(RECTTYPE));
                       calcDispWinWithAR(&original_win, &new_border, new_win);
                   }
                   else{
                       if(dev->user_property.nWidth != 0 && dev->user_property.nHeight != 0)
                           memcpy(&new_border, &dev->user_property, sizeof(RECTTYPE));
                       else
                           memcpy(&new_border, &dev->fb1_resolution, sizeof(RECTTYPE));

                       calcDispWin( &dev->default_fb_resolution, &original_win, &new_border, new_win);
                   }
                   *rotation = 0;
                }
                break;
            }
        default:
            {
                new_win->nLeft = overlayObj->outX;
                new_win->nTop = overlayObj->outY;
                new_win->nWidth = overlayObj->outW;
                new_win->nHeight = overlayObj->outH;
                *rotation = overlayObj->rotation;
                break;
            }
    }

    new_win->nTop = (new_win->nTop + 4) & 0xfffffff8;
    new_win->nLeft = (new_win->nLeft + 4) & 0xfffffff8;
    new_win->nWidth = (new_win->nWidth + 4) & 0xfffffff8;
    new_win->nHeight = (new_win->nHeight + 4) & 0xfffffff8;

    OVERLAY_LOG_INFO("output of %s: %d/%d/%d/%d", __FUNCTION__, new_win->nLeft, new_win->nTop, new_win->nWidth, new_win->nHeight);

    return 0;
}


bool OverlayThread::threadLoop() {
        int index = 0;
        overlay_object *overlayObj0;
        overlay_object *overlayObj1;
        overlay_data_shared_t *dataShared0;
        overlay_data_shared_t *dataShared1;
        unsigned int overlay_buf0;
        unsigned int overlay_buf1;
        bool modeChange = false;
        bool outchange0 = false, modeChange0 = false;
        bool outchange1 = false, modeChange1 = false;
        WIN_REGION overlay0_outregion;
        WIN_REGION overlay1_outregion;
        int rotation0 = 0, rotation1 = 0;
        int crop_x0 = 0,crop_y0 = 0,crop_w0 = 0,crop_h0 = 0;
        int crop_x1 = 0,crop_y1 = 0,crop_w1 = 0,crop_h1 = 0;
        struct timespec wait_time={0,0}, current_time={0,0}, last_check_time={0,0};
        RECTTYPE rect_out0, rect_out1;

        while(m_dev&&(m_dev->overlay_running)) {
            OVERLAY_LOG_RUNTIME("Overlay thread running pid %d tid %d", getpid(),gettid());

            //Wait for semphore for overlay instance buffer queueing
            //sem_wait(&m_dev->control_shared->overlay_sem);

            clock_gettime( CLOCK_REALTIME, &wait_time);

            wait_time.tv_nsec += 500 * 1000000;  // wait 500ms
            if(wait_time.tv_nsec >= 1000000000){
                wait_time.tv_sec += 1;
                wait_time.tv_nsec -= 1000000000;
            }
            sem_timedwait(&m_dev->control_shared->overlay_sem, &wait_time);

            OVERLAY_LOG_RUNTIME("Get overlay semphore here pid %d tid %d",getpid(),gettid());
            overlayObj0 = NULL;
            overlayObj1 = NULL;
            dataShared0 = NULL;
            dataShared1 = NULL;
            overlay_buf0 = NULL;
            overlay_buf1 = NULL;
            outchange0 = false;
            outchange1 = false;
            memset(&overlay0_outregion, 0, sizeof(overlay0_outregion));
            memset(&overlay1_outregion, 0, sizeof(overlay1_outregion));

            //Check current active overlay instance
            pthread_mutex_lock(&m_dev->control_lock);
    
            if(m_dev->overlay_number >= 1) {
                for(index= 0;index < MAX_OVERLAY_INSTANCES;index++) {
                    if(m_dev->overlay_instance_valid[index]) {
                        if(!overlayObj0) {
                            overlayObj0 = m_dev->overlay_intances[index];
                        }
                        //For those small zorder, it should be drawn firstly
                        else if(m_dev->overlay_intances[index]->zorder < overlayObj0->zorder){
                            overlayObj1 = overlayObj0;
                            overlayObj0 = m_dev->overlay_intances[index];
                        }
                        else{
                            overlayObj1 = m_dev->overlay_intances[index];
                        }
                    }
                }
            }

            //OVERLAY_LOG_INFO("obj0: %p, obj1: %p", overlayObj0, overlayObj1);
            if(overlayObj0 || overlayObj1){
                clock_gettime( CLOCK_REALTIME, &current_time);
                if(current_time.tv_sec > last_check_time.tv_sec){
                    checkDispMode(m_dev, &modeChange);
                    if(modeChange){
                        modeChange0 = true;
                        modeChange1 = true;
                        modeChange = false;
                    }
                    clock_gettime( CLOCK_REALTIME, &last_check_time);
                }
            }


            if(overlayObj0) {
                dataShared0 = overlayObj0->mDataShared;
                OVERLAY_LOG_RUNTIME("Process obj 0 instance_id %d, dataShared0 0x%x",
                                    dataShared0->instance_id,dataShared0);
                pthread_mutex_lock(&dataShared0->obj_lock);
                //Fetch one buffer from each overlay instance buffer queue
                OVERLAY_LOG_RUNTIME("queued_count %d,queued_head %d",
                     dataShared0->queued_count,dataShared0->queued_head);

                //If this overlay is under destroy, just return all buffers in queue
                //The user should guarantee the buffer all to be flushed before destroy it.
                if((dataShared0->in_destroy)||(!overlayObj0->visible)) {
                     OVERLAY_LOG_INFO("Overlay in destroy or invisible,flush queue as queued_count %d,queued_head %d",
                     dataShared0->queued_count,dataShared0->queued_head);
                     while(dataShared0->queued_count > 0) {
                        //fetch the head buffer in queued buffers
                        overlay_buf0 = dataShared0->queued_bufs[dataShared0->queued_head];
                        OVERLAY_LOG_RUNTIME("id %d Get queue buffer for Overlay Instance 0: 0x%x queued_count %d",
                             dataShared0->instance_id,overlay_buf0,dataShared0->queued_count);
                        dataShared0->queued_bufs[dataShared0->queued_head] = 0;
                        dataShared0->queued_head ++;
                        dataShared0->queued_head = dataShared0->queued_head%MAX_OVERLAY_BUFFER_NUM;
                        dataShared0->queued_count --;

                        //For push mode, no need to return the buffer back
                        if(dataShared0->overlay_mode == OVERLAY_NORAML_MODE) {
                            dataShared0->free_bufs[dataShared0->free_tail] = overlay_buf0;
                            OVERLAY_LOG_RUNTIME("Id %d back buffer to free queue for Overlay Instance 0: 0x%x at %d free_count %d",
                                 dataShared0->instance_id,overlay_buf0,dataShared0->free_tail,dataShared0->free_count+1);
                            dataShared0->free_tail++;
                            dataShared0->free_tail = dataShared0->free_tail%MAX_OVERLAY_BUFFER_NUM;
                            dataShared0->free_count++;
                            if(dataShared0->free_count > dataShared0->num_buffer) {
                                OVERLAY_LOG_ERR("Error!free_count %d is greater the total number %d",
                                                dataShared0->free_count,dataShared0->num_buffer);
                            }
                        }

                        if(dataShared0->wait_buf_flag) {
                            dataShared0->wait_buf_flag = 0;
                            OVERLAY_LOG_RUNTIME("Id %d Condition signal for Overlay Instance 0",dataShared0->instance_id);
                            pthread_cond_signal(&dataShared0->free_cond);
                        }
                     }
                     overlay_buf0 = NULL;
                }

                //OVERLAY_LOG_INFO("queued_count %d", dataShared0->queued_count);

                if(dataShared0->queued_count > 0) 
                {
                    //fetch the head buffer in queued buffers
                    overlay_buf0 = dataShared0->queued_bufs[dataShared0->queued_head];
                    OVERLAY_LOG_RUNTIME("id %d Get queue buffer for Overlay Instance 0: 0x%x queued_count %d head %d",
                         dataShared0->instance_id,overlay_buf0,dataShared0->queued_count, dataShared0->queued_head);

                    if(dataShared0->overlay_mode == OVERLAY_NORAML_MODE) 
                    {
                        dataShared0->queued_bufs[dataShared0->queued_head] = 0;
                        dataShared0->queued_head ++;
                        dataShared0->queued_head = dataShared0->queued_head%MAX_OVERLAY_BUFFER_NUM;
                        dataShared0->queued_count --;
                    }
                    dataShared0->buf_mixing = true;
                }


                //Check whether output area and zorder changing occure, so 
                //to paint v4l2 buffer or setting fb0/1's local 
                //alpha buffer                 
                outchange0 = overlayObj0->out_changed || modeChange0;
                if(overlay_buf0) {
                    overlayObj0->out_changed = false;
                    modeChange0 = false;
                }

                if(outchange0)
                    calcOutputParam(m_dev, overlayObj0,  &rect_out0, &rotation0);

                overlay0_outregion.left = rect_out0.nLeft;
                overlay0_outregion.right = rect_out0.nLeft + rect_out0.nWidth;
                overlay0_outregion.top = rect_out0.nTop;
                overlay0_outregion.bottom = rect_out0.nTop + rect_out0.nHeight;
                
                crop_x0 = dataShared0->crop_x;
                crop_y0 = dataShared0->crop_y;  
                crop_w0 = dataShared0->crop_w;
                crop_h0 = dataShared0->crop_h;

                pthread_mutex_unlock(&dataShared0->obj_lock); 
            }

            if(overlayObj1) {
                dataShared1 = overlayObj1->mDataShared;
                OVERLAY_LOG_RUNTIME("Process obj 1 instance_id %d dataShared1 0x%x",
                                    dataShared1->instance_id,dataShared1);
                pthread_mutex_lock(&dataShared1->obj_lock);

                //If this overlay is under destroy, just return all buffers in queue
                //The user should guarantee the buffer all to be flushed before destroy it.
                if(dataShared1->in_destroy||(!overlayObj1->visible)) {
                     OVERLAY_LOG_INFO("Overlay in destroy or invisible,flush queue as queued_count %d,queued_head %d",
                     dataShared1->queued_count,dataShared1->queued_head);
                     while(dataShared1->queued_count > 0) {
                        //fetch the head buffer in queued buffers
                        overlay_buf1 = dataShared1->queued_bufs[dataShared1->queued_head];
                        OVERLAY_LOG_RUNTIME("id %d Get queue buffer for Overlay Instance 0: 0x%x queued_count %d",
                             dataShared1->instance_id,overlay_buf1,dataShared1->queued_count);
                        dataShared1->queued_bufs[dataShared1->queued_head] = 0;
                        dataShared1->queued_head ++;
                        dataShared1->queued_head = dataShared1->queued_head%MAX_OVERLAY_BUFFER_NUM;
                        dataShared1->queued_count --;

                        //For push mode, no need to return the buffer back
                        if(dataShared1->overlay_mode == OVERLAY_NORAML_MODE) {
                            dataShared1->free_bufs[dataShared1->free_tail] = overlay_buf1;
                            OVERLAY_LOG_RUNTIME("Id %d back buffer to free queue for Overlay Instance 0: 0x%x at %d free_count %d",
                                 dataShared1->instance_id,overlay_buf1,dataShared1->free_tail,dataShared1->free_count+1);
                            dataShared1->free_tail++;
                            dataShared1->free_tail = dataShared1->free_tail%MAX_OVERLAY_BUFFER_NUM;
                            dataShared1->free_count++;
                            if(dataShared1->free_count > dataShared1->num_buffer) {
                                OVERLAY_LOG_ERR("Error!free_count %d is greater the total number %d",
                                                dataShared1->free_count,dataShared1->num_buffer);
                            }
                        }

                        if(dataShared1->wait_buf_flag) {
                            dataShared1->wait_buf_flag = 0;
                            OVERLAY_LOG_RUNTIME("Id %d Condition signal for Overlay Instance 0",dataShared1->instance_id);
                            pthread_cond_signal(&dataShared1->free_cond);
                        }

                     }
                     overlay_buf1 = NULL;
                }


                //Fetch one buffer from each overlay instance buffer queue
                if(dataShared1->queued_count > 0) 
                {
                    //fetch the head buffer in queued buffers
                    overlay_buf1 = dataShared1->queued_bufs[dataShared1->queued_head];
                    OVERLAY_LOG_RUNTIME("Id %d Get queue buffer for Overlay Instance 1: 0x%x queued_count %d",
                         dataShared1->instance_id,overlay_buf1,dataShared1->queued_count);
                    if(dataShared1->overlay_mode == OVERLAY_NORAML_MODE) 
                    {
                        dataShared1->queued_bufs[dataShared1->queued_head] = 0;
                        dataShared1->queued_head ++;
                        dataShared1->queued_head = dataShared1->queued_head%MAX_OVERLAY_BUFFER_NUM;
                        dataShared1->queued_count --;
                    }
                    dataShared1->buf_mixing = true;
                }       

                //Check whether output area and zorder changing occure, so 
                //to paint v4l2 buffer or setting fb0/1's local 
                //alpha buffer                 
                outchange1 = overlayObj1->out_changed || modeChange1;
                if(overlay_buf1) {
                    overlayObj1->out_changed = false;
                    modeChange1 = false;
                }
                if(outchange1)
                    calcOutputParam(m_dev, overlayObj1,  &rect_out1, &rotation1);

                overlay1_outregion.left = rect_out1.nLeft;
                overlay1_outregion.right = rect_out1.nLeft + rect_out1.nWidth;
                overlay1_outregion.top = rect_out1.nTop;
                overlay1_outregion.bottom = rect_out1.nTop + rect_out1.nHeight;
                
                crop_x1 = dataShared1->crop_x;
                crop_y1 = dataShared1->crop_y;  
                crop_w1 = dataShared1->crop_w;
                crop_h1 = dataShared1->crop_h;
                pthread_mutex_unlock(&dataShared1->obj_lock); 
            }

            //All access to dataShare should be within control_lock, in case it has been destoried
            //If buf_mixing flag been set, do not release the overlay
            pthread_mutex_unlock(&m_dev->control_lock);

            if((!overlay_buf0)&&(!overlay_buf1)) {
                OVERLAY_LOG_RUNTIME("Nothing to do in overlay mixer thread!");
                //It is just a loop function in the loopless of this thread
                //So make a break here, and it will come back later
                continue;
            }
            if((overlay_buf0)&&(overlay_buf1)) {
                OVERLAY_LOG_RUNTIME("Two instance mixer needed");
            } 
                      
            //Check whether refill the origin area to black
            if(outchange0||outchange1) {
				DisplayChanged = 1;
				ClearBufMask |= DisplayBufMask;
				DisplayBufMask = 0;
                OVERLAY_LOG_RUNTIME("Mixer thread refill the origin area to black");
            }

            //Check whether need copy back the latest frame to current frame
            //If only buf0  is available, copy back the overlay1's area in v4l latest buffer after show buf0
            //to v4l current buffer
            //If only buf1  is available, copy back the overlay1's area in v4l latest buffer before show buf1
            //to v4l current buffer

            //Dequeue a V4L2 Buffer
            struct v4l2_buffer *pV4LBuf;
            struct v4l2_buffer v4lbuf;
            memset(&v4lbuf, 0, sizeof(v4l2_buffer));
            //OVERLAY_LOG_ERR("video_frames %d, v4l_bufcount %d", m_dev->video_frames, m_dev->v4l_bufcount);
            
            if(m_dev->video_frames < m_dev->v4l_bufcount) {
                pV4LBuf = &m_dev->v4l_buffers[m_dev->video_frames];
            }
            else{
                v4lbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
                v4lbuf.memory = V4L2_MEMORY_MMAP;
                if(ioctl(m_dev->v4l_id, VIDIOC_DQBUF, &v4lbuf) < 0){
                    OVERLAY_LOG_ERR("Error!Cannot DQBUF a buffer from v4l");

                    //stream off it,so to make it recover
                    int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
                    ioctl(m_dev->v4l_id, VIDIOC_STREAMOFF, &type);
                    m_dev->stream_on = false;
                    m_dev->video_frames = 0;
                    //reset latest queued buffer, since all buffer will start as init
                    memset(&mLatestQueuedBuf,0,sizeof(mLatestQueuedBuf));
                    goto free_buf_exit;
                }
                pV4LBuf = &v4lbuf;
            }

            {	//record the displayed mask
                unsigned int CurBufMask = 1 << (pV4LBuf->index);
                DisplayBufMask |= CurBufMask;
                if(DisplayChanged == 1){
                    if ((ClearBufMask & CurBufMask) != 0){
                        fill_frame_back(m_dev->v4lbuf_addr[pV4LBuf->index], pV4LBuf->length, m_dev->xres, m_dev->yres, m_dev->outpixelformat);
                        ClearBufMask &= ~CurBufMask;
                    }
                    if (ClearBufMask == 0)
                        DisplayChanged = 0;
                }
            }

            OVERLAY_LOG_RUNTIME("DQBUF from v4l 0x%x:index %d, vir 0x%x, phy 0x%x, len %d",
                                pV4LBuf,pV4LBuf->index,m_dev->v4lbuf_addr[pV4LBuf->index],
                                pV4LBuf->m.offset,pV4LBuf->length);   
                      
            //Copyback before ipu update when two active instance, and obj0 has no buffer to update
            //obj1 has buffer to update
            //Workaround, since dataShared0 may be released
            if((overlayObj0)&&(overlayObj1)&&
               (!overlay_buf0)&&(overlay_buf1)/*&&
               (dataShared0->buf_showed)*/)
            {
                OVERLAY_LOG_RUNTIME("Copyback before ipu update");
                if((mLatestQueuedBuf.m.offset)&&
                   (mLatestQueuedBuf.m.offset != pV4LBuf->m.offset)) {
                    //Copy back the region of obj0 to the current v4l buffer
                    //Setting input format
                    mIPUInputParam.width = m_dev->xres;
                    mIPUInputParam.height = m_dev->yres;

                    mIPUInputParam.input_crop_win.pos.x = overlay0_outregion.left;
                    mIPUInputParam.input_crop_win.pos.y = overlay0_outregion.top;  
                    mIPUInputParam.input_crop_win.win_w = overlay0_outregion.right - overlay0_outregion.left;
                    mIPUInputParam.input_crop_win.win_h = overlay0_outregion.bottom - overlay0_outregion.top;
                    mIPUInputParam.fmt = m_dev->outpixelformat;
                    mIPUInputParam.user_def_paddr[0] = mLatestQueuedBuf.m.offset;

                    //Setting output format
                    //Should align with v4l
                    mIPUOutputParam.fmt = m_dev->outpixelformat;
                    mIPUOutputParam.width = m_dev->xres;
                    mIPUOutputParam.height = m_dev->yres;   
                    mIPUOutputParam.show_to_fb = 0;
                    //Output param should be same as input, since no resize,crop
                    mIPUOutputParam.output_win.pos.x = mIPUInputParam.input_crop_win.pos.x;
                    mIPUOutputParam.output_win.pos.y = mIPUInputParam.input_crop_win.pos.y;
                    mIPUOutputParam.output_win.win_w = mIPUInputParam.input_crop_win.win_w;
                    mIPUOutputParam.output_win.win_h = mIPUInputParam.input_crop_win.win_h;
                    mIPUOutputParam.rot = 0;
                    mIPUOutputParam.user_def_paddr[0] = pV4LBuf->m.offset;
                    OVERLAY_LOG_RUNTIME("Copyback(before) Output param: width %d,height %d, pos.x %d, pos.y %d,win_w %d,win_h %d,rot %d",
                          mIPUOutputParam.width,
                          mIPUOutputParam.height,
                          mIPUOutputParam.output_win.pos.x,
                          mIPUOutputParam.output_win.pos.y,
                          mIPUOutputParam.output_win.win_w,
                          mIPUOutputParam.output_win.win_h,
                          mIPUOutputParam.rot);
                                     
                    OVERLAY_LOG_RUNTIME("Copyback(before) Input param: width %d, height %d, fmt %d, crop_win pos x %d, crop_win pos y %d, crop_win win_w %d,crop_win win_h %d",
                                     mIPUInputParam.width,
                                     mIPUInputParam.height,
                                     mIPUInputParam.fmt,
                                     mIPUInputParam.input_crop_win.pos.x,
                                     mIPUInputParam.input_crop_win.pos.y,
                                     mIPUInputParam.input_crop_win.win_w,
                                     mIPUInputParam.input_crop_win.win_h);     
    
                    mIPURet =  mxc_ipu_lib_task_init(&mIPUInputParam,NULL,&mIPUOutputParam,OP_NORMAL_MODE|TASK_PP_MODE,&mIPUHandle);
                    if (mIPURet < 0) {
                       OVERLAY_LOG_ERR("Error!Copyback(before) mxc_ipu_lib_task_init failed mIPURet %d!",mIPURet);
                       goto queue_buf_exit;
                    }  
                    OVERLAY_LOG_RUNTIME("Copyback(before) mxc_ipu_lib_task_init success");
                    mIPURet = mxc_ipu_lib_task_buf_update(&mIPUHandle,overlay_buf0,pV4LBuf->m.offset,NULL,NULL,NULL);
                    if (mIPURet < 0) {
                          OVERLAY_LOG_ERR("Error!Copyback(before) mxc_ipu_lib_task_buf_update failed mIPURet %d!",mIPURet);
                          mxc_ipu_lib_task_uninit(&mIPUHandle);
                          memset(&mIPUHandle, 0, sizeof(ipu_lib_handle_t));
                          goto queue_buf_exit;
                    }
                    OVERLAY_LOG_RUNTIME("Copyback(before) mxc_ipu_lib_task_buf_update success");
                    mxc_ipu_lib_task_uninit(&mIPUHandle);
                    memset(&mIPUHandle, 0, sizeof(ipu_lib_handle_t));
                }
                else{
                    OVERLAY_LOG_ERR("Error!Cannot Copyback before ipu update last buf 0x%x,curr buf 0x%x",
                                    mLatestQueuedBuf.m.offset,
                                    pV4LBuf->m.offset);
                }
            }

            //Mixer the first buffer from overlay instance0 to V4L2 Buffer
            if(overlay_buf0 && (overlayObj0->outX != 0  ||
                     overlayObj0->outY != 0  ||
                     overlayObj0->outW != 0  ||
                     overlayObj0->outH != 0)) {
                //Setting input format
                mIPUInputParam.width = overlayObj0->mHandle.width;
                mIPUInputParam.height = overlayObj0->mHandle.height;
                mIPUInputParam.input_crop_win.pos.x = crop_x0;
                mIPUInputParam.input_crop_win.pos.y = crop_y0;  
                mIPUInputParam.input_crop_win.win_w = crop_w0;
                mIPUInputParam.input_crop_win.win_h = crop_h0;

                if(overlayObj0->mHandle.format == HAL_PIXEL_FORMAT_YCbCr_420_SP) {
                    mIPUInputParam.fmt = v4l2_fourcc('N', 'V', '1', '2');
                }
                else if(overlayObj0->mHandle.format == HAL_PIXEL_FORMAT_YCbCr_420_I) {
                    mIPUInputParam.fmt = v4l2_fourcc('I', '4', '2', '0');
                }
                else if(overlayObj0->mHandle.format == HAL_PIXEL_FORMAT_YCbCr_422_I) {
                    mIPUInputParam.fmt = v4l2_fourcc('4', '2', '2', 'P');
                }
                else if(overlayObj0->mHandle.format == HAL_PIXEL_FORMAT_RGB_565) {
                    mIPUInputParam.fmt = v4l2_fourcc('R', 'G', 'B', 'P');
                }else{
                    OVERLAY_LOG_ERR("Error!Not supported input format %d",overlayObj0->mHandle.format);
                    goto queue_buf_exit;
                }
                mIPUInputParam.user_def_paddr[0] = overlay_buf0;

                //Setting output format
                //Should align with v4l
                mIPUOutputParam.fmt = m_dev->outpixelformat;
                mIPUOutputParam.width = m_dev->xres;
                mIPUOutputParam.height = m_dev->yres;   
                mIPUOutputParam.show_to_fb = 0;
                mIPUOutputParam.output_win.pos.x = rect_out0.nLeft;
                mIPUOutputParam.output_win.pos.y = rect_out0.nTop;
                mIPUOutputParam.output_win.win_w =rect_out0.nWidth;
                mIPUOutputParam.output_win.win_h = rect_out0.nHeight;
                mIPUOutputParam.rot = rotation0;
                mIPUOutputParam.user_def_paddr[0] = pV4LBuf->m.offset;
                OVERLAY_LOG_RUNTIME("Obj0 Output param: width %d,height %d, pos.x %d, pos.y %d,win_w %d,win_h %d,rot %d, paddr 0x%x",
                      mIPUOutputParam.width,
                      mIPUOutputParam.height,
                      mIPUOutputParam.output_win.pos.x,
                      mIPUOutputParam.output_win.pos.y,
                      mIPUOutputParam.output_win.win_w,
                      mIPUOutputParam.output_win.win_h,
                      mIPUOutputParam.rot,
                      mIPUOutputParam.user_def_paddr[0]);
                                 
                OVERLAY_LOG_RUNTIME("Obj0 Input param: width %d, height %d, fmt %d, crop_win pos x %d, crop_win pos y %d, crop_win win_w %d,crop_win win_h %d",
                                 mIPUInputParam.width,
                                 mIPUInputParam.height,
                                 mIPUInputParam.fmt,
                                 mIPUInputParam.input_crop_win.pos.x,
                                 mIPUInputParam.input_crop_win.pos.y,
                                 mIPUInputParam.input_crop_win.win_w,
                                 mIPUInputParam.input_crop_win.win_h);     

                mIPURet =  mxc_ipu_lib_task_init(&mIPUInputParam,NULL,&mIPUOutputParam,OP_NORMAL_MODE|TASK_PP_MODE,&mIPUHandle);
                if (mIPURet < 0) {
                   OVERLAY_LOG_ERR("Error!Obj0 mxc_ipu_lib_task_init failed mIPURet %d!",mIPURet);
                   goto queue_buf_exit;
                }  
                OVERLAY_LOG_RUNTIME("Obj0 mxc_ipu_lib_task_init success");
                mIPURet = mxc_ipu_lib_task_buf_update(&mIPUHandle,overlay_buf0,pV4LBuf->m.offset,NULL,NULL,NULL);
                if (mIPURet < 0) {
                      OVERLAY_LOG_ERR("Error!Obj0 mxc_ipu_lib_task_buf_update failed mIPURet %d!",mIPURet);
                      mxc_ipu_lib_task_uninit(&mIPUHandle);
                      memset(&mIPUHandle, 0, sizeof(ipu_lib_handle_t));
                      goto queue_buf_exit;
                }
                OVERLAY_LOG_RUNTIME("Obj0 mxc_ipu_lib_task_buf_update success");
                mxc_ipu_lib_task_uninit(&mIPUHandle);

                if (m_dev->video_play_mode == DUAL_VIDEO_SIN_UI && m_dev->sec_disp_enable) {
                    sec_video_mIPURet = mxc_ipu_lib_task_buf_update(&sec_video_mIPUHandle,overlay_buf0,NULL,NULL,NULL,NULL);
                    if (sec_video_mIPURet < 0) {
                          OVERLAY_LOG_ERR("Error!Obj0 second video mxc_ipu_lib_task_buf_update failed mIPURet %d!",sec_video_mIPURet);
                          mxc_ipu_lib_task_uninit(&sec_video_mIPUHandle);
                          memset(&sec_video_mIPUHandle, 0, sizeof(ipu_lib_handle_t));
                          goto queue_buf_exit;
                    }
                    OVERLAY_LOG_RUNTIME("sec video mxc_ipu_lib_task_buf_update success");
                }
                memset(&mIPUHandle, 0, sizeof(ipu_lib_handle_t));
            }

            //Check whether we need to do another mixer, based on
            //buffers in overlay instance1's buffer queue
            if(overlay_buf1 && (overlayObj1->outX != 0  ||
                     overlayObj1->outY != 0  ||
                     overlayObj1->outW != 0  ||
                     overlayObj1->outH != 0)) {
                //Setting input format
                mIPUInputParam.width = overlayObj1->mHandle.width;
                mIPUInputParam.height = overlayObj1->mHandle.height;
                mIPUInputParam.input_crop_win.pos.x = crop_x1;
                mIPUInputParam.input_crop_win.pos.y = crop_y1;  
                mIPUInputParam.input_crop_win.win_w = crop_w1;
                mIPUInputParam.input_crop_win.win_h = crop_h1;


                if(overlayObj1->mHandle.format == HAL_PIXEL_FORMAT_YCbCr_420_SP) {
                    mIPUInputParam.fmt = v4l2_fourcc('N', 'V', '1', '2');
                }
                else if(overlayObj1->mHandle.format == HAL_PIXEL_FORMAT_YCbCr_420_I) {
                    mIPUInputParam.fmt = v4l2_fourcc('I', '4', '2', '0');
                }
                else if(overlayObj0->mHandle.format == HAL_PIXEL_FORMAT_YCbCr_422_I) {
                    mIPUInputParam.fmt = v4l2_fourcc('4', '2', '2', 'P');
                }
                else if(overlayObj0->mHandle.format == HAL_PIXEL_FORMAT_RGB_565) {
                    mIPUInputParam.fmt = v4l2_fourcc('R', 'G', 'B', 'P');
                }else{
                    OVERLAY_LOG_ERR("Error!Obj1 Not supported input format %d",overlayObj1->mHandle.format);
                    goto queue_buf_exit;
                }
                mIPUInputParam.user_def_paddr[0] = overlay_buf1;

                //Setting output format
                mIPUOutputParam.fmt = v4l2_fourcc('U', 'Y', 'V', 'Y');
                mIPUOutputParam.width = m_dev->xres;
                mIPUOutputParam.height = m_dev->yres;   
                mIPUOutputParam.show_to_fb = 0;
                mIPUOutputParam.output_win.pos.x = rect_out1.nLeft;
                mIPUOutputParam.output_win.pos.y = rect_out1.nTop;
                mIPUOutputParam.output_win.win_w =rect_out1.nWidth;
                mIPUOutputParam.output_win.win_h = rect_out1.nHeight;
                mIPUOutputParam.rot = rotation1;
                mIPUOutputParam.user_def_paddr[0] = pV4LBuf->m.offset;
                OVERLAY_LOG_RUNTIME("Obj1 Output param: width %d,height %d, pos.x %d, pos.y %d,win_w %d,win_h %d,rot %d",
                      mIPUOutputParam.width,
                      mIPUOutputParam.height,
                      mIPUOutputParam.output_win.pos.x,
                      mIPUOutputParam.output_win.pos.y,
                      mIPUOutputParam.output_win.win_w,
                      mIPUOutputParam.output_win.win_h,
                      mIPUOutputParam.rot);
                                 
                OVERLAY_LOG_RUNTIME("Obj1 Input param:width %d,height %d,fmt %d,crop_win pos x %d,crop_win pos y %d,crop_win win_w %d,crop_win win_h %d",
                                 mIPUInputParam.width,
                                 mIPUInputParam.height,
                                 mIPUInputParam.fmt,
                                 mIPUInputParam.input_crop_win.pos.x,
                                 mIPUInputParam.input_crop_win.pos.y,
                                 mIPUInputParam.input_crop_win.win_w,
                                 mIPUInputParam.input_crop_win.win_h);     

                mIPURet =  mxc_ipu_lib_task_init(&mIPUInputParam,NULL,&mIPUOutputParam,OP_NORMAL_MODE|TASK_PP_MODE,&mIPUHandle);
                if (mIPURet < 0) {
                   OVERLAY_LOG_ERR("Error!Obj1 mxc_ipu_lib_task_init failed mIPURet %d!",mIPURet);
                   goto queue_buf_exit;
                }  
                OVERLAY_LOG_RUNTIME("Obj1 mxc_ipu_lib_task_init success");
                mIPURet = mxc_ipu_lib_task_buf_update(&mIPUHandle,overlay_buf1,pV4LBuf->m.offset,NULL,NULL,NULL);
                if (mIPURet < 0) {
                      OVERLAY_LOG_ERR("Error!Obj1 mxc_ipu_lib_task_buf_update failed mIPURet %d!",mIPURet);
                      mxc_ipu_lib_task_uninit(&mIPUHandle);
                      memset(&mIPUHandle, 0, sizeof(ipu_lib_handle_t));
                      goto queue_buf_exit;
                }
                OVERLAY_LOG_RUNTIME("Obj1 mxc_ipu_lib_task_buf_update success");
                mxc_ipu_lib_task_uninit(&mIPUHandle);
                memset(&mIPUHandle, 0, sizeof(ipu_lib_handle_t));
            }
        
            //Copyback after ipu update when two active instance, and obj0 has one buffer to update
            //obj0 has no buffer to update
            //Workaround, since dataShared0 may be released
            if((overlayObj0)&&(overlayObj1)&&
               (overlay_buf0)&&(!overlay_buf1)/*&&
               (dataShared1->buf_showed)*/)
            {
                OVERLAY_LOG_RUNTIME("Copyback after ipu update");
                if((mLatestQueuedBuf.m.offset)&&
                   (mLatestQueuedBuf.m.offset != pV4LBuf->m.offset)) {
                    //Copy back the region of obj0 to the current v4l buffer
                    //Setting input format
                    mIPUInputParam.width = m_dev->xres;
                    mIPUInputParam.height = m_dev->yres;

                    mIPUInputParam.input_crop_win.pos.x = overlay1_outregion.left;
                    mIPUInputParam.input_crop_win.pos.y = overlay1_outregion.top;  
                    mIPUInputParam.input_crop_win.win_w = overlay1_outregion.right - overlay1_outregion.left;
                    mIPUInputParam.input_crop_win.win_h = overlay1_outregion.bottom - overlay1_outregion.top;
                    mIPUInputParam.fmt = m_dev->outpixelformat;
                    mIPUInputParam.user_def_paddr[0] = mLatestQueuedBuf.m.offset;
    
                    //Setting output format
                    //Should align with v4l
                    mIPUOutputParam.fmt = m_dev->outpixelformat;
                    mIPUOutputParam.width = m_dev->xres;
                    mIPUOutputParam.height = m_dev->yres;   
                    mIPUOutputParam.show_to_fb = 0;
                    //Output param should be same as input, since no resize,crop
                    mIPUOutputParam.output_win.pos.x = mIPUInputParam.input_crop_win.pos.x;
                    mIPUOutputParam.output_win.pos.y = mIPUInputParam.input_crop_win.pos.y;
                    mIPUOutputParam.output_win.win_w = mIPUInputParam.input_crop_win.win_w;
                    mIPUOutputParam.output_win.win_h = mIPUInputParam.input_crop_win.win_h;
                    mIPUOutputParam.rot = 0;
                    mIPUOutputParam.user_def_paddr[0] = pV4LBuf->m.offset;
                    OVERLAY_LOG_RUNTIME("Copyback(after) Output param: width %d,height %d, pos.x %d, pos.y %d,win_w %d,win_h %d,rot %d",
                          mIPUOutputParam.width,
                          mIPUOutputParam.height,
                          mIPUOutputParam.output_win.pos.x,
                          mIPUOutputParam.output_win.pos.y,
                          mIPUOutputParam.output_win.win_w,
                          mIPUOutputParam.output_win.win_h,
                          mIPUOutputParam.rot);
                                     
                    OVERLAY_LOG_RUNTIME("Copyback(after) Input param: width %d, height %d, fmt %d, crop_win pos x %d, crop_win pos y %d, crop_win win_w %d,crop_win win_h %d",
                                     mIPUInputParam.width,
                                     mIPUInputParam.height,
                                     mIPUInputParam.fmt,
                                     mIPUInputParam.input_crop_win.pos.x,
                                     mIPUInputParam.input_crop_win.pos.y,
                                     mIPUInputParam.input_crop_win.win_w,
                                     mIPUInputParam.input_crop_win.win_h);     
    
                    mIPURet =  mxc_ipu_lib_task_init(&mIPUInputParam,NULL,&mIPUOutputParam,OP_NORMAL_MODE|TASK_PP_MODE,&mIPUHandle);
                    if (mIPURet < 0) {
                       OVERLAY_LOG_ERR("Error!Copyback(after) mxc_ipu_lib_task_init failed mIPURet %d!",mIPURet);
                       goto queue_buf_exit;
                    }  
                    OVERLAY_LOG_RUNTIME("Copyback(after) mxc_ipu_lib_task_init success");
                    mIPURet = mxc_ipu_lib_task_buf_update(&mIPUHandle,overlay_buf0,pV4LBuf->m.offset,NULL,NULL,NULL);
                    if (mIPURet < 0) {
                          OVERLAY_LOG_ERR("Error!Copyback(after) mxc_ipu_lib_task_buf_update failed mIPURet %d!",mIPURet);
                          mxc_ipu_lib_task_uninit(&mIPUHandle);
                          memset(&mIPUHandle, 0, sizeof(ipu_lib_handle_t));
                          goto queue_buf_exit;
                    }
                    OVERLAY_LOG_RUNTIME("Copyback(after) mxc_ipu_lib_task_buf_update success");
                    mxc_ipu_lib_task_uninit(&mIPUHandle);
                    memset(&mIPUHandle, 0, sizeof(ipu_lib_handle_t));
                }
                else{
                    OVERLAY_LOG_ERR("Error!Cannot Copyback before ipu update last buf 0x%x,curr buf 0x%x",
                                    mLatestQueuedBuf.m.offset,
                                    pV4LBuf->m.offset);
                }
            }

queue_buf_exit:
            //Queue the mixed V4L2 Buffer for display
            gettimeofday(&pV4LBuf->timestamp, 0);
            if(ioctl(m_dev->v4l_id, VIDIOC_QBUF, pV4LBuf) < 0){
                OVERLAY_LOG_ERR("Error!Cannot QBUF a buffer from v4l");
                //reset latest queued buffer, since all buffer will start as init
                memset(&mLatestQueuedBuf,0,sizeof(mLatestQueuedBuf));
                goto free_buf_exit;
            }
            OVERLAY_LOG_RUNTIME("QBUF from v4l at frame %d:index %d, phy 0x%x at sec %d usec %d",
                                m_dev->video_frames,pV4LBuf->index,pV4LBuf->m.offset,
                                pV4LBuf->timestamp.tv_sec,pV4LBuf->timestamp.tv_usec); 

            //record the latest buffer we queued
            memcpy(&mLatestQueuedBuf,pV4LBuf,sizeof(mLatestQueuedBuf));
            
            //Only stream on after two frames queued 
            m_dev->video_frames++;
            if((m_dev->video_frames>=2)&&(!m_dev->stream_on)) {
                int type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
                ioctl(m_dev->v4l_id, VIDIOC_STREAMON, &type);
                m_dev->stream_on = true;
                OVERLAY_LOG_INFO("V4L STREAMON NOW");
            }

free_buf_exit:
            //push back the buffer to overlay instance0 freequeue
            //signal instance condition if wait flag is true
            //reset wait flag
            //pthread_cond_signal(&cond);  
            if((overlayObj0)&&(overlay_buf0)) {
                pthread_mutex_lock(&dataShared0->obj_lock);
                dataShared0->buf_showed++;
                //For push mode, no need to return the buffer back
                if(dataShared0->overlay_mode == OVERLAY_NORAML_MODE) {
                    dataShared0->free_bufs[dataShared0->free_tail] = overlay_buf0;
                    OVERLAY_LOG_RUNTIME("Id %d back buffer to free queue for Overlay Instance 0: 0x%x at %d free_count %d",
                         dataShared0->instance_id,overlay_buf0,dataShared0->free_tail,dataShared0->free_count+1);
                    dataShared0->free_tail++;
                    dataShared0->free_tail = dataShared0->free_tail%MAX_OVERLAY_BUFFER_NUM;
                    dataShared0->free_count++;
                    if(dataShared0->free_count > dataShared0->num_buffer) {
                        OVERLAY_LOG_ERR("Error!free_count %d is greater the total number %d",
                                        dataShared0->free_count,dataShared0->num_buffer);
                    }
                }

                if(dataShared0->wait_buf_flag) {
                    dataShared0->wait_buf_flag = 0;
                    OVERLAY_LOG_RUNTIME("Id %d Condition signal for Overlay Instance 0",dataShared0->instance_id);
                    pthread_cond_signal(&dataShared0->free_cond);
                }
                dataShared0->buf_mixing = false;

                if(dataShared0->overlay_mode == OVERLAY_PUSH_MODE) 
                    dataShared0->buf_displayed[dataShared0->queued_head] = 1;

                pthread_mutex_unlock(&dataShared0->obj_lock);
            }

            //push back the buffer of overlay instance1 freequeue
            //signal instance condition if wait flag is true
            //reset wait flag
            //pthread_cond_signal(&cond);  
            if((overlayObj1)&&(overlay_buf1)) {
                pthread_mutex_lock(&dataShared1->obj_lock);
                dataShared1->buf_showed++;
                //For push mode, no need to return the buffer back
                if(dataShared1->overlay_mode == OVERLAY_NORAML_MODE) {
                    dataShared1->free_bufs[dataShared1->free_tail] = overlay_buf1;
                    dataShared1->free_tail++;
                    dataShared1->free_tail = dataShared1->free_tail%MAX_OVERLAY_BUFFER_NUM;
                    dataShared1->free_count++;
                    OVERLAY_LOG_RUNTIME("Id %d back buffer to free queue for Overlay Instance 0: 0x%x free_count %d",
                         dataShared1->instance_id,overlay_buf1,dataShared1->free_count);
                }

                if(dataShared1->wait_buf_flag) {
                    dataShared1->wait_buf_flag = 0;
                    OVERLAY_LOG_RUNTIME("Id %d Condition signal for Overlay Instance 1",
                                        dataShared1->instance_id);
                    pthread_cond_signal(&dataShared1->free_cond);
                }
                dataShared1->buf_mixing = false;

                if(dataShared1->overlay_mode == OVERLAY_PUSH_MODE) 
                    dataShared1->buf_displayed[dataShared1->queued_head] = 1;

                pthread_mutex_unlock(&dataShared1->obj_lock);
            }
            
        }
        // loop until we need to quit
        return true;
    }






