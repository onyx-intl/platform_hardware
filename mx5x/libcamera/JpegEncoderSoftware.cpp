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

#include "JpegEncoderSoftware.h"

namespace android{

    JPEG_ENC_UINT32 JpegEncoderSoftware::g_JpegDataSize = 0;//Total size of g_JpegData
    JPEG_ENC_UINT32 JpegEncoderSoftware::g_JpegDataLen = 0;//Valid data len of g_JpegData
    JPEG_ENC_UINT8 *JpegEncoderSoftware::g_JpegData = NULL;//Buffer to hold jpeg data

    JpegEncoderSoftware :: JpegEncoderSoftware()
        :mSupportedTypeIdx(0),
        pEncCfgLocal(NULL),
        pEncObj(NULL)
    {
        mSupportedType[0] = v4l2_fourcc('Y','U','1','2');
    }

    JpegEncoderSoftware :: ~JpegEncoderSoftware()
    {

    }

    JPEG_ENC_ERR_RET  JpegEncoderSoftware :: EnumJpegEncParam(JPEEG_QUERY_TYPE QueryType, void * pQueryRet)
    {

        int * pSupportedType = (int *)pQueryRet;
        switch(QueryType){	
            case SUPPORTED_FMT:
                if (mSupportedTypeIdx < MAX_ENC_SUPPORTED_YUV_TYPE){
                    *pSupportedType = mSupportedType[mSupportedTypeIdx];
                    mSupportedTypeIdx ++;
                }else{
                    mSupportedTypeIdx = 0;
                    return JPEG_ENC_ERROR_BAD_PARAM;
                }
                break;
            default:
                return JPEG_ENC_ERROR_BAD_PARAM;
        }

        return JPEG_ENC_ERROR_NONE;
    }

