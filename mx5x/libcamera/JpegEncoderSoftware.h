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

#ifndef JPEG_ENCODER_SOFTWARE_H
#define JPEG_ENCODER_SOFTWARE_H

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

#include "JpegEncoderInterface.h"
#include "jpeg_enc_interface.h"


namespace android{
#define MAX_ENC_SUPPORTED_YUV_TYPE  1

    class JpegEncoderSoftware : public JpegEncoderInterface{
    public:
        virtual  JPEG_ENC_ERR_RET  EnumJpegEncParam(JPEEG_QUERY_TYPE QueryType, void * pQueryRet);
        virtual  JPEG_ENC_ERR_RET JpegEncoderInit(enc_cfg_param *pEncCfg);
        virtual  JPEG_ENC_ERR_RET DoEncode( DMA_BUFFER *inBuf, DMA_BUFFER *outBuf, struct jpeg_encoding_conf *pJpegEncCfg);
        virtual  JPEG_ENC_ERR_RET JpegEncoderDeInit();

        static sp<JpegEncoderInterface>createInstance();
    private:

        JpegEncoderSoftware();
        virtual ~JpegEncoderSoftware();

        virtual JPEG_ENC_ERR_RET CheckEncParm();
        virtual JPEG_ENC_ERR_RET encodeImge(DMA_BUFFER *inBuf, DMA_BUFFER *outBuf, unsigned int *pEncSize);


        static JPEG_ENC_UINT8 pushJpegOutput(JPEG_ENC_UINT8 ** out_buf_ptrptr,
                JPEG_ENC_UINT32 *out_buf_len_ptr,
                JPEG_ENC_UINT8 flush, 
                void * context, 
                JPEG_ENC_MODE enc_mode);
        void createJpegExifTags(jpeg_enc_object * obj_ptr);
        int yuv_resize(unsigned char *dst_ptr, int dst_width, int dst_height, unsigned char *src_ptr, int src_width, int src_height);


        unsigned int mSupportedType[MAX_ENC_SUPPORTED_YUV_TYPE];
        unsigned int mSupportedTypeIdx;
        enc_cfg_param *pEncCfgLocal;
        jpeg_enc_object *pEncObj;


        static JPEG_ENC_UINT32 g_JpegDataSize ;//Total size of g_JpegData
        static JPEG_ENC_UINT32 g_JpegDataLen ;//Valid data len of g_JpegData
        static JPEG_ENC_UINT8 *g_JpegData ;//Buffer to hold jpeg data

    }; 
};

#endif
