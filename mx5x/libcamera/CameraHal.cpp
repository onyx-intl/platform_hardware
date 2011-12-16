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


#include <cutils/properties.h>
#include "CameraHal.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <hardware_legacy/power.h>

namespace android {

    CameraHal::CameraHal()
        : mParameters(),
        mCallbackCookie(NULL),
        mNotifyCb(NULL),
        mDataCb(NULL),
        mDataCbTimestamp(NULL),
        mCaptureFrameThread(NULL),
        mPostProcessThread(NULL),
        mPreviewShowFrameThread(NULL),
        mEncodeFrameThread(NULL),
        mAutoFocusThread(NULL),
        mTakePicThread(NULL),
        mLock(),
        supportedPictureSizes(NULL),
        supportedPreviewSizes(NULL),
        supportedFPS(NULL),
        supprotedThumbnailSizes(NULL),
        mOverlay(NULL),
        mMsgEnabled(0),
        mPreviewHeap(0),
        mVideoBufNume(VIDEO_OUTPUT_BUFFER_NUM),
        mPPbufNum(0),
        mPreviewRunning(0),
        mPreviewFormat(V4L2_PIX_FMT_NV12), //the optimized selected format, hard code
        mPreviewFrameSize(0),
        mTakePicFlag(false),
        mUvcSpecialCaptureFormat(V4L2_PIX_FMT_YUYV),
        mCaptureFrameSize(0),
        mCaptureBufNum(0),
        mRecordRunning(0),
        mCurrentRecordFrame(0),
        nCameraBuffersQueued(0),
        mPreviewHeapBufNum(PREVIEW_HEAP_BUF_NUM),
        mTakePicBufQueNum(TAKE_PIC_QUE_BUF_NUM),
        mCameraReady(false),
        mCaptureDeviceOpen(false),
        mPPDeviceNeed(false),
        mPPDeviceNeedForPic(false),
        mPowerLock(false),
        mPreviewRotate(CAMERA_PREVIEW_BACK_REF)
    {
        CAMERA_HAL_LOG_FUNC;
        preInit();
    }

    CameraHal :: ~CameraHal()
    {
        CAMERA_HAL_LOG_FUNC;
        CameraMiscDeInit();
        CloseCaptureDevice();
        FreeInterBuf();
        postDestroy();
    }

    void CameraHal :: release()
    {
        CAMERA_HAL_LOG_FUNC;
        Mutex::Autolock lock(mLock);

        mCameraReady = false;
        CameraHALStopPreview();
        UnLockWakeLock();
        return;
    }

    void CameraHal :: preInit()
    {
        CAMERA_HAL_LOG_FUNC;

    }
    void CameraHal :: postDestroy()
    {
        CAMERA_HAL_LOG_FUNC;
    }

    CAMERA_HAL_ERR_RET CameraHal :: setCaptureDevice(sp<CaptureDeviceInterface> capturedevice)
    {
        CAMERA_HAL_LOG_FUNC;
        CAMERA_HAL_ERR_RET ret = CAMERA_HAL_ERR_NONE;
        if (mCameraReady == false)
            mCaptureDevice = capturedevice;
        else
            ret = CAMERA_HAL_ERR_BAD_ALREADY_RUN;
        return ret;
    }

    CAMERA_HAL_ERR_RET CameraHal :: setPostProcessDevice(sp<PostProcessDeviceInterface> postprocessdevice)
    {
        CAMERA_HAL_LOG_FUNC;
        CAMERA_HAL_ERR_RET ret = CAMERA_HAL_ERR_NONE;
        if (mCameraReady == false)
            mPPDevice = postprocessdevice;
        else 
            ret = CAMERA_HAL_ERR_BAD_ALREADY_RUN;
        return ret;
    }

    CAMERA_HAL_ERR_RET CameraHal :: setJpegEncoder(sp<JpegEncoderInterface>jpegencoder)
    {
        CAMERA_HAL_LOG_FUNC;
        CAMERA_HAL_ERR_RET ret = CAMERA_HAL_ERR_NONE;
        if (mCameraReady == false)
            mJpegEncoder = jpegencoder;
        else
            ret = CAMERA_HAL_ERR_BAD_ALREADY_RUN;
        return ret;
    }

    CAMERA_HAL_ERR_RET CameraHal::Init()
    {
        CAMERA_HAL_LOG_FUNC;
        CAMERA_HAL_ERR_RET ret = CAMERA_HAL_ERR_NONE;
        mCameraReady == true;

        if ((ret = AolLocForInterBuf())<0)
            return ret;
        if ((ret = InitCameraHalParam()) < 0)
            return ret;
        if (mPPDeviceNeed == true && mPPDevice == NULL)
            return CAMERA_HAL_ERR_PP_NULL;
        if ((ret = CameraMiscInit()) < 0)
            return ret;

        return ret;
    }
    void  CameraHal::setPreviewRotate(CAMERA_PREVIEW_ROTATE previewRotate)
    {
        CAMERA_HAL_LOG_FUNC;
        mPreviewRotate = previewRotate;
        return ;
    }

    CAMERA_HAL_ERR_RET  CameraHal :: AolLocForInterBuf()
    {
        CAMERA_HAL_LOG_FUNC;
        CAMERA_HAL_ERR_RET ret = CAMERA_HAL_ERR_NONE;

        supportedPictureSizes = (char *)malloc(CAMER_PARAM_BUFFER_SIZE);
        supportedPreviewSizes = (char *)malloc(CAMER_PARAM_BUFFER_SIZE);
        supportedFPS          = (char *)malloc(CAMER_PARAM_BUFFER_SIZE);
        supprotedThumbnailSizes = (char *)malloc(CAMER_PARAM_BUFFER_SIZE);

        if (supportedPictureSizes == NULL ||
                supportedPreviewSizes == NULL ||
                supportedFPS          == NULL ||
                supprotedThumbnailSizes == NULL)
            ret = CAMERA_HAL_ERR_ALLOC_BUF;

        return ret;
    }
    void  CameraHal :: FreeInterBuf()
    {
        CAMERA_HAL_LOG_FUNC;
        if (supportedPictureSizes)
            free(supportedPictureSizes);
        if (supportedPreviewSizes)
            free(supportedPreviewSizes);
        if (supportedFPS)
            free(supportedFPS);
        if (supprotedThumbnailSizes)
            free(supprotedThumbnailSizes);
    }

    CAMERA_HAL_ERR_RET CameraHal :: InitCameraHalParam()
    {	
        CAMERA_HAL_LOG_FUNC;
        CAMERA_HAL_ERR_RET ret = CAMERA_HAL_ERR_NONE;

        if ((ret = GetCameraBaseParam(&mParameters)) < 0)
            return ret;

        if ((ret = GetPictureExifParam(&mParameters)) < 0)
            return ret;

        return ret;
    }

    CAMERA_HAL_ERR_RET CameraHal::CameraMiscInit()
    {
        CAMERA_HAL_LOG_FUNC;
        CAMERA_HAL_ERR_RET ret = CAMERA_HAL_ERR_NONE;
        pthread_mutex_init(&mPPIOParamMutex, NULL);
        pthread_mutex_init(&mOverlayMutex, NULL);
        return ret;
    }
    CAMERA_HAL_ERR_RET CameraHal::CameraMiscDeInit()
    {
        CAMERA_HAL_LOG_FUNC;
        CAMERA_HAL_ERR_RET ret = CAMERA_HAL_ERR_NONE;
        pthread_mutex_destroy(&mPPIOParamMutex);
        pthread_mutex_destroy(&mOverlayMutex);
        return ret;
    }