    JPEG_ENC_ERR_RET JpegEncoderSoftware :: JpegEncoderInit(enc_cfg_param *pEncCfg)
    {
        CAMERA_HAL_LOG_FUNC;

        JPEG_ENC_ERR_RET ret = JPEG_ENC_ERROR_NONE;
        struct jpeg_enc_focallength_t * pFoclLength = NULL;
        struct jpeg_enc_make_info_t *pMakeInfo = NULL;
        struct jpeg_enc_makernote_info_t *pMakeNote = NULL;
        struct jpeg_enc_model_info_t *pModelInfo = NULL;
        struct jpeg_enc_datetime_info_t *pDatetimeInfo = NULL;
        struct jpeg_enc_gps_param *pGpsInfoLocal = NULL;

        if(pEncCfg == NULL){
            return JPEG_ENC_ERROR_BAD_PARAM;
        }

        pEncCfgLocal = (enc_cfg_param *)malloc(sizeof(enc_cfg_param));

        if (pEncCfgLocal == NULL){
            CAMERA_HAL_ERR("Allocat buffer for EncCfg failed");
            return JPEG_ENC_ERROR_ALOC_BUF;
        }

        memset(pEncCfgLocal, 0, sizeof(enc_cfg_param));
        memcpy(pEncCfgLocal, pEncCfg, sizeof(enc_cfg_param));


        if ((ret = CheckEncParm()) != JPEG_ENC_ERROR_NONE){
            goto INT_ERR_RET;
        }

        if (pEncCfg->pFoclLength != NULL){
            pFoclLength = (struct jpeg_enc_focallength_t *)malloc(sizeof(struct jpeg_enc_focallength_t));

            if (pFoclLength == NULL){
                CAMERA_HAL_ERR("Allocat buffer for pFoclLength failed");
                ret = JPEG_ENC_ERROR_ALOC_BUF;
                goto INT_ERR_RET;
            }
            memset(pFoclLength, 0, sizeof(struct jpeg_enc_focallength_t));
            memcpy(pFoclLength, pEncCfg->pFoclLength, sizeof(struct jpeg_enc_focallength_t));
            pEncCfgLocal->pFoclLength = pFoclLength;
        }


        if (pEncCfg->pMakeInfo != NULL){
            pMakeInfo = (struct jpeg_enc_make_info_t *)malloc(sizeof(struct jpeg_enc_make_info_t));

            if (pMakeInfo == NULL){
                CAMERA_HAL_ERR("Allocat buffer for pMakeInfo failed");
                ret = JPEG_ENC_ERROR_ALOC_BUF;
                goto INT_ERR_RET;
            }
            memset(pMakeInfo, 0, sizeof(struct jpeg_enc_make_info_t));
            memcpy(pMakeInfo, pEncCfg->pMakeInfo, sizeof(struct jpeg_enc_make_info_t));
            pEncCfgLocal->pMakeInfo = pMakeInfo;
        }

        if (pEncCfg->pMakeNote != NULL){
            pMakeNote = (struct jpeg_enc_makernote_info_t *)malloc(sizeof(struct jpeg_enc_makernote_info_t));

            if (pMakeNote == NULL){
                CAMERA_HAL_ERR("Allocat buffer for pMakeNote failed");
                ret = JPEG_ENC_ERROR_ALOC_BUF;
                goto INT_ERR_RET;
            }
            memset(pMakeNote, 0, sizeof(struct jpeg_enc_makernote_info_t));
            memcpy(pMakeNote, pEncCfg->pMakeNote, sizeof(struct jpeg_enc_makernote_info_t));
            pEncCfgLocal->pMakeNote = pMakeNote;
        }

        if (pEncCfg->pModelInfo != NULL){
            pModelInfo = (struct jpeg_enc_model_info_t *)malloc(sizeof(struct jpeg_enc_model_info_t));

            if (pModelInfo == NULL){
                CAMERA_HAL_ERR("Allocat buffer for pModelInfo failed");
                ret = JPEG_ENC_ERROR_ALOC_BUF;
                goto INT_ERR_RET;
            }
            memset(pModelInfo, 0, sizeof(struct jpeg_enc_model_info_t));
            memcpy(pModelInfo, pEncCfg->pModelInfo, sizeof(struct jpeg_enc_model_info_t));
            pEncCfgLocal->pModelInfo = pModelInfo;
        }

        if (pEncCfg->pDatetimeInfo != NULL){
            pDatetimeInfo = (struct jpeg_enc_datetime_info_t *)malloc(sizeof(struct jpeg_enc_datetime_info_t));

            if (pDatetimeInfo == NULL){
                CAMERA_HAL_ERR("Allocat buffer for pDatetimeInfo failed");
                ret = JPEG_ENC_ERROR_ALOC_BUF;
                goto INT_ERR_RET;
            }
            memset(pDatetimeInfo, 0, sizeof(struct jpeg_enc_datetime_info_t));
            memcpy(pDatetimeInfo, pEncCfg->pDatetimeInfo, sizeof(struct jpeg_enc_datetime_info_t));
            pEncCfgLocal->pDatetimeInfo = pDatetimeInfo;
        }

        if (pEncCfg->pGps_info != NULL){
            pGpsInfoLocal = (struct jpeg_enc_gps_param *)malloc(sizeof(struct jpeg_enc_gps_param));

            if (pGpsInfoLocal == NULL){
                CAMERA_HAL_ERR("Allocat buffer for pGpsInfoLocal failed");
                ret = JPEG_ENC_ERROR_ALOC_BUF;
                goto INT_ERR_RET;
            }
            memset(pGpsInfoLocal, 0, sizeof(struct jpeg_enc_gps_param));
            memcpy(pGpsInfoLocal, pEncCfg->pGps_info, sizeof(struct jpeg_enc_gps_param));
            pEncCfgLocal->pGps_info = pGpsInfoLocal;
        }

        return ret;

INT_ERR_RET:
        if(pEncCfgLocal)
            free(pEncCfgLocal);
        if(pFoclLength)
            free(pFoclLength);
        if(pMakeInfo)
            free(pMakeInfo);
        if(pMakeNote)
            free(pMakeNote);
        if(pModelInfo)
            free(pModelInfo);
        if(pDatetimeInfo)
            free(pDatetimeInfo);
        if(pGpsInfoLocal)
            free(pGpsInfoLocal);
        return ret;

    }

