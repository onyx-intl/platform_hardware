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


#ifndef CAMERA_HAL_BASE_H
#define CAMERA_HAL_BASE_H

#include <string.h>
#include <unistd.h>
#include <time.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <utils/threads.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <camera/CameraHardwareInterface.h>
#include <ui/Overlay.h>
#include <semaphore.h>

#include "Camera_pmem.h"
#include "CaptureDeviceInterface.h"
#include "PostProcessDeviceInterface.h"
#include "JpegEncoderInterface.h"


#define EXIF_MAKENOTE "fsl_makernote"
#define EXIF_MODEL    "fsl_model"

#define CAMER_PARAM_BUFFER_SIZE 512
#define MAX_QUERY_FMT_TIMES 20
#define PARAMS_DELIMITER ","
#define V4LSTREAM_WAKE_LOCK "V4LCapture"
#define MAX_SENSOR_NAME 32

#define PREVIEW_HEAP_BUF_NUM    5
#define VIDEO_OUTPUT_BUFFER_NUM 5
#define POST_PROCESS_BUFFER_NUM 5
#define TAKE_PIC_QUE_BUF_NUM 5

#define PREVIEW_CAPTURE_BUFFER_NUM 5
#define PICTURE_CAPTURE_BUFFER_NUM 3

namespace android {

    typedef enum{
        CAMERA_HAL_ERR_NONE = 0,
        CAMERA_HAL_ERR_OPEN_CAPTURE_DEVICE = -1,
        CAMERA_HAL_ERR_GET_PARAM           = -2,
        CAMERA_HAL_ERR_BAD_PARAM =-3,
        CAMERA_HAL_ERR_BAD_ALREADY_RUN = -4,
        CAMERA_HAL_ERR_INIT = -5,
        CAMERA_HAL_ERR_ALLOC_BUF =-6,
        CAMERA_HAL_ERR_PP_NULL = -7
    }CAMERA_HAL_ERR_RET;

	typedef enum{
        CAMERA_PREVIEW_BACK_REF = 0,
        CAMERA_PREVIEW_VERT_FLIP = 1,
        CAMERA_PREVIEW_HORIZ_FLIP = 2,
        CAMERA_PREVIEW_ROATE_180 = 3,
        CAMERA_PREVIEW_ROATE_LAST = 3
	}CAMERA_PREVIEW_ROTATE;

    class CameraHal : public CameraHardwareInterface {
    public:
        virtual sp<IMemoryHeap> getPreviewHeap() const;
        virtual sp<IMemoryHeap> getRawHeap() const;

        virtual void        setCallbacks(notify_callback notify_cb,
                data_callback data_cb,
                data_callback_timestamp data_cb_timestamp,
                void* user);

        virtual void        enableMsgType(int32_t msgType);
        virtual void        disableMsgType(int32_t msgType);
        virtual bool        msgTypeEnabled(int32_t msgType);

        virtual bool        useOverlay() { return true; }
        virtual status_t    setOverlay(const sp<Overlay> &overlay);

        virtual status_t    startPreview();
        virtual void        stopPreview();
        virtual bool        previewEnabled();

        virtual status_t    startRecording();
        virtual void        stopRecording();
        virtual bool        recordingEnabled();
        virtual void        releaseRecordingFrame(const sp<IMemory>& mem);

        virtual status_t    autoFocus();
        virtual status_t    cancelAutoFocus();
        virtual status_t    takePicture();
        virtual status_t    cancelPicture();
        virtual status_t    dump(int fd, const Vector<String16>& args) const;
        virtual status_t    setParameters(const CameraParameters& params);
        virtual CameraParameters  getParameters() const;
        virtual status_t    sendCommand(int32_t command, int32_t arg1,
                int32_t arg2);
        virtual void release();

        CAMERA_HAL_ERR_RET setCaptureDevice(sp<CaptureDeviceInterface> capturedevice);
        CAMERA_HAL_ERR_RET setPostProcessDevice(sp<PostProcessDeviceInterface> postprocessdevice);
        CAMERA_HAL_ERR_RET setJpegEncoder(sp<JpegEncoderInterface>jpegencoder);
        CAMERA_HAL_ERR_RET  Init();
        void  setPreviewRotate(CAMERA_PREVIEW_ROTATE previewRotate);

