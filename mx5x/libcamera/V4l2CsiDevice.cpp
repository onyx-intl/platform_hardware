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

/*
 * Copyright 2009-2011 Freescale Semiconductor, Inc. All Rights Reserved.
 */
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/time.h>
#include <linux/videodev.h>
#include <linux/videodev2.h>
#include <linux/mxc_v4l2.h>
#include <linux/mxcfb.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <utils/threads.h>
#include <dirent.h>

#include "V4l2CsiDevice.h"

namespace android{
    V4l2CsiDevice :: V4l2CsiDevice(){
        mSupportedFmt[0] = v4l2_fourcc('N','V','1','2');
        mSupportedFmt[1] = v4l2_fourcc('Y','U','1','2');
        mSupportedFmt[2] = v4l2_fourcc('Y','U','Y','V');

    }
    V4l2CsiDevice :: ~V4l2CsiDevice()
    {
    }


    CAPTURE_DEVICE_ERR_RET V4l2CsiDevice :: V4l2Open(){
        CAMERA_HAL_LOG_FUNC;
        int fd = 0, i, j, is_found = 0;
        const char *flags[] = {"uncompressed", "compressed"};

        char	dev_node[CAMAERA_FILENAME_LENGTH];
        DIR *v4l_dir = NULL;
        struct dirent *dir_entry;
        struct v4l2_dbg_chip_ident vid_chip;
        struct v4l2_fmtdesc vid_fmtdesc;
        struct v4l2_frmsizeenum vid_frmsize;
        CAPTURE_DEVICE_ERR_RET ret = CAPTURE_DEVICE_ERR_NONE;

        if(mCameraDevice > 0)
            return CAPTURE_DEVICE_ERR_ALRADY_OPENED;
        else if (mCaptureDeviceName[0] != '#'){
            CAMERA_HAL_LOG_RUNTIME("already get the device name %s", mCaptureDeviceName);
            mCameraDevice = open(mCaptureDeviceName, O_RDWR, O_NONBLOCK);
            if (mCameraDevice < 0)
                return CAPTURE_DEVICE_ERR_OPEN;
        }
        else{
            CAMERA_HAL_LOG_RUNTIME("deviceName is %s", mInitalDeviceName);
            v4l_dir = opendir("/sys/class/video4linux");
            if (v4l_dir){
                while((dir_entry = readdir(v4l_dir))) {
                    memset((void *)dev_node, 0, CAMAERA_FILENAME_LENGTH);
                    if(strncmp(dir_entry->d_name, "video", 5)) 
                        continue;
                    sprintf(dev_node, "/dev/%s", dir_entry->d_name);
                    if ((fd = open(dev_node, O_RDWR, O_NONBLOCK)) < 0)
                        continue;
                    CAMERA_HAL_LOG_RUNTIME("dev_node is %s", dev_node);
                    if(ioctl(fd, VIDIOC_DBG_G_CHIP_IDENT, &vid_chip) < 0 ) {
                        close(fd);
                        continue;
                    } else if (strstr(vid_chip.match.name, mInitalDeviceName) != 0) {
                        is_found = 1;
                        strcpy(mCaptureDeviceName, dev_node);
                        strcpy(mInitalDeviceName, vid_chip.match.name);
                        CAMERA_HAL_LOG_INFO("device name is %s", mCaptureDeviceName);
                        CAMERA_HAL_LOG_INFO("sensor name is %s", mInitalDeviceName);
                        break;
                    } else{
                        close(fd);
                        fd = 0;
                    }
                }
            }
            if (fd > 0)
                mCameraDevice = fd;
            else{
                CAMERA_HAL_ERR("The device name is not correct or the device is error");
                return CAPTURE_DEVICE_ERR_OPEN;
            }
        }
        return ret; 
    }