    JPEG_ENC_ERR_RET JpegEncoderSoftware :: DoEncode( DMA_BUFFER *inBuf, DMA_BUFFER *outBuf, struct jpeg_encoding_conf *pJpegEncCfg){
        if (inBuf == NULL || outBuf == NULL || inBuf->virt_start == NULL || outBuf->virt_start == NULL){
            return JPEG_ENC_ERROR_BAD_PARAM;
        }else{
            return encodeImge(inBuf,outBuf, &(pJpegEncCfg->output_jpeg_size));
        }
    }

    JPEG_ENC_ERR_RET JpegEncoderSoftware :: JpegEncoderDeInit(){
        CAMERA_HAL_LOG_FUNC;
        JPEG_ENC_ERR_RET ret = JPEG_ENC_ERROR_NONE;

        if (pEncCfgLocal != NULL ){
            if (pEncCfgLocal->pFoclLength != NULL)
                free(pEncCfgLocal->pFoclLength);
            if (pEncCfgLocal->pMakeInfo != NULL)
                free(pEncCfgLocal->pMakeInfo);
            if (pEncCfgLocal->pMakeNote != NULL)
                free(pEncCfgLocal->pMakeNote);
            if (pEncCfgLocal->pModelInfo != NULL)
                free(pEncCfgLocal->pModelInfo);
            if (pEncCfgLocal->pDatetimeInfo != NULL)
                free(pEncCfgLocal->pDatetimeInfo);
            if (pEncCfgLocal->pGps_info != NULL)
                free(pEncCfgLocal->pGps_info);
            free(pEncCfgLocal);
        }

        return ret;

    }

    JPEG_ENC_ERR_RET JpegEncoderSoftware :: CheckEncParm(){

        CAMERA_HAL_LOG_FUNC;
        int i = 0;

        JPEG_ENC_ERR_RET ret = JPEG_ENC_ERROR_NONE;

        if ((pEncCfgLocal->PicWidth <= 0) && (pEncCfgLocal->PicHeight<= 0)){
            CAMERA_HAL_ERR("The input widht and height is wrong");
            return JPEG_ENC_ERROR_BAD_PARAM;
        }

        if((pEncCfgLocal->PicWidth <= 0) || (pEncCfgLocal->PicHeight <= 0)|| 
                (pEncCfgLocal->ThumbWidth > pEncCfgLocal->PicWidth) ||
                (pEncCfgLocal->ThumbHeight > pEncCfgLocal->PicHeight) ){
            CAMERA_HAL_ERR("The input widht and height is wrong");
            return JPEG_ENC_ERROR_BAD_PARAM;
        }

        for (i = 0; i< MAX_ENC_SUPPORTED_YUV_TYPE; i++){
            if(pEncCfgLocal->BufFmt == mSupportedType[i])
                break;
        }

        if (i == MAX_ENC_SUPPORTED_YUV_TYPE)
            ret = JPEG_ENC_ERROR_BAD_PARAM;

        return ret;
    }

