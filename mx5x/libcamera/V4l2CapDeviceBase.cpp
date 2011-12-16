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
#include <linux/videodev2.h>
#include <linux/mxcfb.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <utils/threads.h>
#include <dirent.h>

#include "V4l2CapDeviceBase.h"




namespace android{

    V4l2CapDeviceBase ::V4l2CapDeviceBase()
        :mCameraDevice(0),
        mFmtParamIdx(0),
        mSizeFPSParamIdx(0),
        mRequiredFmt(0),
        mBufQueNum(0),
        mQueuedBufNum(0)

    {
        mCaptureDeviceName[0] = '#';
    }

    V4l2CapDeviceBase :: ~V4l2CapDeviceBase()
    {
    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase::SetDevName(char * deviceName){
        CAMERA_HAL_LOG_FUNC;
        CAPTURE_DEVICE_ERR_RET ret = CAPTURE_DEVICE_ERR_NONE; 
        if(NULL == deviceName)
            return CAPTURE_DEVICE_ERR_BAD_PARAM;
        strcpy(mInitalDeviceName, deviceName);
        return ret;
    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase::GetDevName(char * deviceName){
        CAMERA_HAL_LOG_FUNC;
        CAPTURE_DEVICE_ERR_RET ret = CAPTURE_DEVICE_ERR_NONE;
        if(NULL == deviceName)
            return CAPTURE_DEVICE_ERR_BAD_PARAM;
        strcpy(deviceName, mInitalDeviceName);
        return ret;
    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase::DevOpen(){
        CAMERA_HAL_LOG_FUNC;

        return V4l2Open(); 
    } 

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase::EnumDevParam(DevParamType devParamType, void *retParam){
        CAPTURE_DEVICE_ERR_RET ret = CAPTURE_DEVICE_ERR_NONE; 
        CAMERA_HAL_LOG_FUNC;

        if(mCameraDevice <= 0)
            return CAPTURE_DEVICE_ERR_OPEN;
        else
            return V4l2EnumParam(devParamType,retParam);

    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: DevSetConfig(struct capture_config_t *pCapcfg){

        CAMERA_HAL_LOG_FUNC;
        if (mCameraDevice <= 0 || pCapcfg == NULL){
            return CAPTURE_DEVICE_ERR_BAD_PARAM;
        }

        mCapCfg = *pCapcfg;
        return V4l2SetConfig(pCapcfg);

    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: DevAllocateBuf(DMA_BUFFER *DevBufQue, unsigned int *pBufQueNum){

        CAMERA_HAL_LOG_FUNC;
        if (mCameraDevice <= 0){
            return CAPTURE_DEVICE_ERR_OPEN;
        }else
            return V4l2AllocateBuf(DevBufQue, pBufQueNum);
    }

    CAPTURE_DEVICE_ERR_RET  V4l2CapDeviceBase :: DevPrepare(){

        CAMERA_HAL_LOG_FUNC;
        if (mCameraDevice <= 0){
            return CAPTURE_DEVICE_ERR_OPEN;
        }else
            return V4l2Prepare();
    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: DevStart(){

        CAMERA_HAL_LOG_FUNC;
        if (mCameraDevice <= 0){
            return CAPTURE_DEVICE_ERR_OPEN;
        }else
            return V4l2Start();
    }
    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: DevDequeue(unsigned int *pBufQueIdx){

        CAMERA_HAL_LOG_FUNC;
        if (mCameraDevice <= 0 || mBufQueNum == 0 || mCaptureBuffers == NULL){
            return CAPTURE_DEVICE_ERR_OPEN;
        }else{
            return V4l2Dequeue(pBufQueIdx);
        }

    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: DevQueue( unsigned int BufQueIdx){

        CAMERA_HAL_LOG_FUNC;
        if (mCameraDevice <= 0 || mBufQueNum == 0 || mCaptureBuffers == NULL){
            return CAPTURE_DEVICE_ERR_OPEN;
        }else{
            return V4l2Queue(BufQueIdx);
        }
    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: DevStop(){
        CAMERA_HAL_LOG_FUNC;
        if (mCameraDevice <= 0){
            return CAPTURE_DEVICE_ERR_OPEN;
        }else{
            return V4l2Stop();
        }

    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: DevDeAllocate(){
        CAMERA_HAL_LOG_FUNC;

        if (mCameraDevice <= 0){
            return CAPTURE_DEVICE_ERR_OPEN;
        }else{
            return V4l2DeAlloc();
        }
    }
    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: DevClose(){

        CAMERA_HAL_LOG_FUNC;

        if (mCameraDevice <= 0){
            return CAPTURE_DEVICE_ERR_OPEN;
        }else{
            return V4l2Close();
        }
    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: V4l2Open(){
        CAMERA_HAL_LOG_FUNC;
        int fd = 0, i, j, is_found = 0;
        const char *flags[] = {"uncompressed", "compressed"};

        char   dev_node[CAMAERA_FILENAME_LENGTH];
        DIR *v4l_dir = NULL;
        struct dirent *dir_entry;
        struct v4l2_capability v4l2_cap;
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
                    if(ioctl(fd, VIDIOC_QUERYCAP, &v4l2_cap) < 0 ) {
                        close(fd);
                        continue;
                    } else if ((strstr((char *)v4l2_cap.driver, mInitalDeviceName) != 0) &&
                            (v4l2_cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
                        is_found = 1;
                        strcpy(mCaptureDeviceName, dev_node);
                        CAMERA_HAL_LOG_RUNTIME("device name is %s", mCaptureDeviceName);
                        break;
                    } else
                        close(fd);
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

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: V4l2EnumParam(DevParamType devParamType, void *retParam){
        CAPTURE_DEVICE_ERR_RET ret = CAPTURE_DEVICE_ERR_NONE; 

        CAMERA_HAL_LOG_FUNC;
        CAMERA_HAL_LOG_RUNTIME("devParamType is %d", devParamType);

        if(mCameraDevice <= 0)
            return CAPTURE_DEVICE_ERR_OPEN;
        switch(devParamType){
            case OUTPU_FMT: 
                ret = V4l2EnumFmt(retParam);
                break;
            case FRAME_SIZE_FPS:
                {
                    ret = V4l2EnumSizeFps(retParam);
                    break;
                }
            default:
                {
                    ret = CAPTURE_DEVICE_ERR_SET_PARAM;
                    break;
                }
        }
        return ret;

    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: V4l2EnumFmt(void *retParam){
        CAMERA_HAL_LOG_FUNC;
        CAPTURE_DEVICE_ERR_RET ret = CAPTURE_DEVICE_ERR_NONE; 
        struct v4l2_fmtdesc vid_fmtdesc;
        unsigned int *pParamVal = (unsigned int *)retParam;

        vid_fmtdesc.index = mFmtParamIdx;
        vid_fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl(mCameraDevice, VIDIOC_ENUM_FMT, &vid_fmtdesc ) != 0){
            mFmtParamIdx = 0;
            ret = CAPTURE_DEVICE_ERR_GET_PARAM;
        }else{
            CAMERA_HAL_LOG_RUNTIME("vid_fmtdesc.pixelformat is %x", vid_fmtdesc.pixelformat);
            *pParamVal = vid_fmtdesc.pixelformat;
            mFmtParamIdx ++;
            ret = CAPTURE_DEVICE_ERR_ENUM_CONTINUE;
        }
        return ret;
    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: V4l2EnumSizeFps(void *retParam){
        CAMERA_HAL_LOG_FUNC;
        CAPTURE_DEVICE_ERR_RET ret = CAPTURE_DEVICE_ERR_NONE; 
        struct v4l2_frmsizeenum vid_frmsize;
        struct v4l2_frmivalenum vid_frmval;

        struct capture_config_t *pCapCfg =(struct capture_config_t *) retParam;
        memset(&vid_frmsize, 0, sizeof(struct v4l2_frmsizeenum));
        vid_frmsize.index = mSizeFPSParamIdx;
        CAMERA_HAL_LOG_RUNTIME("the query for size fps fmt is %x",pCapCfg->fmt);
        vid_frmsize.pixel_format = pCapCfg->fmt;
        if (ioctl(mCameraDevice, VIDIOC_ENUM_FRAMESIZES, &vid_frmsize) != 0){
            mSizeFPSParamIdx = 0;
            ret = CAPTURE_DEVICE_ERR_SET_PARAM;
        }else{
            memset(&vid_frmval, 0, sizeof(struct v4l2_frmivalenum));
            CAMERA_HAL_LOG_RUNTIME("in %s the w %d, h %d", __FUNCTION__,vid_frmsize.discrete.width, vid_frmsize.discrete.height);
            vid_frmval.index = 0; //get the first, that is the min frame interval, but the biggest fps
            vid_frmval.pixel_format = pCapCfg->fmt;
            vid_frmval.width = vid_frmsize.discrete.width;
            vid_frmval.height= vid_frmsize.discrete.height;
            if (ioctl(mCameraDevice, VIDIOC_ENUM_FRAMEINTERVALS, &vid_frmval) != 0){
                CAMERA_HAL_ERR("VIDIOC_ENUM_FRAMEINTERVALS error");
                mSizeFPSParamIdx = 0;
                ret = CAPTURE_DEVICE_ERR_SET_PARAM;
            }else{
                pCapCfg->width	= vid_frmsize.discrete.width;
                pCapCfg->height = vid_frmsize.discrete.height;
                pCapCfg->tv.numerator = vid_frmval.discrete.numerator;
                pCapCfg->tv.denominator = vid_frmval.discrete.denominator;
                mSizeFPSParamIdx ++;
                ret = CAPTURE_DEVICE_ERR_ENUM_CONTINUE;
            }
        }
        return ret;
    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: V4l2ConfigInput(struct capture_config_t *pCapcfg){

        CAMERA_HAL_LOG_FUNC;
        CAPTURE_DEVICE_ERR_RET ret = CAPTURE_DEVICE_ERR_NONE;
        if (mCameraDevice <= 0 || pCapcfg == NULL){
            return CAPTURE_DEVICE_ERR_BAD_PARAM;
        }

        //For uvc Camera do nothing here.

        return ret;
    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: V4l2GetCaptureMode(struct capture_config_t *pCapcfg, unsigned int *pMode){

        CAMERA_HAL_LOG_FUNC;
        if (mCameraDevice <= 0 || pCapcfg == NULL){
            return CAPTURE_DEVICE_ERR_BAD_PARAM;
        }
        *pMode = 0;
        return CAPTURE_DEVICE_ERR_NONE;
    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: V4l2SetRot(struct capture_config_t *pCapcfg){

        CAMERA_HAL_LOG_FUNC;
        CAPTURE_DEVICE_ERR_RET ret = CAPTURE_DEVICE_ERR_NONE;
        if (mCameraDevice <= 0 || pCapcfg == NULL){
            return CAPTURE_DEVICE_ERR_BAD_PARAM;
        }

        //For uvc Camera do nothing here.

        return ret;
    }


    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: V4l2SetConfig(struct capture_config_t *pCapcfg){

        CAMERA_HAL_LOG_FUNC;
        if (mCameraDevice <= 0 || pCapcfg == NULL){
            return CAPTURE_DEVICE_ERR_BAD_PARAM;
        }

        CAPTURE_DEVICE_ERR_RET ret = CAPTURE_DEVICE_ERR_NONE;
        struct v4l2_format fmt;
        struct v4l2_control ctrl;
        struct v4l2_streamparm parm;

        V4l2ConfigInput(pCapcfg);

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

        parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        parm.parm.capture.timeperframe.numerator = pCapcfg->tv.numerator;
        parm.parm.capture.timeperframe.denominator = pCapcfg->tv.denominator;
        ret = V4l2GetCaptureMode(pCapcfg, &(parm.parm.capture.capturemode));
        if (ret != CAPTURE_DEVICE_ERR_NONE)
            return ret;

        if (ioctl(mCameraDevice, VIDIOC_S_PARM, &parm) < 0) {
            CAMERA_HAL_ERR("%s:%d  VIDIOC_S_PARM failed\n", __FUNCTION__,__LINE__);
            CAMERA_HAL_ERR("frame timeval is numerator %d, denominator %d",parm.parm.capture.timeperframe.numerator,
                    parm.parm.capture.timeperframe.denominator);
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
        pCapcfg->picture_waite_number = 1; //For uvc, the first frame is ok.

        return CAPTURE_DEVICE_ERR_NONE;
    }


    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: V4l2AllocateBuf(DMA_BUFFER *DevBufQue, unsigned int *pBufQueNum){
        unsigned int i;
        struct v4l2_buffer buf;
        enum v4l2_buf_type type;
        struct v4l2_requestbuffers req;
        int BufQueNum;

        CAMERA_HAL_LOG_FUNC;
        if (mCameraDevice <= 0 || DevBufQue == NULL || pBufQueNum == NULL || *pBufQueNum == 0){
            return CAPTURE_DEVICE_ERR_BAD_PARAM;
        }

        mBufQueNum = *pBufQueNum;

        memset(&req, 0, sizeof (req));
        req.count = mBufQueNum;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        if (ioctl(mCameraDevice, VIDIOC_REQBUFS, &req) < 0) {
            CAMERA_HAL_ERR("v4l_capture_setup: VIDIOC_REQBUFS failed\n");
            return CAPTURE_DEVICE_ERR_SYS_CALL;
        }

        /*the driver may can't meet the request, and return the buf num it can handle*/
        *pBufQueNum = mBufQueNum = req.count;

        for (i = 0; i < mBufQueNum; i++) {
            memset(&buf, 0, sizeof (buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.index = i;
            if (ioctl(mCameraDevice, VIDIOC_QUERYBUF, &buf) < 0) {
                CAMERA_HAL_ERR("VIDIOC_QUERYBUF error\n");
                return CAPTURE_DEVICE_ERR_SYS_CALL;
            } else {
                CAMERA_HAL_LOG_RUNTIME("VIDIOC_QUERYBUF ok\n");
            }

            mCaptureBuffers[i].length = DevBufQue[i].length= buf.length;
            mCaptureBuffers[i].phy_offset = DevBufQue[i].phy_offset = (size_t) buf.m.offset;
            mCaptureBuffers[i].virt_start = DevBufQue[i].virt_start = (unsigned char *)mmap (NULL, mCaptureBuffers[i].length,
                    PROT_READ | PROT_WRITE, MAP_SHARED, mCameraDevice, mCaptureBuffers[i].phy_offset);
            memset(mCaptureBuffers[i].virt_start, 0xFF, mCaptureBuffers[i].length);
            CAMERA_HAL_LOG_RUNTIME("capture buffers[%d].length = %d\n", i, mCaptureBuffers[i].length);
            CAMERA_HAL_LOG_RUNTIME("capture buffers[%d].phy_offset = 0x%x\n", i, mCaptureBuffers[i].phy_offset);
            CAMERA_HAL_LOG_RUNTIME("capture buffers[%d].virt_start = 0x%x\n", i, (unsigned int)(mCaptureBuffers[i].virt_start));
        }

        return CAPTURE_DEVICE_ERR_NONE;
    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: V4l2Prepare(){
        CAMERA_HAL_LOG_FUNC;
        struct v4l2_buffer buf;
        mQueuedBufNum = 0;
        for (unsigned int i = 0; i < mBufQueNum; i++) {
            memset(&buf, 0, sizeof (struct v4l2_buffer));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            buf.m.offset = mCaptureBuffers[i].phy_offset;

            if (ioctl (mCameraDevice, VIDIOC_QBUF, &buf) < 0) {
                CAMERA_HAL_ERR("VIDIOC_QBUF error\n");
                return CAPTURE_DEVICE_ERR_SYS_CALL;
            } 
            mQueuedBufNum ++;
        }

        return CAPTURE_DEVICE_ERR_NONE;
    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: V4l2Start(){
        enum v4l2_buf_type type;
        CAMERA_HAL_LOG_FUNC;
        if (mCameraDevice <= 0 ){
            return CAPTURE_DEVICE_ERR_BAD_PARAM;
        }
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl (mCameraDevice, VIDIOC_STREAMON, &type) < 0) {
            CAMERA_HAL_ERR("VIDIOC_STREAMON error\n");
            return CAPTURE_DEVICE_ERR_SYS_CALL;
        } else{
            CAMERA_HAL_LOG_RUNTIME("VIDIOC_STREAMON ok\n");
        }
        return CAPTURE_DEVICE_ERR_NONE;
    }


    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: V4l2Dequeue(unsigned int *pBufQueIdx){
        int ret;
        struct v4l2_buffer cfilledbuffer;
        CAMERA_HAL_LOG_FUNC;
        if (mCameraDevice <= 0 || mBufQueNum == 0 || mCaptureBuffers == NULL){
            return CAPTURE_DEVICE_ERR_OPEN;
        }
        memset(&cfilledbuffer, 0, sizeof (cfilledbuffer));
        cfilledbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        cfilledbuffer.memory = V4L2_MEMORY_MMAP;
        ret = ioctl(mCameraDevice, VIDIOC_DQBUF, &cfilledbuffer);
        if (ret < 0) {
            CAMERA_HAL_ERR("Camera VIDIOC_DQBUF failure, ret=%d", ret);
            return CAPTURE_DEVICE_ERR_SYS_CALL;
        }
        *pBufQueIdx = cfilledbuffer.index;

        mQueuedBufNum --;

        return CAPTURE_DEVICE_ERR_NONE;
    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: V4l2Queue(unsigned int BufQueIdx){
        int ret;
        struct v4l2_buffer cfilledbuffer;
        CAMERA_HAL_LOG_FUNC;
        if (mCameraDevice <= 0 || mBufQueNum == 0 || mCaptureBuffers == NULL){
            return CAPTURE_DEVICE_ERR_OPEN;
        }
        memset(&cfilledbuffer, 0, sizeof (struct v4l2_buffer));
        cfilledbuffer.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        cfilledbuffer.memory = V4L2_MEMORY_MMAP;
        cfilledbuffer.index = BufQueIdx;
        ret = ioctl(mCameraDevice, VIDIOC_QBUF, &cfilledbuffer);
        if (ret < 0) {
            CAMERA_HAL_ERR("Camera VIDIOC_DQBUF failure, ret=%d", ret);
            return CAPTURE_DEVICE_ERR_SYS_CALL;
        }

        mQueuedBufNum ++;

        return CAPTURE_DEVICE_ERR_NONE;
    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: V4l2Stop(){
        enum v4l2_buf_type type;
        CAMERA_HAL_LOG_FUNC;
        if (mCameraDevice <= 0 ){
            return CAPTURE_DEVICE_ERR_BAD_PARAM;
        }
        type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (ioctl (mCameraDevice, VIDIOC_STREAMOFF, &type) < 0) {
            CAMERA_HAL_ERR("VIDIOC_STREAMON error\n");
            return CAPTURE_DEVICE_ERR_SYS_CALL;
        } else
            CAMERA_HAL_LOG_INFO("VIDIOC_STREAMOFF ok\n");


        return CAPTURE_DEVICE_ERR_NONE;
    }

    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: V4l2DeAlloc(){

        CAMERA_HAL_LOG_FUNC;
        if (mCameraDevice <= 0 ){
            return CAPTURE_DEVICE_ERR_BAD_PARAM;
        }

        for (unsigned int i = 0; i < mBufQueNum; i++) {
            if (mCaptureBuffers[i].length && (mCaptureBuffers[i].virt_start > 0)) {
                munmap(mCaptureBuffers[i].virt_start, mCaptureBuffers[i].length);
                mCaptureBuffers[i].length = 0;
                CAMERA_HAL_LOG_RUNTIME("munmap buffers 0x%x\n", (unsigned int)(mCaptureBuffers[i].virt_start));
            }
        }
        return CAPTURE_DEVICE_ERR_NONE;
    }
    CAPTURE_DEVICE_ERR_RET V4l2CapDeviceBase :: V4l2Close(){

        CAMERA_HAL_LOG_FUNC;

        if (mCameraDevice <= 0 ){
            CAMERA_HAL_LOG_INFO("the device handle is error");
            return CAPTURE_DEVICE_ERR_BAD_PARAM;
        }
        CAMERA_HAL_LOG_INFO("close the device");
        close(mCameraDevice);
        mCameraDevice = -1;
        return CAPTURE_DEVICE_ERR_NONE;
    }


};