    CAPTURE_DEVICE_ERR_RET V4l2CsiDevice :: V4l2EnumFmt(void *retParam){
        CAMERA_HAL_LOG_FUNC;
        CAPTURE_DEVICE_ERR_RET ret = CAPTURE_DEVICE_ERR_NONE; 
        unsigned int *pParamVal = (unsigned int *)retParam;

        if (mFmtParamIdx < ENUM_SUPPORTED_FMT){
            CAMERA_HAL_LOG_RUNTIME("vid_fmtdesc.pixelformat is %x", mSupportedFmt[mFmtParamIdx]);
            *pParamVal = mSupportedFmt[mFmtParamIdx];
            mFmtParamIdx ++;
            ret = CAPTURE_DEVICE_ERR_ENUM_CONTINUE;
        }else{
            mFmtParamIdx = 0;
            ret = CAPTURE_DEVICE_ERR_GET_PARAM;
        }
        return ret;
    }

    CAPTURE_DEVICE_ERR_RET V4l2CsiDevice :: V4l2EnumSizeFps(void *retParam){
        CAMERA_HAL_LOG_FUNC;
        CAPTURE_DEVICE_ERR_RET ret = CAPTURE_DEVICE_ERR_NONE; 
        struct v4l2_frmsizeenum vid_frmsize;

        struct capture_config_t *pCapCfg =(struct capture_config_t *) retParam;
        memset(&vid_frmsize, 0, sizeof(struct v4l2_frmsizeenum));
        vid_frmsize.index = mSizeFPSParamIdx;
        CAMERA_HAL_LOG_RUNTIME("the query for size fps fmt is %x",pCapCfg->fmt);
        vid_frmsize.pixel_format = pCapCfg->fmt;
        if (ioctl(mCameraDevice, VIDIOC_ENUM_FRAMESIZES, &vid_frmsize) != 0){
            mSizeFPSParamIdx = 0;
            ret = CAPTURE_DEVICE_ERR_SET_PARAM;
        }else{
            //hardcode here for ov3640
            if (strstr(mInitalDeviceName, "3640") != NULL){
                CAMERA_HAL_LOG_INFO("the sensor  is  mInitalDeviceName");
                if (vid_frmsize.discrete.width == 1024 && vid_frmsize.discrete.height == 768){
                    mSizeFPSParamIdx ++;
                    vid_frmsize.index = mSizeFPSParamIdx;
                    if (ioctl(mCameraDevice, VIDIOC_ENUM_FRAMESIZES, &vid_frmsize) != 0){
                        mSizeFPSParamIdx = 0;
                        ret = CAPTURE_DEVICE_ERR_SET_PARAM;
                    }
                }
            }
            CAMERA_HAL_LOG_RUNTIME("in %s the w %d, h %d", __FUNCTION__,vid_frmsize.discrete.width, vid_frmsize.discrete.height);
            pCapCfg->width  = vid_frmsize.discrete.width;
            pCapCfg->height = vid_frmsize.discrete.height;
            if(vid_frmsize.discrete.width > 1280 || vid_frmsize.discrete.height >720){
                pCapCfg->tv.numerator = 1;
                pCapCfg->tv.denominator = 15;
            }else{
                pCapCfg->tv.numerator = 1;
                pCapCfg->tv.denominator = 30;
            }
            mSizeFPSParamIdx ++;
            ret = CAPTURE_DEVICE_ERR_ENUM_CONTINUE;
        }
        return ret;
    }

    CAPTURE_DEVICE_ERR_RET V4l2CsiDevice :: V4l2ConfigInput(struct capture_config_t *pCapcfg)
    {
        CAMERA_HAL_LOG_FUNC;
        int input = 1;
        if (ioctl(mCameraDevice, VIDIOC_S_INPUT, &input) < 0) {
            CAMERA_HAL_ERR("set input failed");
            return CAPTURE_DEVICE_ERR_SYS_CALL;
        }
        return CAPTURE_DEVICE_ERR_NONE;
    }