    JPEG_ENC_ERR_RET JpegEncoderSoftware :: encodeImge(DMA_BUFFER *inBuf, DMA_BUFFER *outBuf, unsigned int *pEncSize){

        CAMERA_HAL_LOG_FUNC;

        JPEG_ENC_ERR_RET ret = JPEG_ENC_ERROR_NONE;
        int width, height, size,index;
        JPEG_ENC_UINT8 * i_buff = NULL;
        JPEG_ENC_UINT8 * y_buff = NULL;
        JPEG_ENC_UINT8 * u_buff = NULL;
        JPEG_ENC_UINT8 * v_buff = NULL;
        JPEG_ENC_RET_TYPE return_val;
        jpeg_enc_parameters * params = NULL;
        jpeg_enc_object * obj_ptr = NULL;
        JPEG_ENC_UINT8 number_mem_info;
        jpeg_enc_memory_info * mem_info = NULL;
        unsigned char *thumbnail_buffer,*temp_buffer=NULL;
        int thumbnail_width, thumbnail_height;
        unsigned char *buffer = inBuf->virt_start;

        bool mEncodeThumbnailFlag = true;

        width = pEncCfgLocal->PicWidth;
        height = pEncCfgLocal->PicHeight;

        thumbnail_width = pEncCfgLocal->ThumbWidth;
        thumbnail_height = pEncCfgLocal->ThumbHeight;


        if (thumbnail_width <= 0 || thumbnail_height<= 0)
            mEncodeThumbnailFlag = false;

        g_JpegDataSize = 0;//Total size of g_JpegData
        g_JpegDataLen = 0;//Valid data len of g_JpegData
        g_JpegData = NULL;//Buffer to hold jpeg data
        size = width * height * 3 / 2;

        g_JpegData = outBuf->virt_start;
        g_JpegDataSize = size;
        if(!g_JpegData)
        {
            return JPEG_ENC_ERROR_BAD_PARAM;
        }

        /* --------------------------------------------
         * Allocate memory for Encoder Object
         * -------------------------------------------*/
        obj_ptr = (jpeg_enc_object *) malloc(sizeof(jpeg_enc_object));
        if(!obj_ptr)
        {
            return JPEG_ENC_ERROR_ALOC_BUF;
        }
        memset(obj_ptr, 0, sizeof(jpeg_enc_object));

        /* Assign the function for streaming output */
        obj_ptr->jpeg_enc_push_output = pushJpegOutput;
        obj_ptr->context=NULL;   //user can put private variables into it
        /* --------------------------------------------
         * Fill up the parameter structure of JPEG Encoder
         * -------------------------------------------*/
        params = &(obj_ptr->parameters);

        if(mEncodeThumbnailFlag==true)
        {

            //need resizing code here!!!
            thumbnail_buffer = (unsigned char *)malloc(thumbnail_width * thumbnail_height * 3 / 2);
            if(!thumbnail_buffer)
            {
                return JPEG_ENC_ERROR_ALOC_BUF;
            }

            yuv_resize((unsigned char *)thumbnail_buffer, thumbnail_width, thumbnail_height, buffer, width, height);

            width = thumbnail_width;
            height = thumbnail_height;

            temp_buffer = buffer;
            buffer = thumbnail_buffer;

            params->mode = JPEG_ENC_THUMB;
        }
        else
        {
            params->mode = JPEG_ENC_MAIN_ONLY;
        }

encodeframe:

        params->compression_method = JPEG_ENC_SEQUENTIAL;
        params->quality = 75;
        params->restart_markers = 0;
        if (pEncCfgLocal->BufFmt == v4l2_fourcc('Y','U','1','2')){
            params->y_width = width;
            params->y_height = height;
            params->u_width = params->y_width/2;
            params->u_height = params->y_height/2;
            params->v_width = params->y_width/2;
            params->v_height = params->y_height/2;
            params->primary_image_height = height;
            params->primary_image_width = width;
            params->yuv_format = JPEG_ENC_YUV_420_NONINTERLEAVED;
        }else if (pEncCfgLocal->BufFmt == v4l2_fourcc('Y','U','Y','V')){
            params->y_width = width;
            params->y_height = height;
            params->u_width = params->y_width/2;
            params->u_height = params->y_height;
            params->v_width = params->y_width/2;
            params->v_height = params->y_height;
            params->primary_image_height = height;
            params->primary_image_width = width;
            params->yuv_format = JPEG_ENC_YU_YV_422_INTERLEAVED;
        }
        params->exif_flag = 1;

        params->y_left = 0;
        params->y_top = 0;
        params->y_total_width = 0;
        params->y_total_height = 0;
        params->raw_dat_flag= 0;

        if(params->y_total_width==0)
        {
            params->y_left=0;
            params->u_left=0;
            params->v_left=0;
            params->y_total_width=params->y_width;  // no cropping
            params->u_total_width=params->u_width;  // no cropping
            params->v_total_width=params->v_width;  // no cropping
        }

        if(params->y_total_height==0)
        {
            params->y_top=0;
            params->u_top=0;
            params->v_top=0;
            params->y_total_height=params->y_height; // no cropping
            params->u_total_height=params->u_height; // no cropping
            params->v_total_height=params->v_height; // no cropping
        }

        /* Pixel size is unknown by default */
        params->jfif_params.density_unit = 0;
        /* Pixel aspect ratio is square by default */
        params->jfif_params.X_density = 1;
        params->jfif_params.Y_density = 1;
        if (params->yuv_format == JPEG_ENC_YUV_420_NONINTERLEAVED){
            y_buff = (JPEG_ENC_UINT8 *)buffer;
            u_buff = y_buff+width*height;
            v_buff = u_buff+width*height/4;
            i_buff = NULL;
        }else if (params->yuv_format == JPEG_ENC_YU_YV_422_INTERLEAVED){
            y_buff = NULL;
            u_buff = NULL;
            v_buff = NULL;
            i_buff = (JPEG_ENC_UINT8 *)buffer;
        }
        CAMERA_HAL_LOG_RUNTIME("version: %s\n", jpege_CodecVersionInfo());

        /* --------------------------------------------
         * QUERY MEMORY REQUIREMENTS
         * -------------------------------------------*/
        return_val = jpeg_enc_query_mem_req(obj_ptr);

        if(return_val != JPEG_ENC_ERR_NO_ERROR)
        {
            CAMERA_HAL_LOG_RUNTIME("JPEG encoder returned an error when jpeg_enc_query_mem_req was called \n");
            CAMERA_HAL_LOG_RUNTIME("Return Val %d\n",return_val);
            goto done;
        }
        CAMERA_HAL_LOG_RUNTIME("jpeg_enc_query_mem_req success");
        /* --------------------------------------------
         * ALLOCATE MEMORY REQUESTED BY CODEC
         * -------------------------------------------*/
        number_mem_info = obj_ptr->mem_infos.no_entries;
        for(index = 0; index < number_mem_info; index++)
        {
            /* This example code ignores the 'alignment' and
             * 'memory_type', but some other applications might want
             * to allocate memory based on them */
            mem_info = &(obj_ptr->mem_infos.mem_info[index]);
            mem_info->memptr = (void *) malloc(mem_info->size);
            if(mem_info->memptr==NULL) {
                CAMERA_HAL_LOG_RUNTIME("Malloc error after query\n");
                goto done;
            }
        }

        return_val = jpeg_enc_init(obj_ptr);
        if(return_val != JPEG_ENC_ERR_NO_ERROR)
        {
            CAMERA_HAL_LOG_RUNTIME("JPEG encoder returned an error when jpeg_enc_init was called \n");
            CAMERA_HAL_LOG_RUNTIME("Return Val %d\n",return_val);
            goto done;
        }

        CAMERA_HAL_LOG_RUNTIME("jpeg_enc_init success");
        if(params->mode == JPEG_ENC_THUMB)
            createJpegExifTags(obj_ptr);

        return_val = jpeg_enc_encodeframe(obj_ptr, i_buff,
                y_buff, u_buff, v_buff);

        if(return_val != JPEG_ENC_ERR_ENCODINGCOMPLETE)
        {
            CAMERA_HAL_LOG_RUNTIME("JPEG encoder returned an error in jpeg_enc_encodeframe \n");
            CAMERA_HAL_LOG_RUNTIME("Return Val %d\n",return_val);
            goto done;
        }

        if(params->mode == JPEG_ENC_THUMB)
        {
            JPEG_ENC_UINT8 num_entries;
            JPEG_ENC_UINT32 *offset_tbl_ptr = (JPEG_ENC_UINT32 *)malloc(sizeof(JPEG_ENC_UINT32)*JPEG_ENC_NUM_OF_OFFSETS);
            JPEG_ENC_UINT8 *value_tbl_ptr = (JPEG_ENC_UINT8 *)malloc(sizeof(JPEG_ENC_UINT8)*JPEG_ENC_NUM_OF_OFFSETS);

            jpeg_enc_find_length_position(obj_ptr, offset_tbl_ptr,value_tbl_ptr,&num_entries);

            for(int i = 0; i < num_entries; i++)
            {
                *((JPEG_ENC_UINT8 *)g_JpegData+offset_tbl_ptr[i]) = value_tbl_ptr[i];
            }

            free(offset_tbl_ptr);
            free(value_tbl_ptr);

            free(buffer);

            number_mem_info = obj_ptr->mem_infos.no_entries;
            for(index = 0; index < number_mem_info; index++)
            {
                mem_info = &(obj_ptr->mem_infos.mem_info[index]);
                if(mem_info)
                    free(mem_info->memptr);
            }

            g_JpegData += g_JpegDataLen;
            g_JpegDataSize -= g_JpegDataLen;


            //recover to build the main jpeg
            params->mode = JPEG_ENC_MAIN;

            buffer = temp_buffer;
            width = pEncCfgLocal->PicWidth;
            height = pEncCfgLocal->PicHeight;

            goto encodeframe;
        }
        CAMERA_HAL_LOG_RUNTIME("jpeg_enc_encodeframe success");
        // Make an IMemory for each frame
        //jpegPtr = new MemoryBase(mJpegImageHeap, 0, g_JpegDataLen);
        *pEncSize = g_JpegDataLen;

done:
        /* --------------------------------------------
         * FREE MEMORY REQUESTED BY CODEC
         * -------------------------------------------*/
        if(obj_ptr)
        {
            number_mem_info = obj_ptr->mem_infos.no_entries;
            for(index = 0; index < number_mem_info; index++)
            {
                mem_info = &(obj_ptr->mem_infos.mem_info[index]);
                if(mem_info)
                    free(mem_info->memptr);
            }
            free(obj_ptr);
        }

        return ret;
    }

