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

#ifndef JPEG_ENCODER_INTERFACE_H
#define JPEG_ENCODER_INTERFACE_H

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
#include "Camera_utils.h"
#include <utils/RefBase.h>

namespace android{


#define MAX_JPEG_MAKE_BYTES 256
#define MAX_JPEG_MAKERNOTE_BYTES 256
#define MAX_JPEG_MODEL_BYTES 256
#define TIME_FMT_LENGTH 20
#define MAX_GPS_PROCESSING_BYTES	256

    typedef enum{
    ORIENTATION_UNDEFINED = 0,
    ORIENTATION_NORMAL = 1,
    ORIENTATION_FLIP_HORIZONTAL = 2,
    ORIENTATION_ROTATE_180 = 3,
    ORIENTATION_FLIP_VERTICAL = 4,
    ORIENTATION_TRANSPOSE = 5,
    ORIENTATION_ROTATE_90 = 6,
    ORIENTATION_TRANSVERSE = 7,
    ORIENTATION_ROTATE_270 = 8
    }JPEG_ENCODER_ROTATION;

    typedef enum{
    WHITEBALANCE_AUTO = 0,
    WHITEBALANCE_MANUAL = 1
    }JPEG_ENCODER_WHITEBALANCE;

    typedef enum{
    FLASH_NOT_FIRE = 0x00,
    FLASH_FIRED    = 0x01,
    FLASH_FIRED_AUTO = 0x19,
    FLASH_FIRED_RED_EYE_REDUCE = 0x41,
    FLASH_FIRED_COMPULOSORY = 0x09
    }JPEG_ENCODER_FLASH;

    typedef enum{
        SOFTWARE_JPEG_ENC = 0,
        HARDWARE_JPEG_ENC = 1
    }JPEG_ENCODER_TYPE;

    typedef enum{
        JPEG_ENC_ERROR_NONE = 0,
        JPEG_ENC_ERROR_BAD_PARAM = -1,
        JPEG_ENC_ERROR_ALOC_BUF = -2
    }JPEG_ENC_ERR_RET;


    typedef enum{
        SUPPORTED_FMT = 0,
    }JPEEG_QUERY_TYPE;

    struct jpeg_enc_focallength_t
    {
        unsigned int numerator;
        unsigned int denominator;
    };

    struct jpeg_enc_make_info_t
    {
        unsigned char make_bytes;
        unsigned char make[MAX_JPEG_MAKE_BYTES];
    };
    struct jpeg_enc_makernote_info_t
    {
        unsigned char makernote_bytes;
        unsigned char makernote[MAX_JPEG_MAKERNOTE_BYTES];
    };

    struct jpeg_enc_model_info_t
    {
        unsigned char model_bytes;
        unsigned char model[MAX_JPEG_MODEL_BYTES];	
    };

    struct jpeg_enc_datetime_info_t
    {
        unsigned char datetime[TIME_FMT_LENGTH];	// "YYYY:MM:DD HH:MM:SS" with time shown in 24-hour format
    };

    struct jpeg_enc_gps_param{
        unsigned int version;//GPSVersionID
        char latitude_ref[2];//GPSLatitudeRef: "N " is positive; "S " is negative
        char longtitude_ref[2];	//GPSLongtitudeRef: "E " is positive; "W " is negative
        unsigned int latitude_degree[2];//GPSLatitude
        unsigned int latitude_minute[2];
        unsigned int latitude_second[2];
        unsigned int longtitude_degree[2];//GPSLongitude
        unsigned int longtitude_minute[2];
        unsigned int longtitude_second[2];
        char altitude_ref;//GPSAltitudeRef: 0 or 1(negative)
        unsigned int altitude[2];//GPSAltitude: unit is meters
        unsigned int hour[2];//GPSTimeStamp
        unsigned int minute[2];
        unsigned int seconds[2];
        char processmethod[MAX_GPS_PROCESSING_BYTES]; //GPSProcessingMethod
        char processmethod_bytes;
        char datestamp[11];//GPSDateStamp: "YYYY:MM:DD "
    };

    typedef struct{
        unsigned int PicWidth;
        unsigned int PicHeight;
        unsigned int ThumbWidth;
        unsigned int ThumbHeight;
        unsigned int BufFmt;
        JPEG_ENCODER_ROTATION RotationInfo;
        JPEG_ENCODER_WHITEBALANCE WhiteBalanceInfo;
        JPEG_ENCODER_FLASH FlashInfo;
        struct jpeg_enc_focallength_t *pFoclLength;
        struct jpeg_enc_make_info_t *pMakeInfo;
        struct jpeg_enc_makernote_info_t *pMakeNote;
        struct jpeg_enc_model_info_t *pModelInfo;
        struct jpeg_enc_datetime_info_t *pDatetimeInfo;
        struct jpeg_enc_gps_param *pGps_info;
    }enc_cfg_param;

    struct jpeg_encoding_conf{
        unsigned int output_jpeg_size;
    };

    class JpegEncoderInterface : public virtual RefBase{
    public:
        virtual  JPEG_ENC_ERR_RET  EnumJpegEncParam(JPEEG_QUERY_TYPE QueryType, void * pQueryRet)=0;
        virtual  JPEG_ENC_ERR_RET JpegEncoderInit(enc_cfg_param *pEncCfg)=0;
        virtual  JPEG_ENC_ERR_RET DoEncode( DMA_BUFFER *inBuf, DMA_BUFFER *outBuf, struct jpeg_encoding_conf *pJpegEncCfg)=0;
        virtual  JPEG_ENC_ERR_RET JpegEncoderDeInit()=0;

        virtual ~ JpegEncoderInterface(){}
    }; 

    extern "C" sp<JpegEncoderInterface> createJpegEncoder(JPEG_ENCODER_TYPE jpeg_enc_type);

};

#endif