    CAPTURE_DEVICE_ERR_RET V4l2CsiDevice :: V4l2SetConfig(struct capture_config_t *pCapcfg)
    {

        CAMERA_HAL_LOG_FUNC;
        if (mCameraDevice <= 0 || pCapcfg == NULL){
            return CAPTURE_DEVICE_ERR_BAD_PARAM;
        }

        CAPTURE_DEVICE_ERR_RET ret = CAPTURE_DEVICE_ERR_NONE;
        struct v4l2_format fmt;
        struct v4l2_control ctrl;
        struct v4l2_streamparm parm;

        V4l2ConfigInput(pCapcfg);

        parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        //hard code here to do a walk around.
        if(pCapcfg->tv.denominator != 30 && pCapcfg->tv.denominator != 15){
            pCapcfg->tv.numerator = 1;
            pCapcfg->tv.denominator = 30;
        }
        CAMERA_HAL_LOG_RUNTIME("the fps is %d", pCapcfg->tv.denominator);

        parm.parm.capture.timeperframe.numerator = pCapcfg->tv.numerator;
        parm.parm.capture.timeperframe.denominator = pCapcfg->tv.denominator;
        ret = V4l2GetCaptureMode(pCapcfg, &(parm.parm.capture.capturemode));
        if (ret != CAPTURE_DEVICE_ERR_NONE)
            return ret;

        if (ioctl(mCameraDevice, VIDIOC_S_PARM, &parm) < 0) {
            parm.parm.capture.timeperframe.numerator = 1;
            parm.parm.capture.timeperframe.denominator = 15;
            if (ioctl(mCameraDevice, VIDIOC_S_PARM, &parm) < 0){
                CAMERA_HAL_ERR("%s:%d  VIDIOC_S_PARM failed\n", __FUNCTION__,__LINE__);
                CAMERA_HAL_ERR("frame timeval is numerator %d, denominator %d",parm.parm.capture.timeperframe.numerator, 
                        parm.parm.capture.timeperframe.denominator);
                return CAPTURE_DEVICE_ERR_SYS_CALL;
            }
        }


        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.pixelformat = pCapcfg->fmt;

        fmt.fmt.pix.width = pCapcfg->width&0xFFFFFFF8;
        fmt.fmt.pix.height = pCapcfg->height&0xFFFFFFF8;
        if (pCapcfg->fmt == V4L2_PIX_FMT_YUYV)
            fmt.fmt.pix.bytesperline = fmt.fmt.pix.width * 2;
        else
            fmt.fmt.pix.bytesperline = fmt.fmt.pix.width;
        fmt.fmt.pix.priv = 0;
        fmt.fmt.pix.sizeimage = 0;

        if (ioctl(mCameraDevice, VIDIOC_S_FMT, &fmt) < 0) {
            CAMERA_HAL_ERR("set format failed\n");
            CAMERA_HAL_ERR("pCapcfg->width is %d, pCapcfg->height is %d", pCapcfg->width, pCapcfg->height);
            CAMERA_HAL_ERR(" Set the Format :%c%c%c%c\n",
                    pCapcfg->fmt & 0xFF, (pCapcfg->fmt >> 8) & 0xFF,
                    (pCapcfg->fmt >> 16) & 0xFF, (pCapcfg->fmt >> 24) & 0xFF);
            return CAPTURE_DEVICE_ERR_SYS_CALL;
        }

        if(V4l2SetRot(pCapcfg) < 0)
            return CAPTURE_DEVICE_ERR_SYS_CALL;

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(mCameraDevice, VIDIOC_G_FMT, &parm) < 0) {
            CAMERA_HAL_ERR("VIDIOC_S_PARM failed\n");
            return CAPTURE_DEVICE_ERR_SYS_CALL;
        }else{

            CAMERA_HAL_LOG_RUNTIME(" Width = %d\n", fmt.fmt.pix.width);
            CAMERA_HAL_LOG_RUNTIME(" Height = %d \n", fmt.fmt.pix.height);
            CAMERA_HAL_LOG_RUNTIME(" Image size = %d\n", fmt.fmt.pix.sizeimage);
            CAMERA_HAL_LOG_RUNTIME(" pixelformat = %x\n", fmt.fmt.pix.pixelformat);
        }
        pCapcfg->framesize = fmt.fmt.pix.sizeimage;