    JPEG_ENC_UINT8 JpegEncoderSoftware::pushJpegOutput(JPEG_ENC_UINT8 ** out_buf_ptrptr,JPEG_ENC_UINT32 *out_buf_len_ptr,
            JPEG_ENC_UINT8 flush, void * context, JPEG_ENC_MODE enc_mode)
    {
        JPEG_ENC_UINT32 i;
        if(*out_buf_ptrptr == NULL)
        {
            /* This function is called for the 1'st time from the
             * codec */
            *out_buf_ptrptr = g_JpegData;
            *out_buf_len_ptr = g_JpegDataSize;
        }

        else if(flush == 1)
        {
            /* Flush the buffer*/
            g_JpegDataLen += *out_buf_len_ptr;
            CAMERA_HAL_LOG_RUNTIME("jpeg output data len %d",(int)g_JpegDataLen);

            *out_buf_ptrptr = NULL;
            *out_buf_len_ptr = NULL;
        }
        else
        {
            CAMERA_HAL_LOG_RUNTIME("Not enough buffer for encoding");
            return 0;
        }

        return(1); /* Success */
    }

    void JpegEncoderSoftware::createJpegExifTags(jpeg_enc_object * obj_ptr)
    {
        CAMERA_HAL_LOG_RUNTIME("version: %s\n", jpege_CodecVersionInfo());

        jpeg_enc_set_exifheaderinfo(obj_ptr, JPEGE_ENC_SET_HEADER_ORIENTATION, (unsigned int)(&(pEncCfgLocal->RotationInfo)));
        jpeg_enc_set_exifheaderinfo(obj_ptr, JPEGE_ENC_SET_HEADER_WHITEBALANCE, (unsigned int)(&(pEncCfgLocal->WhiteBalanceInfo)));
        jpeg_enc_set_exifheaderinfo(obj_ptr, JPEGE_ENC_SET_HEADER_FLASH, (unsigned int)(&(pEncCfgLocal->FlashInfo)));

        if(pEncCfgLocal->pMakeInfo)
            jpeg_enc_set_exifheaderinfo(obj_ptr, JPEGE_ENC_SET_HEADER_MAKE, (unsigned int)(pEncCfgLocal->pMakeInfo));
        if(pEncCfgLocal->pMakeNote)
            jpeg_enc_set_exifheaderinfo(obj_ptr, JPEGE_ENC_SET_HEADER_MAKERNOTE, (unsigned int)(pEncCfgLocal->pMakeNote));
        if(pEncCfgLocal->pModelInfo)
            jpeg_enc_set_exifheaderinfo(obj_ptr, JPEGE_ENC_SET_HEADER_MODEL, (unsigned int)(pEncCfgLocal->pModelInfo));
        if(pEncCfgLocal->pDatetimeInfo)
            jpeg_enc_set_exifheaderinfo(obj_ptr, JPEGE_ENC_SET_HEADER_DATETIME, (unsigned int)(pEncCfgLocal->pDatetimeInfo));
        if(pEncCfgLocal->pFoclLength)
            jpeg_enc_set_exifheaderinfo(obj_ptr, JPEGE_ENC_SET_HEADER_FOCALLENGTH, (unsigned int)(pEncCfgLocal->pFoclLength));

        if (pEncCfgLocal->pGps_info)
            jpeg_enc_set_exifheaderinfo(obj_ptr, JPEGE_ENC_SET_HEADER_GPS, (unsigned int)(pEncCfgLocal->pGps_info));

        return;
    }