    CAMERA_HAL_ERR_RET CameraHal :: GetCameraBaseParam(CameraParameters *pParam)
    {
        CAMERA_HAL_LOG_FUNC;
        char TmpStr[20];
        unsigned int CapPreviewFmt[MAX_QUERY_FMT_TIMES];
        struct capture_config_t CaptureSizeFps;
        int  previewCnt= 0, pictureCnt = 0, i;

        pParam->setPreviewFormat(CameraParameters::PIXEL_FORMAT_YUV420SP);
        pParam->set(CameraParameters::KEY_VIDEO_FRAME_FORMAT, CameraParameters::PIXEL_FORMAT_YUV420SP);
        pParam->set(CameraParameters::KEY_SUPPORTED_PREVIEW_FORMATS, CameraParameters::PIXEL_FORMAT_YUV420SP);

        //the Camera Open here will not be close immediately, for later preview.
        if (OpenCaptureDevice() < 0)
            return CAMERA_HAL_ERR_OPEN_CAPTURE_DEVICE;

        memset(mCaptureSupportedFormat, 0, sizeof(unsigned int)*MAX_QUERY_FMT_TIMES);

        for(i =0; i< MAX_QUERY_FMT_TIMES; i ++){
            if (mCaptureDevice->EnumDevParam(OUTPU_FMT,&(mCaptureSupportedFormat[i])) < 0)
                break;
        }
        if (i == 0)
            return CAMERA_HAL_ERR_GET_PARAM;

        if (NegotiateCaptureFmt(false) < 0)
            return CAMERA_HAL_ERR_GET_PARAM;

        CaptureSizeFps.fmt = mPreviewCapturedFormat;

        CAMERA_HAL_LOG_INFO("mPreviewCapturedFormat is %x", mPreviewCapturedFormat);

        for(;;){
            if (mCaptureDevice->EnumDevParam(FRAME_SIZE_FPS,&CaptureSizeFps) <0){
                CAMERA_HAL_LOG_RUNTIME("get the frame size and time interval error");
                break;
            }
            sprintf(TmpStr, "%dx%d", CaptureSizeFps.width,CaptureSizeFps.height);
            CAMERA_HAL_LOG_INFO("the size is %s , the framerate is %d ", TmpStr, (CaptureSizeFps.tv.denominator/CaptureSizeFps.tv.numerator));
            if (previewCnt == 0)
                strncpy((char*) supportedPictureSizes, TmpStr, CAMER_PARAM_BUFFER_SIZE);
            else{
                strncat(supportedPictureSizes,  PARAMS_DELIMITER, CAMER_PARAM_BUFFER_SIZE);
                strncat(supportedPictureSizes, TmpStr, CAMER_PARAM_BUFFER_SIZE);
            }
            pictureCnt ++;

            if (CaptureSizeFps.tv.denominator/CaptureSizeFps.tv.numerator > 25){
                if (previewCnt == 0)
                    strncpy((char*) supportedPreviewSizes, TmpStr, CAMER_PARAM_BUFFER_SIZE);
                else{
                    strncat(supportedPreviewSizes,  PARAMS_DELIMITER, CAMER_PARAM_BUFFER_SIZE);
                    strncat(supportedPreviewSizes, TmpStr, CAMER_PARAM_BUFFER_SIZE);
                }
                previewCnt ++;
            }
        }

        /*hard code here*/
        strcpy(supportedFPS, "15,30");
        CAMERA_HAL_LOG_INFO("##The supportedPictureSizes is %s##", supportedPictureSizes);
        CAMERA_HAL_LOG_INFO("##the supportedPreviewSizes is %s##", supportedPreviewSizes);
        CAMERA_HAL_LOG_INFO("##the supportedFPS is %s##", supportedFPS);

        pParam->set(CameraParameters::KEY_SUPPORTED_PICTURE_SIZES, supportedPictureSizes);
        pParam->set(CameraParameters::KEY_SUPPORTED_PREVIEW_SIZES, supportedPreviewSizes);
        pParam->set(CameraParameters::KEY_SUPPORTED_PREVIEW_FRAME_RATES, supportedFPS);
        pParam->set(CameraParameters::KEY_SUPPORTED_PREVIEW_FPS_RANGE, "(1000,15000),(5000,30000)");
        pParam->set(CameraParameters::KEY_PREVIEW_FPS_RANGE, "5000,30000");

        pParam->setPreviewSize(640, 480);
        pParam->setPictureSize(640, 480);
        pParam->setPreviewFrameRate(5);

        return CAMERA_HAL_ERR_NONE;

    }

    status_t CameraHal :: OpenCaptureDevice()
    {
        CAMERA_HAL_LOG_FUNC;
        status_t ret = NO_ERROR;
        if (mCaptureDeviceOpen){
            CAMERA_HAL_LOG_INFO("The capture device already open");
            return NO_ERROR;
        }
        else if (mCaptureDevice != NULL){
            if ( mCaptureDevice->DevOpen()<0 )
                return INVALID_OPERATION;
            mCaptureDeviceOpen = true;
        }else{
            CAMERA_HAL_ERR("no capture device assigned");
            return INVALID_OPERATION;
        }
        return ret;
    }
    void CameraHal ::CloseCaptureDevice()
    {
        CAMERA_HAL_LOG_FUNC;
        if (mCaptureDeviceOpen && mCaptureDevice != NULL){
            mCaptureDevice->DevClose();
            mCaptureDeviceOpen = false;
        }
    }