        CameraHal();
        virtual             ~CameraHal();

    private:

        class CaptureFrameThread : public Thread {
            CameraHal* mHardware;
        public:
            CaptureFrameThread(CameraHal* hw)
                : Thread(false), mHardware(hw) { }
            virtual void onFirstRef() {
                run("CaptureFrameThread", PRIORITY_URGENT_DISPLAY);
            }
            virtual bool threadLoop() {
                mHardware->captureframeThread();
                return true;
            }
        };

        class PostProcessThread : public Thread {
            CameraHal* mHardware;
        public:
            PostProcessThread(CameraHal* hw)
                : Thread(false), mHardware(hw) { }
            virtual void onFirstRef() {
                run("PostProcessThread", PRIORITY_URGENT_DISPLAY);
            }
            virtual bool threadLoop() {
                mHardware->postprocessThread();
                return true;
            }
        };


        class PreviewShowFrameThread : public Thread {
            CameraHal* mHardware;
        public:
            PreviewShowFrameThread(CameraHal* hw)
                : Thread(false), mHardware(hw) { }
            virtual void onFirstRef() {
                run("CameraPreviewShowFrameThread", PRIORITY_URGENT_DISPLAY);
            }
            virtual bool threadLoop() {
                mHardware->previewshowFrameThread();
                return true;
            }
        };

        class EncodeFrameThread : public Thread {
            CameraHal* mHardware;
        public:
            EncodeFrameThread(CameraHal* hw)
                : Thread(false), mHardware(hw) { }
            virtual void onFirstRef() {
                run("EncodeFrameThread", PRIORITY_URGENT_DISPLAY);
            }
            virtual bool threadLoop() {
                mHardware->encodeframeThread();
                return true;
            }
        };

        class AutoFocusThread : public Thread {
            CameraHal* mHardware;
        public:
            AutoFocusThread(CameraHal* hw)
                : Thread(false), mHardware(hw) { }
            virtual void onFirstRef() {
                run("AutoFocusThread", PRIORITY_URGENT_DISPLAY);
            }
            virtual bool threadLoop() {
                if (mHardware->autoFocusThread()>=0)
                    return true;
                else
                    return false;
            }
        };


        class TakePicThread : public Thread {
            CameraHal* mHardware;
        public:
            TakePicThread(CameraHal* hw)
                : Thread(false), mHardware(hw) { }
            virtual void onFirstRef() {
                run("TakePicThread", PRIORITY_URGENT_DISPLAY);
            }
            virtual bool threadLoop() {
                if (mHardware->takepicThread()>=0)
                    return true;
                else
                    return false;
            }
        };

        void preInit();
        void postDestroy();

        status_t OpenCaptureDevice();
        void CloseCaptureDevice();

        CAMERA_HAL_ERR_RET AolLocForInterBuf();
        void  FreeInterBuf();
        CAMERA_HAL_ERR_RET InitCameraHalParam();
        CAMERA_HAL_ERR_RET GetCameraBaseParam(CameraParameters *pParam);
        CAMERA_HAL_ERR_RET GetPictureExifParam(CameraParameters *pParam);
        CAMERA_HAL_ERR_RET CameraMiscInit();
        CAMERA_HAL_ERR_RET CameraMiscDeInit();
        status_t CameraHALPreviewStart();
        int captureframeThread();
        int postprocessThread();
        int previewshowFrameThread();
        int encodeframeThread();
        status_t AllocateRecordVideoBuf();

        status_t CameraHALStartPreview();
        void     CameraHALStopPreview();

        status_t PreparePreviwBuf();
        status_t PrepareCaptureDevices();
        status_t PreparePostProssDevice();
        status_t PreparePreviwMisc();

        void CameraHALStopThreads();
        void LockWakeLock();

        void UnLockWakeLock();

        int autoFocusThread();
        int takepicThread();

        int GetJpegEncoderParam();
        int NegotiateCaptureFmt(bool TakePicFlag);
        int cameraHALTakePicture();
        void CameraHALStopMisc();
        int PrepareJpegEncoder();
        void convertNV12toYUV420SP(uint8_t *inputBuffer, uint8_t *outputBuffer, int width, int height);