        return CAPTURE_DEVICE_ERR_NONE;
    }

    CAPTURE_DEVICE_ERR_RET V4l2CsiDevice :: V4l2GetCaptureMode(struct capture_config_t *pCapcfg, unsigned int *pMode){

        CAMERA_HAL_LOG_FUNC;
        if (mCameraDevice <= 0 || pCapcfg == NULL){
            return CAPTURE_DEVICE_ERR_BAD_PARAM;
        }

        unsigned int capturemode = 0;
        unsigned int capturewidth =  pCapcfg->width;
        unsigned int captureheight = pCapcfg->height;
        unsigned int pic_waite_buf_num = 0;
        if ((strstr(mInitalDeviceName, OV5640_NAME_STR) != 0) ||
                (strstr(mInitalDeviceName, OV5642_NAME_STR) != 0)){
            pic_waite_buf_num = 10;
            if (capturewidth == 640 && captureheight == 480)
                capturemode = 0;	/* VGA mode */
            else if (capturewidth == 320 && captureheight == 240)
                capturemode = 1;	/* QVGA mode */
            else if (capturewidth == 720 && captureheight == 480)
                capturemode = 2;	/* PAL mode */
            else if (capturewidth == 720 && captureheight == 576)
                capturemode = 3;	/* PAL mode */
            else if (capturewidth == 1280 && captureheight == 720)
                capturemode = 4;	/* 720P mode */
            else if (capturewidth == 1920 && captureheight == 1080){
                pic_waite_buf_num = 5;
                capturemode = 5;	/* 1080P mode */
            }
            else if (capturewidth == 2592 && captureheight == 1944) {
                pic_waite_buf_num = 5;
                capturemode = 6;	/* 2592x1944 mode */
            }
            else if (capturewidth == 176 && captureheight == 144) {
                capturemode = 7;	/* QCIF mode */
            }
            else{
                CAMERA_HAL_ERR("The camera mode is not supported!!!!");
                return CAPTURE_DEVICE_ERR_BAD_PARAM;
            }
        }else if(strstr(mInitalDeviceName, OV3640_NAME_STR) != 0){
            pic_waite_buf_num = 10;
            if (capturewidth == 320 && captureheight == 240)
                capturemode = 1;	/* QVGA mode */
            else if (capturewidth == 640 && captureheight == 480)
                capturemode = 0;	/* VGA mode */
            else if (capturewidth == 720 && captureheight == 480)
                capturemode = 4;
            else if (capturewidth == 720 && captureheight == 576)
                capturemode = 5;
            else if (capturewidth == 2048 && captureheight == 1536)
            {
                pic_waite_buf_num = 10;
                capturemode = 3;	/* QXGA mode */
            }
            else
            {
                CAMERA_HAL_ERR("The camera mode is not supported!!!!");
                return CAPTURE_DEVICE_ERR_BAD_PARAM;
            }
        }else{
            capturemode = 0;
            pic_waite_buf_num = 0;
        }

        CAMERA_HAL_LOG_INFO("the mode is %d", capturemode);
        *pMode = capturemode;
        pCapcfg->picture_waite_number = pic_waite_buf_num;

        return CAPTURE_DEVICE_ERR_NONE;
    }

    CAPTURE_DEVICE_ERR_RET V4l2CsiDevice :: V4l2SetRot(struct capture_config_t *pCapcfg){

        CAMERA_HAL_LOG_FUNC;
        CAPTURE_DEVICE_ERR_RET ret = CAPTURE_DEVICE_ERR_NONE;
        if (mCameraDevice <= 0 || pCapcfg == NULL){
            return CAPTURE_DEVICE_ERR_BAD_PARAM;
        }

        struct v4l2_control ctrl;

        // Set rotation
        ctrl.id = V4L2_CID_MXC_ROT;
        if (pCapcfg->rotate == SENSOR_PREVIEW_BACK_REF)
            ctrl.value = V4L2_MXC_CAM_ROTATE_NONE;
        else if (pCapcfg->rotate == SENSOR_PREVIEW_VERT_FLIP)
            ctrl.value = V4L2_MXC_CAM_ROTATE_VERT_FLIP;
        else if (pCapcfg->rotate == SENSOR_PREVIEW_HORIZ_FLIP)
            ctrl.value = V4L2_MXC_CAM_ROTATE_HORIZ_FLIP;
        else if (pCapcfg->rotate == SENSOR_PREVIEW_ROATE_180)
            ctrl.value = V4L2_MXC_CAM_ROTATE_180;
        else
            ctrl.value = V4L2_MXC_ROTATE_NONE;

        if (ioctl(mCameraDevice, VIDIOC_S_CTRL, &ctrl) < 0) {
            CAMERA_HAL_ERR("set ctrl failed\n");
            return CAPTURE_DEVICE_ERR_SYS_CALL;
        }

        return ret;
    }
};