    CAMERA_HAL_ERR_RET CameraHal :: GetPictureExifParam(CameraParameters *pParam)
    {
        CAMERA_HAL_LOG_FUNC;
        char tmpBuffer[CAMER_PARAM_BUFFER_SIZE];

        /*hard code here*/
        pParam->set(CameraParameters::KEY_FOCUS_DISTANCES, "24.0,50.0,2147483648.0");
        pParam->setPictureFormat(CameraParameters::PIXEL_FORMAT_JPEG);
        pParam->set(CameraParameters::KEY_SUPPORTED_PICTURE_FORMATS, CameraParameters::PIXEL_FORMAT_JPEG);
        pParam->set(CameraParameters::KEY_JPEG_QUALITY, 100);
        strcpy(supprotedThumbnailSizes, "0x0,128x128,96x96");
        pParam->set(CameraParameters::KEY_SUPPORTED_JPEG_THUMBNAIL_SIZES, supprotedThumbnailSizes);
        pParam->set(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH, "96");
        pParam->set(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT, "96");
        pParam->set(CameraParameters::KEY_JPEG_THUMBNAIL_QUALITY, "90");

        memset(tmpBuffer, '\0', sizeof(*tmpBuffer));
        strncat((char*) tmpBuffer, (const char*) CameraParameters::WHITE_BALANCE_AUTO, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) CameraParameters::WHITE_BALANCE_INCANDESCENT, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) CameraParameters::WHITE_BALANCE_FLUORESCENT, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) CameraParameters::WHITE_BALANCE_DAYLIGHT, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) CameraParameters::WHITE_BALANCE_SHADE, CAMER_PARAM_BUFFER_SIZE);
        pParam->set(CameraParameters::KEY_SUPPORTED_WHITE_BALANCE, tmpBuffer);
        pParam->set(CameraParameters::KEY_WHITE_BALANCE, CameraParameters::WHITE_BALANCE_AUTO);

        memset(tmpBuffer, '\0', sizeof(*tmpBuffer));
        strncat((char*) tmpBuffer, (const char*) CameraParameters::EFFECT_NONE, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) CameraParameters::EFFECT_MONO, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) CameraParameters::EFFECT_NEGATIVE, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) CameraParameters::EFFECT_SOLARIZE,  CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) CameraParameters::EFFECT_SEPIA, CAMER_PARAM_BUFFER_SIZE);
        pParam->set(CameraParameters::KEY_SUPPORTED_EFFECTS, tmpBuffer);
        pParam->set(CameraParameters::KEY_EFFECT, CameraParameters::EFFECT_NONE);

        memset(tmpBuffer, '\0', sizeof(*tmpBuffer));
        strncat((char*) tmpBuffer, (const char*) CameraParameters::SCENE_MODE_AUTO, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) CameraParameters::SCENE_MODE_PORTRAIT, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) CameraParameters::SCENE_MODE_LANDSCAPE, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) CameraParameters::SCENE_MODE_SPORTS, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) CameraParameters::SCENE_MODE_NIGHT_PORTRAIT, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) CameraParameters::SCENE_MODE_FIREWORKS, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) CameraParameters::SCENE_MODE_NIGHT, CAMER_PARAM_BUFFER_SIZE);
        pParam->set(CameraParameters::KEY_SUPPORTED_SCENE_MODES, tmpBuffer);
        pParam->set(CameraParameters::KEY_SCENE_MODE, CameraParameters::SCENE_MODE_AUTO);

        pParam->set(CameraParameters::KEY_SUPPORTED_FOCUS_MODES, CameraParameters::FOCUS_MODE_AUTO);
        pParam->set(CameraParameters::KEY_FOCUS_MODE, CameraParameters::FOCUS_MODE_AUTO);

        pParam->set(CameraParameters::KEY_FOCAL_LENGTH, "10.001");
        pParam->set(CameraParameters::KEY_HORIZONTAL_VIEW_ANGLE, "54.8");
        pParam->set(CameraParameters::KEY_VERTICAL_VIEW_ANGLE, "42.5");
        pParam->set(CameraParameters::KEY_EXPOSURE_COMPENSATION, "0");
        pParam->set(CameraParameters::KEY_MAX_EXPOSURE_COMPENSATION, "0");
        pParam->set(CameraParameters::KEY_MIN_EXPOSURE_COMPENSATION, "0");
        pParam->set(CameraParameters::KEY_EXPOSURE_COMPENSATION_STEP, "0.0");

        memset(tmpBuffer, '\0', sizeof(*tmpBuffer));
        strncat((char*) tmpBuffer, (const char*) CameraParameters::ANTIBANDING_50HZ, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) CameraParameters::ANTIBANDING_60HZ, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) PARAMS_DELIMITER, CAMER_PARAM_BUFFER_SIZE);
        strncat((char*) tmpBuffer, (const char*) CameraParameters::ANTIBANDING_OFF, CAMER_PARAM_BUFFER_SIZE);
        pParam->set(CameraParameters::KEY_SUPPORTED_ANTIBANDING, tmpBuffer);
        pParam->set(CameraParameters::KEY_ANTIBANDING, CameraParameters::ANTIBANDING_OFF);

        memset(tmpBuffer, '\0', sizeof(*tmpBuffer));
        strncat( (char*) tmpBuffer, (const char*) CameraParameters::FLASH_MODE_OFF, CAMER_PARAM_BUFFER_SIZE);
        pParam->set(CameraParameters::KEY_SUPPORTED_FLASH_MODES, tmpBuffer);
        pParam->set(CameraParameters::KEY_FLASH_MODE, CameraParameters::FLASH_MODE_OFF);
        pParam->set(CameraParameters::KEY_ZOOM_SUPPORTED, CameraParameters::TRUE);
        pParam->set(CameraParameters::KEY_MAX_ZOOM, "1");
        // default zoom should be 0 as CTS defined
        pParam->set(CameraParameters::KEY_ZOOM, "0");
        //the zoom ratios in 1/100 increments. Ex: a zoom of 3.2x is
        //returned as 320. The number of elements is {@link
        //#getMaxZoom} + 1. The list is sorted from small to large. The
        //first element is always 100. The last element is the zoom
        //ratio of the maximum zoom value.
        pParam->set(CameraParameters::KEY_ZOOM_RATIOS, "100,200");

        return CAMERA_HAL_ERR_NONE;
    }

    sp<IMemoryHeap> CameraHal::getPreviewHeap() const
    {
        CAMERA_HAL_LOG_FUNC;

        return mPreviewHeap;
    }

    sp<IMemoryHeap> CameraHal::getRawHeap() const
    {
        return NULL;
    }

    status_t CameraHal::dump(int fd, const Vector<String16>& args) const
    {
        return NO_ERROR;
    }

    status_t CameraHal::sendCommand(int32_t command, int32_t arg1,
            int32_t arg2)
    {
        return BAD_VALUE;
    }

    void CameraHal::setCallbacks(notify_callback notify_cb,
            data_callback data_cb,
            data_callback_timestamp data_cb_timestamp,
            void* user)
    {
        Mutex::Autolock lock(mLock);
        mNotifyCb = notify_cb;
        mDataCb = data_cb;
        mDataCbTimestamp = data_cb_timestamp;
        mCallbackCookie = user;
    }

    void CameraHal::enableMsgType(int32_t msgType)
    {
        Mutex::Autolock lock(mLock);
        CAMERA_HAL_LOG_INFO("###the mesg enabled is %x###", msgType);
        mMsgEnabled |= msgType;
    }

    void CameraHal::disableMsgType(int32_t msgType)
    {
        Mutex::Autolock lock(mLock);
        CAMERA_HAL_LOG_INFO("###the mesg disabled is %x###", msgType);
        mMsgEnabled &= ~msgType;
    }
    bool CameraHal::msgTypeEnabled(int32_t msgType)
    {
        Mutex::Autolock lock(mLock);
        CAMERA_HAL_LOG_INFO("###the mesg check is %x###", msgType);
        return (mMsgEnabled & msgType);
    }

    CameraParameters CameraHal::getParameters() const
    {
        CAMERA_HAL_LOG_FUNC;

        Mutex::Autolock lock(mLock);
        return mParameters;
    }

    status_t  CameraHal:: setParameters(const CameraParameters& params)
    {
        CAMERA_HAL_LOG_FUNC;
        int w, h;
        int framerate;
        int max_zoom,zoom, max_fps, min_fps;
        char tmp[128];
        Mutex::Autolock lock(mLock);

        max_zoom = params.getInt(CameraParameters::KEY_MAX_ZOOM);
        zoom = params.getInt(CameraParameters::KEY_ZOOM);
        if(zoom > max_zoom){
            CAMERA_HAL_ERR("Invalid zoom setting, zoom %d, max zoom %d",zoom,max_zoom);
            return BAD_VALUE;
        }
        if (!(strcmp(params.getPreviewFormat(), "yuv420sp") == 0) ||
                (strcmp(params.getPreviewFormat(), "yuv422i") == 0)) {
            CAMERA_HAL_ERR("Only yuv420 or yuv420i is supported");
            return BAD_VALUE;
        }

        if (strcmp(params.getPictureFormat(), "jpeg") != 0) {
            CAMERA_HAL_ERR("Only jpeg still pictures are supported");
            return BAD_VALUE;
        }

        params.getPreviewSize(&w, &h);
        sprintf(tmp, "%dx%d", w, h);
        CAMERA_HAL_LOG_INFO("##the set preview size is %s ##", tmp);
        if (strstr(supportedPreviewSizes, tmp) == NULL){
            CAMERA_HAL_ERR("The preview size w %d, h %d is not corrected", w, h);
            return BAD_VALUE;
        }

        params.getPictureSize(&w, &h);
        sprintf(tmp, "%dx%d", w, h);
        CAMERA_HAL_LOG_INFO("##the set picture size is %s ##", tmp);
        if (strstr(supportedPictureSizes, tmp) == NULL){
            CAMERA_HAL_ERR("The picture size w %d, h %d is not corrected", w, h);
            return BAD_VALUE;
        }

        framerate = params.getPreviewFrameRate();
        CAMERA_HAL_LOG_INFO("##the set frame rate is %d ##", framerate);
        if (framerate >30 || framerate<0 ){
            CAMERA_HAL_ERR("The framerate is not corrected");
            return BAD_VALUE;
        }

        params.getPreviewFpsRange(&min_fps, &max_fps);
        CAMERA_HAL_LOG_INFO("###the fps is %d###", max_fps);
        if (max_fps < 1000 || min_fps < 1000 || max_fps > 30000 || min_fps > 30000){
            CAMERA_HAL_ERR("The fps range from %d to %d is error", min_fps, max_fps);
            return BAD_VALUE;
        }

        mParameters = params;

        return NO_ERROR;
    }

    status_t CameraHal::setOverlay(const sp<Overlay> &overlay)
    {
        CAMERA_HAL_LOG_FUNC;

        Mutex::Autolock lock(mLock);
        if (overlay == NULL){
            CAMERA_HAL_LOG_INFO("Trying to set overlay, but overlay is null!");
        }
        else{
            CAMERA_HAL_LOG_INFO("Get the overlay to display");
            overlay->setParameter(OVERLAY_MODE, OVERLAY_PUSH_MODE);
        }
        pthread_mutex_lock(&mOverlayMutex);
        mOverlay = overlay;
        pthread_mutex_unlock(&mOverlayMutex);
        return NO_ERROR;
    }

    status_t CameraHal::startPreview()
    {
        CAMERA_HAL_LOG_FUNC;
        status_t ret = NO_ERROR;

        Mutex::Autolock lock(mLock);
        if (mPreviewRunning) {
            return NO_ERROR;
        }
        if ((ret == CameraHALStartPreview())<0)
            return ret;

        LockWakeLock();
        return ret;
    }

    void CameraHal::stopPreview()
    {
        CAMERA_HAL_LOG_FUNC;
        struct timeval af_time, be_time;
        Mutex::Autolock lock(mLock);
        /* Cannot stop preview in recording */
        //   if(mMsgEnabled & CAMERA_MSG_VIDEO_FRAME)
        //       return;

        CameraHALStopPreview();
        UnLockWakeLock();

    }

    bool CameraHal::previewEnabled()
    {
        CAMERA_HAL_LOG_FUNC;
        return mPreviewRunning;
    }

    status_t CameraHal::startRecording()
    {
        CAMERA_HAL_LOG_FUNC;
        status_t ret = NO_ERROR;

        if (mRecordRunning == true ) {
            CAMERA_HAL_LOG_INFO("Recording is already existed\n");
            return ret;
        }
        if((ret = AllocateRecordVideoBuf())<0)
            return ret;

        mRecordRunning = true;

        return NO_ERROR;
    }

    void CameraHal::stopRecording()
    {
        CAMERA_HAL_LOG_FUNC;
        mRecordRunning = false;
    }

    void CameraHal::releaseRecordingFrame(const sp<IMemory>& mem)
    {
        ssize_t offset;
        size_t  size;
        int index;

        offset = mem->offset();
        size   = mem->size();
        index = offset / size;

        mVideoBufferUsing[index] = 0;
    }

    bool CameraHal::recordingEnabled()
    {
        CAMERA_HAL_LOG_FUNC;
        return (mPreviewRunning && mRecordRunning);
    }

    status_t CameraHal::autoFocus()
    {
        CAMERA_HAL_LOG_FUNC;

        Mutex::Autolock lock(mLock);

        if (mAutoFocusThread != NULL)
            mAutoFocusThread.clear();

        mAutoFocusThread = new AutoFocusThread(this);
        if (mAutoFocusThread == NULL)
            return UNKNOWN_ERROR;
        return NO_ERROR;
    }

    status_t CameraHal::cancelAutoFocus()
    {
        CAMERA_HAL_LOG_FUNC;

        return NO_ERROR;
    }

    status_t CameraHal::takePicture()
    {
        CAMERA_HAL_LOG_FUNC;
        Mutex::Autolock lock(mLock);

        if (mTakePicThread != NULL)
            mTakePicThread.clear();

        mTakePicThread= new TakePicThread(this);
        if (mTakePicThread == NULL)
            return UNKNOWN_ERROR;
        return NO_ERROR;
    }

    status_t CameraHal::cancelPicture()
    {
        CAMERA_HAL_LOG_FUNC;

        return NO_ERROR;
    }


    int CameraHal::autoFocusThread()
    {
        CAMERA_HAL_LOG_FUNC;
        int FocusFlag = 0;

        if (mMsgEnabled & CAMERA_MSG_FOCUS)
            mNotifyCb(CAMERA_MSG_FOCUS, true, 0, mCallbackCookie);

        return UNKNOWN_ERROR; //exit the thread
    }

    int CameraHal::takepicThread()
    {
        CAMERA_HAL_LOG_FUNC;
        CAMERA_HAL_LOG_INFO("Camera is taking picture!");

        /* Stop preview, start picture capture, and then restart preview again for CSI camera*/
        CameraHALStopPreview();
        cameraHALTakePicture();

        return UNKNOWN_ERROR;
    }

    int CameraHal :: cameraHALTakePicture()
    {
        CAMERA_HAL_LOG_FUNC;
        int ret = NO_ERROR;
        unsigned int DeQueBufIdx = 0;
        struct jpeg_encoding_conf JpegEncConf;
        DMA_BUFFER Buf_input, Buf_output;
        sp<MemoryBase> JpegMemBase = NULL;
        sp<MemoryHeapBase> JpegImageHeap = NULL;
        int  max_fps, min_fps;

        if (mJpegEncoder == NULL){
            CAMERA_HAL_ERR("the jpeg encoder is NULL");
            return BAD_VALUE;
        }
        mParameters.getPictureSize((int *)&(mCaptureDeviceCfg.width),(int *)&(mCaptureDeviceCfg.height));
        mCaptureDeviceCfg.tv.numerator = 1;
        mCaptureDevice->GetDevName(mCameraSensorName);
        if (strstr(mCameraSensorName, "uvc") == NULL){
        //according to google's doc getPreviewFrameRate & getPreviewFpsRange should support both.
        // so here just a walkaround, if the app set the frameRate, will follow this frame rate.
        if (mParameters.getPreviewFrameRate() >= 15)
            mCaptureDeviceCfg.tv.denominator = mParameters.getPreviewFrameRate();
        else{
            mParameters.getPreviewFpsRange(&min_fps, &max_fps);
            CAMERA_HAL_LOG_INFO("###start the preview the fps is %d###", max_fps);
            mCaptureDeviceCfg.tv.denominator = max_fps/1000;
        }
        }else{
                mCaptureDeviceCfg.tv.denominator = 15;
        }
        mCaptureBufNum = PICTURE_CAPTURE_BUFFER_NUM;
        mPPbufNum = 1;
        mTakePicFlag = true;
        mPPDeviceNeedForPic = false;
        if ((ret = GetJpegEncoderParam()) < 0)
            return ret;
        if ((ret = NegotiateCaptureFmt(true)) < 0)
            return ret;

        if (mPPDeviceNeedForPic){
            if ((ret = PreparePostProssDevice()) < 0){
                CAMERA_HAL_ERR("PreparePostProssDevice error");
                return ret;
            }
        }
        if ((ret = PrepareCaptureDevices()) < 0)
            return ret;

        if (mPPDeviceNeedForPic){
            if ((ret = PreparePreviwBuf()) < 0){
                CAMERA_HAL_ERR("PreparePreviwBuf error");
                return ret;
            }
        }
        if ((ret = PrepareJpegEncoder()) < 0)
            return ret;

        if (mCaptureDevice->DevStart()<0){
            CAMERA_HAL_ERR("the capture start up failed !!!!");
            return INVALID_OPERATION;
        }

        for (unsigned int i =0;;){
            if (mCaptureDevice->DevDequeue(&DeQueBufIdx) < 0){
                LOGE("VIDIOC_DQBUF Failed!!!");
                ret = UNKNOWN_ERROR;
                goto Pic_out;
            }

            if (++i == mCaptureDeviceCfg.picture_waite_number)
                break;

            if (mCaptureDevice->DevQueue(DeQueBufIdx) < 0 ){
                ret = UNKNOWN_ERROR;
                goto Pic_out;
            }
        }

        JpegImageHeap= new MemoryHeapBase(mCaptureFrameSize);
        if (JpegImageHeap == NULL){
            ret = NO_MEMORY;
            goto Pic_out;
        }
        // do the csc if necessary
        if (mPPDeviceNeedForPic){
            mPPInputParam.user_def_paddr = mCaptureBuffers[DeQueBufIdx].phy_offset;
            mPPOutputParam.user_def_paddr = mPPbuf[0].phy_offset;
            mPPDevice->PPDeviceInit(&mPPInputParam, &mPPOutputParam);
            mPPDevice->DoPorcess(&(mCaptureBuffers[DeQueBufIdx]), &(mPPbuf[0]));
            mPPDevice->PPDeviceDeInit();
            Buf_input = mPPbuf[0];
        }else{
            Buf_input = mCaptureBuffers[DeQueBufIdx];
        }

        Buf_output.virt_start = (unsigned char *)(JpegImageHeap->getBase());
        CAMERA_HAL_LOG_INFO("Generated a picture");

        if (mMsgEnabled & CAMERA_MSG_SHUTTER) {
            CAMERA_HAL_LOG_INFO("CAMERA_MSG_SHUTTER");
            mNotifyCb(CAMERA_MSG_SHUTTER, 0, 0, mCallbackCookie);
        }

        if (mJpegEncoder->DoEncode(&Buf_input,&Buf_output,&JpegEncConf) < 0){
            ret = UNKNOWN_ERROR;
            goto Pic_out;
        }

        JpegMemBase = new MemoryBase(JpegImageHeap, 0, JpegEncConf.output_jpeg_size);

Pic_out:
        mCaptureDevice->DevStop();
        mCaptureDevice->DevDeAllocate();
        CloseCaptureDevice();

        if ((JpegMemBase != NULL) && (mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE)) {
            CAMERA_HAL_LOG_INFO("==========CAMERA_MSG_COMPRESSED_IMAGE==================");
            mDataCb(CAMERA_MSG_COMPRESSED_IMAGE, JpegMemBase, mCallbackCookie);
        }


        return ret;

    }

    int CameraHal :: GetJpegEncoderParam()
    {
        CAMERA_HAL_LOG_FUNC;
        int ret = NO_ERROR, i = 0;
        memset(mEncoderSupportedFormat, 0, sizeof(unsigned int)*MAX_QUERY_FMT_TIMES);

        for (i = 0; i < MAX_QUERY_FMT_TIMES; i++){
            if (mJpegEncoder->EnumJpegEncParam(SUPPORTED_FMT,&(mEncoderSupportedFormat[i])) < 0)
                break;
        }
        if (i == 0){
            CAMERA_HAL_ERR("Get the parameters error");
            return UNKNOWN_ERROR;
        }
        return ret;
    }
    int CameraHal :: NegotiateCaptureFmt(bool TakePicFlag)
    {
        CAMERA_HAL_LOG_FUNC;
        int ret = NO_ERROR, i = 0, j = 0;


        if(TakePicFlag){
            mPictureEncodeFormat = 0;
            for (i = 0; i < MAX_QUERY_FMT_TIMES; i++){
                for (j = 0; j < MAX_QUERY_FMT_TIMES; j++){
                    if (mEncoderSupportedFormat[j] == 0)
                        break;
                    if (mCaptureSupportedFormat[i] == mEncoderSupportedFormat[j]){
                        mPictureEncodeFormat= mCaptureSupportedFormat[i];

                        CAMERA_HAL_LOG_INFO(" Get the mPictureEncodeFormat :%c%c%c%c\n",
                                mPictureEncodeFormat & 0xFF, (mPictureEncodeFormat >> 8) & 0xFF,
                                (mPictureEncodeFormat >> 16) & 0xFF, (mPictureEncodeFormat >> 24) & 0xFF);
                        break;
                    }
                }
                if ((mPictureEncodeFormat != 0) || (mCaptureSupportedFormat[i] == 0))
                    break;
            }
            if (mPictureEncodeFormat == 0){
                mPictureEncodeFormat = mEncoderSupportedFormat[0];
                mCaptureDeviceCfg.fmt = mUvcSpecialCaptureFormat; //For uvc now, IPU only can support yuyv.
                mPPDeviceNeedForPic = true;
                CAMERA_HAL_LOG_INFO("Need to do the CSC for Jpeg encoder");
                CAMERA_HAL_LOG_INFO(" Get the captured format is :%c%c%c%c\n",
                        mCaptureDeviceCfg.fmt & 0xFF, (mCaptureDeviceCfg.fmt >> 8) & 0xFF,
                        (mCaptureDeviceCfg.fmt >> 16) & 0xFF, (mCaptureDeviceCfg.fmt >> 24) & 0xFF);
                CAMERA_HAL_LOG_INFO(" Get the mPictureEncodeFormat :%c%c%c%c\n",
                        mPictureEncodeFormat & 0xFF, (mPictureEncodeFormat >> 8) & 0xFF,
                        (mPictureEncodeFormat >> 16) & 0xFF, (mPictureEncodeFormat >> 24) & 0xFF);
            }else{
                mCaptureDeviceCfg.fmt = mPictureEncodeFormat;
            }
        }else{
            CAMERA_HAL_LOG_INFO("mPreviewFormat :%c%c%c%c\n",
                    mPreviewFormat & 0xFF, (mPreviewFormat >> 8) & 0xFF,
                    (mPreviewFormat >> 16) & 0xFF, (mPreviewFormat >> 24) & 0xFF);
            CAMERA_HAL_LOG_INFO("mUvcSpecialCaptureFormat :%c%c%c%c\n",
                    mUvcSpecialCaptureFormat & 0xFF, (mUvcSpecialCaptureFormat >> 8) & 0xFF,
                    (mUvcSpecialCaptureFormat >> 16) & 0xFF, (mUvcSpecialCaptureFormat >> 24) & 0xFF);

            for(i =0; i< MAX_QUERY_FMT_TIMES; i ++){
                CAMERA_HAL_LOG_RUNTIME("mCaptureSupportedFormat[%d] is %x", i, mCaptureSupportedFormat[i]);
                if (mCaptureSupportedFormat[i] == mPreviewFormat){
                    CAMERA_HAL_LOG_RUNTIME("get the correct format [%d] is %x", i, mCaptureSupportedFormat[i]);
                    mPPDeviceNeed = false;
                    mPreviewCapturedFormat = mPreviewFormat;
                    break;
                }
            }

            if (i == MAX_QUERY_FMT_TIMES){
                for(i =0; i< MAX_QUERY_FMT_TIMES; i ++){
                    //since for CSI, the CSI can convert to any YUV format if necessary, so specailly is just for UVC
                    if (mCaptureSupportedFormat[i] == mUvcSpecialCaptureFormat){
                        CAMERA_HAL_LOG_RUNTIME("get the correct format [%d] is %x", i, mCaptureSupportedFormat[i]);
                        mPPDeviceNeed = true;
                        mPreviewCapturedFormat = mUvcSpecialCaptureFormat;
                        break;
                    }
                }
            }

            CAMERA_HAL_LOG_INFO("mPreviewCapturedFormat :%c%c%c%c\n",
                    mPreviewCapturedFormat & 0xFF, (mPreviewCapturedFormat >> 8) & 0xFF,
                    (mPreviewCapturedFormat >> 16) & 0xFF, (mPreviewCapturedFormat >> 24) & 0xFF);

            if ((i == MAX_QUERY_FMT_TIMES)){
                CAMERA_HAL_ERR("Negotiate for the preview format error");
                return BAD_VALUE;
            }
        }


        return ret;
    }

    int CameraHal :: PrepareJpegEncoder()
    {
        int ret = NO_ERROR;
        struct jpeg_enc_make_info_t make_info;
        struct jpeg_enc_makernote_info_t makernote_info;
        struct jpeg_enc_model_info_t model_info;
        struct jpeg_enc_datetime_info_t datetime_info;
        struct jpeg_enc_focallength_t focallength_info;
        struct jpeg_enc_gps_param gps_info;
        int rotate_angle = 0;
        JPEG_ENCODER_WHITEBALANCE whitebalance_info;
        JPEG_ENCODER_FLASH flash_info;
        const char * pWhiteBalanceStr, *pFlashStr;

        char temp_string[30], gps_datetime_string[11];
        char format[30] = "%Y:%m:%d %k:%M:%S";
        time_t clock;
        struct tm *tm, *temp_tm;
        char * cLatitude, *cLongtitude, *cAltitude,*cTimeStamp;
        double dAltitude;

        mJpegEncCfg.BufFmt = mPictureEncodeFormat;
        mParameters.getPictureSize((int *)&(mJpegEncCfg.PicWidth), (int *)&(mJpegEncCfg.PicHeight));
        mJpegEncCfg.ThumbWidth = (unsigned int)mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_WIDTH);
        mJpegEncCfg.ThumbHeight =(unsigned int)mParameters.getInt(CameraParameters::KEY_JPEG_THUMBNAIL_HEIGHT);
        CAMERA_HAL_LOG_INFO("the pic width %d, height %d, fmt %d", mJpegEncCfg.PicWidth, mJpegEncCfg.PicHeight, mJpegEncCfg.BufFmt);
        CAMERA_HAL_LOG_INFO("the thumbnail width is %d, height is %d", mJpegEncCfg.ThumbWidth, mJpegEncCfg.ThumbHeight);
        //set focallength info
        focallength_info.numerator=10001;
        focallength_info.denominator=1000;  // hardcode here for the cts
        mJpegEncCfg.pFoclLength = &focallength_info;

        //set the make info
        make_info.make_bytes=strlen(EXIF_MAKENOTE);
        strcpy((char *)make_info.make, EXIF_MAKENOTE);
        mJpegEncCfg.pMakeInfo = &make_info;

        //set makernote info
        makernote_info.makernote_bytes=strlen(EXIF_MAKENOTE);
        strcpy((char *)makernote_info.makernote, EXIF_MAKENOTE);
        mJpegEncCfg.pMakeNote = &makernote_info;

        //set model info
        model_info.model_bytes=strlen(EXIF_MODEL);
        strcpy((char *)model_info.model,EXIF_MODEL);
        mJpegEncCfg.pModelInfo = &model_info;

        //set datetime
        time(&clock);
        tm = localtime(&clock);
        time_t GpsUtcTime;
        strftime(temp_string, sizeof(temp_string), format, tm);
        CAMERA_HAL_LOG_INFO("the date time is %s", temp_string);
        memcpy((char *)datetime_info.datetime, temp_string, sizeof(datetime_info.datetime));
        mJpegEncCfg.pDatetimeInfo = &datetime_info;

        rotate_angle = mParameters.getInt(CameraParameters::KEY_ROTATION);
        if (rotate_angle == 0)
            mJpegEncCfg.RotationInfo = ORIENTATION_NORMAL; //the android and the jpeg has the same define
        else if (rotate_angle == 90)
            mJpegEncCfg.RotationInfo = ORIENTATION_ROTATE_90;
        else if (rotate_angle == 180)
            mJpegEncCfg.RotationInfo = ORIENTATION_ROTATE_180;
        else if (rotate_angle == 270)
            mJpegEncCfg.RotationInfo = ORIENTATION_ROTATE_270;
        else
            mJpegEncCfg.RotationInfo = ORIENTATION_NORMAL;
        CAMERA_HAL_LOG_INFO("ratate info is %d", rotate_angle);

        pWhiteBalanceStr = mParameters.get(CameraParameters::KEY_WHITE_BALANCE);
        CAMERA_HAL_LOG_INFO("white balance is %s",pWhiteBalanceStr);
        if (strcmp(pWhiteBalanceStr, CameraParameters::WHITE_BALANCE_AUTO) == 0){
            whitebalance_info = WHITEBALANCE_AUTO;
        }else{
            whitebalance_info = WHITEBALANCE_MANUAL;
        }
        mJpegEncCfg.WhiteBalanceInfo = whitebalance_info;

        pFlashStr = mParameters.get(CameraParameters::KEY_FLASH_MODE);
        CAMERA_HAL_LOG_INFO("flash mode is %s", pFlashStr);
        if (strcmp(pFlashStr, CameraParameters::FLASH_MODE_OFF) == 0){
            flash_info = FLASH_NOT_FIRE;
        }else if (strcmp(pFlashStr, CameraParameters::FLASH_MODE_AUTO) == 0){
            flash_info = FLASH_FIRED_AUTO;
        }else if (strcmp(pFlashStr, CameraParameters::FLASH_MODE_ON) == 0){
            flash_info = FLASH_FIRED;
        }else if (strcmp(pFlashStr, CameraParameters::FLASH_MODE_RED_EYE) == 0){
            flash_info = FLASH_FIRED_RED_EYE_REDUCE;
        }
        else if (strcmp(pFlashStr, CameraParameters::FLASH_MODE_TORCH) == 0){
            flash_info = FLASH_FIRED_COMPULOSORY;
        }
        else{
            flash_info = FLASH_NOT_FIRE;
        }
        mJpegEncCfg.FlashInfo = flash_info;

        cLatitude   = (char *)mParameters.get(CameraParameters::KEY_GPS_LATITUDE);
        cLongtitude = (char *)mParameters.get(CameraParameters::KEY_GPS_LONGITUDE);
        cAltitude   = (char *)mParameters.get(CameraParameters::KEY_GPS_ALTITUDE);
        cTimeStamp  = (char *)mParameters.get(CameraParameters::KEY_GPS_TIMESTAMP);

        if (cLatitude !=NULL && cLatitude!=NULL && cAltitude!=NULL && cTimeStamp!=NULL){

            gps_info.version=0x02020000;

            //latitude: dd/1,mm/1,ss/1
            gps_info.latitude_degree[1]=1;
            gps_info.latitude_minute[1]=1;
            gps_info.latitude_second[1]=1;
            memcpy((char *)gps_info.latitude_ref, (char *)"N ", sizeof(gps_info.latitude_ref));

            if (stringTodegree(cLatitude, gps_info.latitude_degree[0],gps_info.latitude_minute[0],gps_info.latitude_second[0])>0){
                //the ref is south
                memcpy((char *)gps_info.latitude_ref, (char *)"S ", sizeof(gps_info.latitude_ref));
            }

            //longtitude: dd/1,mm/1,ss/1
            gps_info.longtitude_degree[1]=1;
            gps_info.longtitude_minute[1]=1;
            gps_info.longtitude_second[1]=1;
            memcpy((char *)gps_info.longtitude_ref, (char *)"E ", sizeof(gps_info.longtitude_ref));

            if (stringTodegree(cLatitude, gps_info.longtitude_degree[0],gps_info.longtitude_minute[0],gps_info.longtitude_second[0])>0){
                //the ref is Weston
                memcpy((char *)gps_info.longtitude_ref, (char *)"W ", sizeof(gps_info.longtitude_ref));
            }

            //altitude(meters): aa/1
            gps_info.altitude_ref=0;		// 0: up sea level; 1: below sea level
            gps_info.altitude[0]=1000;
            gps_info.altitude[1]=1;
            if (cAltitude != NULL){
                int intValue;
                gps_info.altitude[1]=1000;	   // the precision is CM
                dAltitude= atof(cAltitude);
                CAMERA_HAL_LOG_RUNTIME("the altitude is %s", cAltitude);
                intValue = (int)(dAltitude * 1000.0);
                if (intValue<0) {gps_info.altitude_ref = 1; intValue *= -1;}
                gps_info.altitude[0] = (unsigned long) intValue;
                CAMERA_HAL_LOG_RUNTIME("gps_info.altitude[0] is %u, gps_info.altitude_ref is %d", gps_info.altitude[0], gps_info.altitude_ref);
            }

            //timestamp: hh/1,mm/1,ss/1
            gps_info.hour[1]=1;
            gps_info.minute[1]=1;
            gps_info.seconds[1]=1;
            if (cTimeStamp != NULL){

                GpsUtcTime = atol(cTimeStamp);
                CAMERA_HAL_LOG_INFO("the Timestamp is %s", cTimeStamp);
                temp_tm = gmtime((const time_t*)&GpsUtcTime);
                if (temp_tm != NULL)
                    tm = temp_tm;
            }

            gps_info.hour[0] = tm->tm_hour;
            gps_info.minute[0] = tm->tm_min;
            gps_info.seconds[0] = tm->tm_sec;

            strcpy (format, "%Y:%m:%d ");


            strftime((char *)temp_string, strlen(temp_string), format, tm);
            memcpy(gps_info.datestamp, temp_string, sizeof(gps_info.datestamp));


            char * progressMehod = (char *)mParameters.get(CameraParameters::KEY_GPS_PROCESSING_METHOD);
            if (progressMehod == NULL){
                CAMERA_HAL_LOG_INFO("The progressMethod is NULL, add a fake");
                progressMehod = (char *)"fsl_fake_method";
            }
            CAMERA_HAL_LOG_INFO("the progressMethod is %s", progressMehod);

            memcpy(gps_info.processmethod, progressMehod, strlen(progressMehod));

            gps_info.processmethod_bytes=strlen(progressMehod);

            CAMERA_HAL_LOG_INFO("the method is %s", gps_info.processmethod);

            mJpegEncCfg.pGps_info = &gps_info;
        }else{
            mJpegEncCfg.pGps_info = NULL;
        }

        if (mJpegEncoder->JpegEncoderInit(&mJpegEncCfg)< 0){
            CAMERA_HAL_ERR("Jpeg Encoder Init error !!!");
            return UNKNOWN_ERROR;
        }

        return ret;
    }
    status_t CameraHal::CameraHALStartPreview()
    {
        CAMERA_HAL_LOG_FUNC;
        status_t ret = NO_ERROR;
        int  max_fps, min_fps;
        mParameters.getPreviewSize((int *)&(mCaptureDeviceCfg.width),(int *)&(mCaptureDeviceCfg.height));
        mCaptureDeviceCfg.fmt = mPreviewCapturedFormat;
        mCaptureDeviceCfg.rotate = (SENSOR_PREVIEW_ROTATE)mPreviewRotate;
        mCaptureDeviceCfg.tv.numerator = 1;
        mCaptureDevice->GetDevName(mCameraSensorName);
        if (strstr(mCameraSensorName, "uvc") == NULL){
        //according to google's doc getPreviewFrameRate & getPreviewFpsRange should support both.
        // so here just a walkaround, if the app set the frameRate, will follow this frame rate.
        if (mParameters.getPreviewFrameRate() >= 15)
            mCaptureDeviceCfg.tv.denominator = mParameters.getPreviewFrameRate();
        else{
            mParameters.getPreviewFpsRange(&min_fps, &max_fps);
            CAMERA_HAL_LOG_INFO("###start the capture the fps is %d###", max_fps);
            mCaptureDeviceCfg.tv.denominator = max_fps/1000;
        }
        }else{
                mCaptureDeviceCfg.tv.denominator = 15;
        }
        mCaptureBufNum = PREVIEW_CAPTURE_BUFFER_NUM;
        mPPbufNum = POST_PROCESS_BUFFER_NUM;
        mTakePicFlag = false;

        if ((ret = PrepareCaptureDevices()) < 0){
            CAMERA_HAL_ERR("PrepareCaptureDevices error ");
            return ret;
        }
        if (mPPDeviceNeed){
            if ((ret = PreparePostProssDevice()) < 0){
                CAMERA_HAL_ERR("PreparePostProssDevice error");
                return ret;
            }
        }
        if ((ret = PreparePreviwBuf()) < 0){
            CAMERA_HAL_ERR("PreparePreviwBuf error");
            return ret;
        }

        if ((ret = PreparePreviwMisc()) < 0){
            CAMERA_HAL_ERR("PreparePreviwMisc error");
            return ret;
        }

        if ((ret = CameraHALPreviewStart()) < 0){
            CAMERA_HAL_ERR("CameraHALPreviewStart error");
            return ret;
        }
        return ret;
    }
    void CameraHal::CameraHALStopPreview()
    {
        CAMERA_HAL_LOG_FUNC;
        if (mPreviewRunning != 0)	{
            CameraHALStopThreads();
            CameraHALStopMisc();
            CAMERA_HAL_LOG_INFO("camera hal stop preview done");
        }else{
            CAMERA_HAL_LOG_INFO("Camera hal already stop preview");
        }
        return ;
    }

    void CameraHal :: CameraHALStopThreads()
    {
        CAMERA_HAL_LOG_FUNC;
        mPreviewRunning = 0;
        if (mCaptureFrameThread!= 0){
            mCaptureFrameThread->requestExitAndWait();
            mCaptureFrameThread.clear();
        }
        if (mPPDeviceNeed){
            if (mPostProcessThread!= 0){
                mPostProcessThread->requestExitAndWait();
                mPostProcessThread.clear();
            }
        }
        if (mPreviewShowFrameThread!= 0){
            mPreviewShowFrameThread->requestExitAndWait();
            mPreviewShowFrameThread.clear();
        }

        if (mEncodeFrameThread!= 0){
            mEncodeFrameThread->requestExitAndWait();
            mEncodeFrameThread.clear();
        }
        return ;
    }

    void CameraHal :: CameraHALStopMisc()
    {
        CAMERA_HAL_LOG_FUNC;
        sem_destroy(&avab_dequeue_frame);
        sem_destroy(&avab_show_frame);
        sem_destroy(&avab_enc_frame);
        sem_destroy(&avab_enc_frame_finish);
        if(mPPDeviceNeed){
            sem_destroy(&avab_pp_in_frame);
            sem_destroy(&avab_pp_out_frame);
        }
        if(mPPDeviceNeed){
            for (unsigned int i = 0; i < mPPbufNum; i++){
                mPmemAllocator->deAllocate(&mPPbuf[i]);
            }
            mPmemAllocator = NULL;
        }
        mCaptureDevice->DevStop();
        mCaptureDevice->DevDeAllocate();
        CloseCaptureDevice();

    }
    status_t CameraHal :: PrepareCaptureDevices()
    {
        CAMERA_HAL_LOG_FUNC;
        status_t ret = NO_ERROR;
        int i =0;
        unsigned int CaptureBufNum = mCaptureBufNum;
        struct capture_config_t *pCapcfg;
        if ((ret = OpenCaptureDevice())<0)
            return ret;

        if (mCaptureDevice->DevSetConfig(&mCaptureDeviceCfg) < 0) {//set the config and get the captured framesize
            CAMERA_HAL_ERR("Dev config failed");
            return BAD_VALUE;
        }
        mCaptureFrameSize = mCaptureDeviceCfg.framesize;

        if (mCaptureDevice->DevAllocateBuf(mCaptureBuffers,&CaptureBufNum)< 0){
            CAMERA_HAL_ERR("capture device allocat buf error");
            return BAD_VALUE;
        }
        if(mCaptureBufNum != CaptureBufNum){
            CAMERA_HAL_LOG_INFO("The driver can only supply %d bufs, but required %d bufs", CaptureBufNum, mCaptureBufNum);
        }

        mCaptureBufNum = CaptureBufNum;

        if (mCaptureDevice->DevPrepare()< 0){
            CAMERA_HAL_ERR("capture device prepare error");
            return BAD_VALUE;
        }
        nCameraBuffersQueued = mCaptureBufNum;

        return ret;
    }

    status_t CameraHal::PreparePostProssDevice()
    {

        CAMERA_HAL_LOG_FUNC;
        status_t ret = NO_ERROR;
        unsigned int targetFmt;
        if (mTakePicFlag)
            targetFmt = mPictureEncodeFormat;
        else
            targetFmt = mPreviewFormat;

        pthread_mutex_lock(&mPPIOParamMutex);
        mPPInputParam.width = mCaptureDeviceCfg.width;
        mPPInputParam.height= mCaptureDeviceCfg.height;
        mPPInputParam.fmt   = mCaptureDeviceCfg.fmt;
        mPPInputParam.input_crop_win.pos.x = 0;
        mPPInputParam.input_crop_win.pos.y = 0;
        mPPInputParam.input_crop_win.win_w = mCaptureDeviceCfg.width;
        mPPInputParam.input_crop_win.win_h = mCaptureDeviceCfg.height;

        mPPOutputParam.width = mCaptureDeviceCfg.width;
        mPPOutputParam.height= mCaptureDeviceCfg.height;
        mPPOutputParam.fmt   = targetFmt;
        mPPOutputParam.rot   = 0;
        mPPOutputParam.output_win.pos.x = 0;
        mPPOutputParam.output_win.pos.y = 0;
        mPPOutputParam.output_win.win_w = mCaptureDeviceCfg.width;
        mPPOutputParam.output_win.win_h = mCaptureDeviceCfg.height;
        pthread_mutex_unlock(&mPPIOParamMutex);
        return ret;
    }

    status_t CameraHal::PreparePreviwBuf()
    {
        CAMERA_HAL_LOG_FUNC;
        status_t ret = NO_ERROR;
        unsigned int i =0;

        //temply hard code here
        if (mTakePicFlag == 0){
            if(V4L2_PIX_FMT_NV12)
                mPreviewFrameSize = mCaptureDeviceCfg.width*mCaptureDeviceCfg.height*3/2;
            else 
                mPreviewFrameSize = mCaptureDeviceCfg.width*mCaptureDeviceCfg.height *2;

            //now the preview fmt is supposed to be YUV420SP, so, it is now hard code here
            mPreviewHeap.clear();
            for (i = 0; i< mPreviewHeapBufNum; i++)
                mPreviewBuffers[i].clear();
            mPreviewHeap = new MemoryHeapBase(mPreviewFrameSize * mPreviewHeapBufNum);
            if (mPreviewHeap == NULL)
                return NO_MEMORY;
            for (i = 0; i < mPreviewHeapBufNum; i++)
                mPreviewBuffers[i] = new MemoryBase(mPreviewHeap, mPreviewFrameSize* i, mPreviewFrameSize);
        }
        /*allocate the buffer for IPU process*/
        if (mPPDeviceNeed || mPPDeviceNeedForPic){
            mPmemAllocator = new PmemAllocator(mPPbufNum, mCaptureFrameSize);

            if(mPmemAllocator == NULL || mPmemAllocator->err_ret < 0){
                return NO_MEMORY;
            }
            for (i = 0; i < mPPbufNum; i++){
                if(mPmemAllocator->allocate(&(mPPbuf[i]),mCaptureFrameSize) < 0){
                    return NO_MEMORY;
                }
            }
        }
        return ret;
    }

    status_t CameraHal ::PreparePreviwMisc()
    {
        CAMERA_HAL_LOG_FUNC;
        status_t ret = NO_ERROR;
        dequeue_head = 0;
        preview_heap_buf_head = 0;
        display_head = 0;
        enc_head     = 0;
        pp_in_head   = 0;
        pp_out_head  = 0;
        error_status = 0;
        is_first_buffer = 1;
        last_display_index = 0;

        sem_init(&avab_dequeue_frame, 0, mCaptureBufNum);
        sem_init(&avab_show_frame, 0, 0);
        sem_init(&avab_enc_frame, 0, 0);
        sem_init(&avab_enc_frame_finish, 0, 0);
        if(mPPDeviceNeed){
            sem_init(&avab_pp_in_frame, 0, 0);
            sem_init(&avab_pp_out_frame, 0, mPPbufNum);
        }
        return ret;
    }

    status_t CameraHal ::CameraHALPreviewStart()
    {
        CAMERA_HAL_LOG_FUNC;
        status_t ret = NO_ERROR;
        if (mCaptureDevice->DevStart()<0)
            return INVALID_OPERATION;

        mCaptureFrameThread = new CaptureFrameThread(this);
        mPreviewShowFrameThread = new PreviewShowFrameThread(this);
        mEncodeFrameThread = new EncodeFrameThread(this);
        if(mPPDeviceNeed){
            mPostProcessThread = new PostProcessThread(this);
            if (mPostProcessThread == NULL)
                return UNKNOWN_ERROR;
        }

        if (mCaptureFrameThread == NULL ||
                mPreviewShowFrameThread == NULL ||
                mEncodeFrameThread == NULL){
            return UNKNOWN_ERROR;
        }

        mPreviewRunning = true;
        return ret;
    }


    int CameraHal ::captureframeThread()
    {
        CAMERA_HAL_LOG_FUNC;

        unsigned int DeqBufIdx = 0;
        struct timespec ts;

        do {
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec +=100000; // 100ms
        } while (mPreviewRunning && !error_status &&(sem_timedwait(&avab_dequeue_frame, &ts) != 0) );

        if(!mPreviewRunning || error_status)
            return UNKNOWN_ERROR;

        mCaptureDevice->DevDequeue(&DeqBufIdx);

        nCameraBuffersQueued--;

        buffer_index_maps[dequeue_head]=DeqBufIdx;
        dequeue_head ++;
        dequeue_head %= mCaptureBufNum;

        if(!mPPDeviceNeed){
            sem_post(&avab_show_frame);
            sem_post(&avab_enc_frame);
        }else{
            sem_post(&avab_pp_in_frame);
        }

        return NO_ERROR;
    }

    int CameraHal::postprocessThread()
    {
        CAMERA_HAL_LOG_FUNC;
        int PPInIdx = 0, PPoutIdx = 0;
        DMA_BUFFER PPInBuf, PPoutBuf;
        struct timespec ts;

        do {
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec +=100000; // 100ms
        } while (mPreviewRunning && !error_status &&(sem_timedwait(&avab_pp_in_frame, &ts) != 0) );

        do {
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec +=100000; // 100ms
        } while (mPreviewRunning && !error_status &&(sem_timedwait(&avab_pp_out_frame, &ts) != 0) );


        if(!mPreviewRunning || error_status)
            return UNKNOWN_ERROR;
        PPInIdx = buffer_index_maps[pp_in_head];
        PPInBuf = mCaptureBuffers[PPInIdx];
        pp_in_head ++;
        pp_in_head %= mCaptureBufNum;

        PPoutIdx = pp_out_head;
        PPoutBuf = mPPbuf[PPoutIdx];
        pp_out_head ++;
        pp_out_head %= mPPbufNum;

        pthread_mutex_lock(&mPPIOParamMutex);
        mPPInputParam.user_def_paddr = PPInBuf.phy_offset;
        mPPOutputParam.user_def_paddr = PPoutBuf.phy_offset;
        mPPDevice->PPDeviceInit(&mPPInputParam, &mPPOutputParam);
        mPPDevice->DoPorcess(&PPInBuf, &PPoutBuf);
        mPPDevice->PPDeviceDeInit();
        pthread_mutex_unlock(&mPPIOParamMutex);

        sem_post(&avab_show_frame);
        sem_post(&avab_enc_frame);

        if (mCaptureDevice->DevQueue(PPInIdx) < 0){
            CAMERA_HAL_ERR("queue buf back error");
            return INVALID_OPERATION;
        }
        nCameraBuffersQueued ++;
        sem_post(&avab_dequeue_frame);

        return NO_ERROR;
    }
    int CameraHal ::previewshowFrameThread()
    {
        CAMERA_HAL_LOG_FUNC;
        struct timespec ts;
        int display_index = 0;
        DMA_BUFFER InBuf;
        int queue_back_index = 0;

        do {
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec +=100000; // 100ms
        } while (!error_status && mPreviewRunning &&(sem_timedwait(&avab_show_frame, &ts) != 0) );

        if ((mPreviewRunning == 0) || error_status)
            return UNKNOWN_ERROR;

        if (!mPPDeviceNeed){
            display_index = buffer_index_maps[display_head];
            InBuf = mCaptureBuffers[display_index];
            display_head ++;
            display_head %= mCaptureBufNum;
        }else{
            display_index = display_head;
            InBuf = mPPbuf[display_index];
            display_head ++;
            display_head %= mPPbufNum;
        }

        if (mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME) {
            convertNV12toYUV420SP((uint8_t*)(InBuf.virt_start),
                    (uint8_t*)(mPreviewBuffers[preview_heap_buf_head]->pointer()),mCaptureDeviceCfg.width, mCaptureDeviceCfg.height);
            mDataCb(CAMERA_MSG_PREVIEW_FRAME, mPreviewBuffers[preview_heap_buf_head], mCallbackCookie);
            preview_heap_buf_head ++;
            preview_heap_buf_head %= mPreviewHeapBufNum;
        }

        pthread_mutex_lock(&mOverlayMutex);

        if (mOverlay != 0) {
            if (mOverlay->queueBuffer((overlay_buffer_t)InBuf.phy_offset) < 0){
                CAMERA_HAL_ERR("queueBuffer failed. May be bcos stream was not turned on yet.");
            }

            if (is_first_buffer) {
                is_first_buffer = 0;
                last_display_index = display_index;
                pthread_mutex_unlock(&mOverlayMutex);
                goto show_out;
            }
        }

        if (!mPPDeviceNeed){
            if (mOverlay != 0){
                queue_back_index = last_display_index;
            }else{
                queue_back_index = display_index;
            }
        }
        pthread_mutex_unlock(&mOverlayMutex);

        do {
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec +=200000; // 100ms
        } while ((sem_timedwait(&avab_enc_frame_finish, &ts) != 0)&&!error_status && mPreviewRunning );

        if (!mPPDeviceNeed){
            //queue the v4l2 buf back
            if(mCaptureDevice->DevQueue(queue_back_index) <0){
                CAMERA_HAL_ERR("The Capture device queue buf error !!!!");
                return INVALID_OPERATION;
            }
            last_display_index = display_index;
            nCameraBuffersQueued++;
            sem_post(&avab_dequeue_frame);
        }else{
            sem_post(&avab_pp_out_frame);
        }

show_out:

        return NO_ERROR;
    }

    int CameraHal :: encodeframeThread()
    {
        CAMERA_HAL_LOG_FUNC;
        struct timespec ts;
        unsigned int enc_index = 0, i = 0;
        DMA_BUFFER EncBuf;

        do {
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec +=100000; // 100ms
        } while ((sem_timedwait(&avab_enc_frame, &ts) != 0) && !error_status &&  mPreviewRunning );

        if ((mPreviewRunning == 0) || error_status)
            return UNKNOWN_ERROR;

        if (!mPPDeviceNeed){
            enc_index = buffer_index_maps[enc_head];
            EncBuf = mCaptureBuffers[enc_index];
            enc_head ++;
            enc_head %= mCaptureBufNum;
        }else{
            enc_index = enc_head;
            EncBuf = mPPbuf[enc_index];
            enc_head ++;
            enc_head %= mPPbufNum;
        }

        if ((mMsgEnabled & CAMERA_MSG_VIDEO_FRAME) && mRecordRunning) {
            nsecs_t timeStamp = systemTime(SYSTEM_TIME_MONOTONIC);
            for(i = 0 ; i < mVideoBufNume; i ++) {
                if(mVideoBufferUsing[i] == 0) {
                    memcpy(mVideoBuffers[i]->pointer(),
                            (void*)EncBuf.virt_start, mPreviewFrameSize);

                    mVideoBufferUsing[i] = 1;
                    mDataCbTimestamp(timeStamp, CAMERA_MSG_VIDEO_FRAME, mVideoBuffers[i], mCallbackCookie);
                    break;
                }
            }
            if (i == mVideoBufNume)
                CAMERA_HAL_LOG_INFO("no Buffer can be used for record\n");
        }

        sem_post(&avab_enc_frame_finish);
        return NO_ERROR;

    }


    status_t CameraHal :: AllocateRecordVideoBuf()
    {
        status_t ret = NO_ERROR;
        unsigned int i = 0;
        mVideoHeap.clear();
        for(i = 0; i < mVideoBufNume; i++) {
            mVideoBuffers[i].clear();
            mVideoBufferUsing[i] = 0;
        }

        CAMERA_HAL_LOG_RUNTIME("Init the video Memory size %d", mPreviewFrameSize);
        mVideoHeap = new MemoryHeapBase(mPreviewFrameSize * mVideoBufNume);
        if (mVideoHeap == NULL)
            return NO_MEMORY;
        for(i = 0; i < mVideoBufNume; i++) {
            CAMERA_HAL_LOG_RUNTIME("Init Video Buffer:%d ",i);
            mVideoBuffers[i] = new MemoryBase(mVideoHeap,
                    mPreviewFrameSize * i, mPreviewFrameSize);
        }

        return ret;
    }


    void CameraHal :: LockWakeLock()
    {
        if (!mPowerLock) {
            acquire_wake_lock (PARTIAL_WAKE_LOCK, V4LSTREAM_WAKE_LOCK);
            mPowerLock = true;
        }
    }
    void CameraHal :: UnLockWakeLock()
    {
        if (mPowerLock) {
            release_wake_lock (V4LSTREAM_WAKE_LOCK);
            mPowerLock = false;
        }
    }

    void CameraHal::convertNV12toYUV420SP(uint8_t *inputBuffer, uint8_t *outputBuffer, int width, int height)
    {
        /* Color space conversion from I420 to YUV420SP */
        int Ysize = 0, UVsize = 0;
        uint8_t *Yin, *Uin, *Vin, *Yout, *Uout, *Vout;

        Ysize = width * height;
        UVsize = width *  height >> 2;

        Yin = inputBuffer;
        Uin = Yin + Ysize;
        Vin = Uin + 1;

        Yout = outputBuffer;
        Vout = Yout + Ysize;
        Uout = Vout + 1;

        memcpy(Yout, Yin, Ysize);

        for(int k = 0; k < UVsize; k++) {
            *Uout = *Uin;
            *Vout = *Vin;
            Uout += 2;
            Vout += 2;
            Uin  += 2;
            Vin += 2;
        }
    }



    int CameraHal::stringTodegree(char* cAttribute, unsigned int &degree, unsigned int &minute, unsigned int &second)
    {
        double dAttribtute;
        long intAttribute;
        int ret  = 0;
        if (cAttribute == NULL){
            return -1;
        }

        CAMERA_HAL_LOG_RUNTIME("the attribute is %s", cAttribute);

        dAttribtute = atof(cAttribute);

        CAMERA_HAL_LOG_RUNTIME("the double of the attribute is %lf", dAttribtute);
        intAttribute  = (long)(dAttribtute*(double)3600.0);
        if (intAttribute < 0){
            ret = 1;
            intAttribute *=-1;
        }

        second = (unsigned int)(intAttribute%60);
        minute = (unsigned int)((intAttribute%3600-second)/60);
        degree = (unsigned int)(intAttribute/3600);

        CAMERA_HAL_LOG_RUNTIME("the degree is %u, %u, %u", degree,minute,second);

        return ret;

    }