        int stringTodegree(char* cAttribute, unsigned int &degree, unsigned int &minute, unsigned int &second);

        CameraParameters    mParameters;
        void               *mCallbackCookie;
        notify_callback    mNotifyCb;
        data_callback      mDataCb;
        data_callback_timestamp mDataCbTimestamp;

        sp<CaptureDeviceInterface> mCaptureDevice;
        sp<PostProcessDeviceInterface> mPPDevice;
        sp<JpegEncoderInterface> mJpegEncoder;


        sp<CaptureFrameThread> mCaptureFrameThread;
        sp<PostProcessThread>  mPostProcessThread;
        sp<PreviewShowFrameThread> mPreviewShowFrameThread;
        sp<EncodeFrameThread> mEncodeFrameThread;
        sp<AutoFocusThread>mAutoFocusThread;
        sp<TakePicThread> mTakePicThread;

        mutable Mutex       mLock;

        char *supportedPictureSizes;
        char *supportedPreviewSizes;
        char *supportedFPS;
        char *supprotedThumbnailSizes;

        sp<Overlay>         mOverlay;
        unsigned int        mMsgEnabled;

        struct capture_config_t mCaptureDeviceCfg;
        DMA_BUFFER          mCaptureBuffers[PREVIEW_CAPTURE_BUFFER_NUM];

        sp<MemoryHeapBase>  mPreviewHeap;
        sp<MemoryBase>      mPreviewBuffers[PREVIEW_HEAP_BUF_NUM]; 

        /* the buffer for recorder */
        unsigned int        mVideoBufNume;
        sp<MemoryHeapBase>  mVideoHeap;
        sp<MemoryBase>      mVideoBuffers[VIDEO_OUTPUT_BUFFER_NUM];
        volatile  int       mVideoBufferUsing[VIDEO_OUTPUT_BUFFER_NUM];


        sp<PmemAllocator>   mPmemAllocator;
        DMA_BUFFER          mPPbuf[POST_PROCESS_BUFFER_NUM];
        unsigned int        mPPbufNum;
        pp_input_param_t    mPPInputParam;
        pp_output_param_t   mPPOutputParam;

        volatile bool       mPreviewRunning;
        unsigned int        mPreviewFormat;
        unsigned int 		mPreviewFrameSize;
        unsigned int        mPreviewCapturedFormat;

        bool                mTakePicFlag;
        unsigned int        mEncoderSupportedFormat[MAX_QUERY_FMT_TIMES];
        enc_cfg_param       mJpegEncCfg;


        unsigned int        mUvcSpecialCaptureFormat;
        unsigned int        mCaptureSupportedFormat[MAX_QUERY_FMT_TIMES];
        unsigned int        mPictureEncodeFormat;
        unsigned int        mCaptureFrameSize;
        unsigned int        mCaptureBufNum;
        bool                mRecordRunning;
        int                 mCurrentRecordFrame;
        int 		        nCameraBuffersQueued;

        unsigned int        mPreviewHeapBufNum;
        unsigned int        mTakePicBufQueNum;

        char                mCameraSensorName[MAX_SENSOR_NAME];
        bool mCameraReady;
        bool mCaptureDeviceOpen;
        bool mPPDeviceNeed;
        bool mPPDeviceNeedForPic;
        bool mPreviewStopped;
        bool mRecordStopped;
        bool mPowerLock;

        int error_status;
        unsigned int preview_heap_buf_head;
        unsigned int display_head;
        unsigned int enc_head;
        unsigned int dequeue_head;
        unsigned int is_first_buffer;
        unsigned int last_display_index;
        unsigned int pp_in_head;
        unsigned int pp_out_head;
        unsigned int buffer_index_maps[PREVIEW_CAPTURE_BUFFER_NUM];

        sem_t avab_show_frame;
        sem_t avab_dequeue_frame;
        sem_t avab_enc_frame;
        sem_t avab_enc_frame_finish;
        sem_t avab_pp_in_frame;
        sem_t avab_pp_out_frame;

        pthread_mutex_t mOverlayMutex;
        pthread_mutex_t mMsgMutex;
        pthread_mutex_t mPPIOParamMutex;
        CAMERA_PREVIEW_ROTATE mPreviewRotate;

    };

}; // namespace android

#endif