    int JpegEncoderSoftware::yuv_resize(unsigned char *dst_ptr, int dst_width, int dst_height, unsigned char *src_ptr, int src_width, int src_height)
    {
        int i,j,s;
        int h_offset;
        int v_offset;
        unsigned char *ptr,cc;
        int h_scale_ratio;
        int v_scale_ratio;

        s = 0;

_resize_begin:

        if(!dst_width) return -1;
        if(!dst_height) return -1;

        h_scale_ratio = src_width / dst_width;
        if(!h_scale_ratio) return -1;

        v_scale_ratio = src_height / dst_height;
        if(!v_scale_ratio) return -1;

        h_offset = (src_width - dst_width * h_scale_ratio) / 2;
        v_offset = (src_height - dst_height * v_scale_ratio) / 2;

        for(i = 0; i < dst_height * v_scale_ratio; i += v_scale_ratio)
        {
            for(j = 0; j < dst_width * h_scale_ratio; j += h_scale_ratio)
            {
                ptr = src_ptr + i * src_width + j + v_offset * src_width + h_offset;
                cc = ptr[0];

                ptr = dst_ptr + (i / v_scale_ratio) * dst_width + (j / h_scale_ratio);
                ptr[0] = cc;
            }
        }

        src_ptr += src_width*src_height;
        dst_ptr += dst_width*dst_height;

        if(s < 2)
        {
            if(!s++)
            {
                src_width >>= 1;
                src_height >>= 1;

                dst_width >>= 1;
                dst_height >>= 1;
            }

            goto _resize_begin;
        }

        return 0;
    }

    sp<JpegEncoderInterface> JpegEncoderSoftware::createInstance(){
        sp<JpegEncoderInterface> hardware(new JpegEncoderSoftware());
        return hardware;
    }


};