#define FACE_BACK_CAMERA_NAME "back_camera_name"
#define FACE_FRONT_CAMERA_NAME "front_camera_name"
#define FACE_BACK_CAMERA_ORIENT "back_camera_orient"
#define FACE_FRONT_CAMERA_ORIENT "front_camera_orient"
#define DEFAULT_ERROR_NAME '#'
#define DEFAULT_ERROR_NAME_str "#"
#define UVC_NAME "uvc"
    static CameraInfo sCameraInfo[2];
    static char Camera_name[2][MAX_SENSOR_NAME];

    static void GetCameraPropery(char * pFaceBackCameraName, char *pFaceFrontCameraName, int *pFaceBackOrient, int *pFaceFrontOrient)
    {
        char orientStr[10];

        property_get (FACE_BACK_CAMERA_NAME,
                pFaceBackCameraName,
                DEFAULT_ERROR_NAME_str );
        property_get (FACE_BACK_CAMERA_ORIENT,
                orientStr,
                DEFAULT_ERROR_NAME_str );

        if (orientStr[0] == DEFAULT_ERROR_NAME )
            *pFaceBackOrient = 0;
        else 
            *pFaceBackOrient = atoi(orientStr);

        LOGI("Face Back Camera is %s, orient is %d", pFaceBackCameraName, *pFaceBackOrient);

        property_get (FACE_FRONT_CAMERA_NAME,
                pFaceFrontCameraName,
                DEFAULT_ERROR_NAME_str );

        property_get (FACE_FRONT_CAMERA_ORIENT,
                orientStr,
                DEFAULT_ERROR_NAME_str );


        if (orientStr[0] == DEFAULT_ERROR_NAME )
            *pFaceFrontOrient = 0;
        else 
            *pFaceFrontOrient = atoi(orientStr);

        LOGI("Face Front Camera is %s, orient is %d", pFaceFrontCameraName, *pFaceFrontOrient);

    }

    int HAL_getNumberOfCameras()
    {
        int back_orient =0,  front_orient = 0;
        int back_camera_num = 0, front_camera_num = 0;
        GetCameraPropery(Camera_name[0], Camera_name[1], &back_orient, &front_orient);
        if (Camera_name[0][0] != DEFAULT_ERROR_NAME){
            sCameraInfo[0].facing = CAMERA_FACING_BACK;
            sCameraInfo[0].orientation = back_orient;
            back_camera_num++;
        }
        if (Camera_name[1][0] != DEFAULT_ERROR_NAME){
            if(back_camera_num > 0){
                sCameraInfo[1].facing = CAMERA_FACING_FRONT;
                sCameraInfo[1].orientation = front_orient;
            }else{
                sCameraInfo[0].facing = CAMERA_FACING_FRONT;
                sCameraInfo[0].orientation = front_orient;
            }
            front_camera_num ++;
        }
        return (back_camera_num + front_camera_num);					

    }

    void HAL_getCameraInfo(int cameraId, struct CameraInfo* cameraInfo)
    {
        memcpy(cameraInfo, &sCameraInfo[cameraId], sizeof(CameraInfo));					
    }

    sp<CameraHardwareInterface> HAL_openCameraHardware(int cameraId)
    {
        char *SelectedCameraName;
        int back_camera_num = 0, front_camera_num = 0;
        sp<CaptureDeviceInterface> pCaptureDevice = NULL;
        sp<PostProcessDeviceInterface> pPPDevice = NULL;
        sp<JpegEncoderInterface>pJpegEncoder = NULL;

        if (HAL_getNumberOfCameras() ==0 ){
            CAMERA_HAL_ERR("There is no configure for Cameras");
            return NULL;
        }

        SelectedCameraName = Camera_name[sCameraInfo[cameraId].facing];

        pCaptureDevice = createCaptureDevice(SelectedCameraName);
        pPPDevice = createPPDevice();
        pJpegEncoder = createJpegEncoder(SOFTWARE_JPEG_ENC);

        CameraHal *pCameraHal = new CameraHal();
        if (pCameraHal->setCaptureDevice(pCaptureDevice) < 0 ||
                pCameraHal->setPostProcessDevice(pPPDevice) < 0 ||
                pCameraHal->setJpegEncoder(pJpegEncoder) < 0)
            return NULL;

        if (pCameraHal->Init() < 0)
            return NULL;

        //now the board has only one csi camera sensor, so just do mirror for it
        if(strstr(SelectedCameraName, "ov") != NULL){
            pCameraHal->setPreviewRotate(CAMERA_PREVIEW_BACK_REF);
        }

        sp<CameraHardwareInterface> hardware(pCameraHal);
        CAMERA_HAL_LOG_INFO("created the fsl Camera hal");

        return hardware;
    }

};



