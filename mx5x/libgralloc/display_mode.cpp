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

/* Copyright 2010-2012 Freescale Semiconductor, Inc. */

#include <fcntl.h>
#include <errno.h>
#include <cutils/properties.h>
#include <stdlib.h>
#include "display_mode.h"
#include "cutils/log.h"

static int str2int(char *p, int *len)
{
        int val = 0;
        int length =0;
        if(!p) return -1;

        while(p[0] >= '0' && p[0] <= '9')
        {
                val = val * 10 + p[0] - '0';
                p++;
                length ++;
        }
    *len = length;
        return val;
}

static int disp_mode_compare( const void *arg1, const void *arg2)
{
        disp_mode *dm1 = (disp_mode *)arg1;
        disp_mode *dm2 = (disp_mode *)arg2;

    if(dm1->width  > dm2->width)  return -1;
    else if (dm1->width  < dm2->width) return 1;
    else {
        if(dm1->height > dm2->height) return -1;
        else if (dm1->height < dm2->height )  return 1;
        else {
            if (dm1->freq > dm2->freq ) return -1;
            else if (dm1->freq < dm2->freq ) return 1;
            else return 0;
        }
    }

        return 0;
}

static int get_available_mode(int fbid, const char *mode_list)
{
        int disp_threshold = 0;
        int i,disp_mode_count = 0;
        read_state state = CHECK_NEXT_STATE;
        char *p = (char *)mode_list;
        char *start = p;
        char *end   = p;
    int len = 0;
    if(!p) return 0;


        while(p[0])
        {
                switch(state)
                {
                case CHECK_NEXT_STATE:
                        if(!strncmp(p, "D:", 2)
                                || !strncmp(p, "S:", 2)
                                || !strncmp(p, "U:", 2)
                                || !strncmp(p, "V:", 2))
                        {
                                start = p;
                                state = FIND_WIDTH_STATE;
                                p+=2;
                        }
                        else p++;
                        break;
                case FIND_WIDTH_STATE:
                        if(p[0]>='0' && p[0]<='9')
                        {
                            len = 0;
                                disp_class_list[fbid].disp_mode_list[disp_mode_count].width = str2int(p, &len);
                                state = FIND_JOINT_STATE;
                                p =  p +len;
                        }
                        else p++;
                        break;
                case FIND_JOINT_STATE:
                        if(p[0] == 'x' || p[0] == 'X')
                        {
                            p++;
                                state = FIND_HEIGHT_STATE;
                        }
                        else p++;
                        break;
                case FIND_HEIGHT_STATE:
                        if(p[0]>='0' && p[0]<='9')
                        {
                            len = 0;
                                disp_class_list[fbid].disp_mode_list[disp_mode_count].height = str2int(p,&len);
                                state = PREFIX_FREQ_STATE;
                                p =  p +len;
                        }
                        else p++;
                        break;
                case PREFIX_FREQ_STATE:
                        if(!strncmp(p, "p-", 2) || !strncmp(p, "i-", 2))
                        {
                                state = FREQUENCY_STATE;
                                p+=2;
                        }
                        else p++;
                        break;
                case  FREQUENCY_STATE:
                        if(p[0]>='0' && p[0]<='9')
                        {
                            len = 0;
                                disp_class_list[fbid].disp_mode_list[disp_mode_count].freq = str2int(p,&len);
                                state = FIND_NEWLINE_STATE;
                                p =  p +len;
                        }
                        else p++;
                        break;
                case FIND_NEWLINE_STATE:
                        if(p[0] == '\n')
                        {
                                end = p+1;
                                strncpy(disp_class_list[fbid].disp_mode_list[disp_mode_count].mode, start, (size_t)end -(size_t)start);
                                disp_mode_count ++;
                                state = CHECK_NEXT_STATE;
                                p++;
                if((unsigned int)disp_mode_count >= sizeof(disp_class_list[fbid].disp_mode_list)/sizeof(disp_class_list[fbid].disp_mode_list[0])) goto check_mode_end;
                        }
                        else p++;
                        break;
                default:
                        p++;
                        break;
                }
        }

check_mode_end:

        qsort(&disp_class_list[fbid].disp_mode_list[0], disp_mode_count, sizeof(disp_mode), disp_mode_compare);

    disp_class_list[fbid].disp_mode_length = disp_mode_count;

    return 0;
}

static disp_mode g_config_mode[32];
static int g_config_len = 0;
static int read_mode_finished = 0;

static int read_graphics_fb_mode(int fb)
{
    int size=0;
    int fp_modes=0;
    char fb_modes[1024];
    char temp_name[256];

    if (g_config_len == 0) {
        char conf_modes[1024];
        //int size;
        memset(conf_modes, 0, sizeof(conf_modes));
        memset(&g_config_mode[0], 0, sizeof(g_config_mode));
        int fd = open("/system/etc/display_mode_fb0.conf", O_RDONLY, 0);
        if(fd < 0) {
            LOGE("Warning: /system/etc/display_mode_fb0.conf not defined");
        }
        else {
            size = read(fd, conf_modes, sizeof(conf_modes));
            if(size > 0) {
                char* m_start = conf_modes;
                int m_len = 0;
                char *pmode = conf_modes;
                while(*pmode != '\0') {
                    if (*pmode == '\n') {
                        m_len = pmode - m_start + 1;
                        strncpy(g_config_mode[g_config_len].mode, m_start, m_len);
                        g_config_len ++;
                        m_start = pmode + 1;
                    }
                    pmode ++;
                }//while
            }
            close(fd);
        }//else
    }

    sprintf(temp_name, "/sys/class/graphics/fb%d/modes", fb);
    fp_modes = open(temp_name,O_RDONLY, 0);
    if(fp_modes < 0) {
        LOGI("Error %d! Cannot open %s", fp_modes, temp_name);
        goto set_graphics_fb_mode_error;
    }

    memset(fb_modes, 0, sizeof(fb_modes));
    size = read(fp_modes, fb_modes, sizeof(fb_modes));
    if(size <= 0)
    {
        LOGI("Error! Cannot read %s", temp_name);
        goto set_graphics_fb_mode_error;
    }

    close(fp_modes); fp_modes = 0;

    if(size == sizeof(fb_modes)) fb_modes[size -1] = 0;

    get_available_mode(fb, fb_modes);

    read_mode_finished = 1;
    return 0;

set_graphics_fb_mode_error:

    if(fp_modes > 0) close(fp_modes);

    return -1;
}

int isModeValid(int fb, const char* pMode, int len)
{
    int err = 0;
    int i;

    //LOGW("isModeValid:pMode=%s, len=%d", pMode, len);
    if(read_mode_finished == 0) {
        err = read_graphics_fb_mode(fb);
        if(err)
            return 0;
    }

    for(i=0; i<disp_class_list[fb].disp_mode_length; i++) {
        //LOGW("isModeValid:disp_mode_list[%d].mode=%s", i, disp_class_list[fb].disp_mode_list[i].mode);
        if(!strncmp(disp_class_list[fb].disp_mode_list[i].mode, pMode, len)) {
            return 1;
        }
    }

    return 0;
}

char* getHighestMode(int fb)
{
    int i = 0;

    if(read_mode_finished == 0) {
        read_graphics_fb_mode(fb);
    }

    if(g_config_len > 0) {
        for(i = 0; i<g_config_len; i++) {
            if(isModeValid(fb, g_config_mode[i].mode, strlen(g_config_mode[i].mode)))
                break;
        }
        return g_config_mode[i].mode;
    }

    return disp_class_list[fb].disp_mode_list[i].mode;
}

