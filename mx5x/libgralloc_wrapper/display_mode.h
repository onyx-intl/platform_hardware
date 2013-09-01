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

#ifndef _DISPLAY_MODE_H_
#define _DISPLAY_MODE_H_

#define MAX_DISP_DEVICE                  4
#define MAX_DISP_DEVICE_MODE                  128

typedef int boolean;
typedef struct
{
        char mode[20];
        int width;
        int height;
        int freq;
} disp_mode;

typedef enum {
    CHECK_NEXT_STATE,
    FIND_WIDTH_STATE,
    FIND_JOINT_STATE,
    FIND_HEIGHT_STATE,
    PREFIX_FREQ_STATE,
    FREQUENCY_STATE,
    FIND_NEWLINE_STATE
} read_state;

typedef struct
{
    boolean disp_connected;
    boolean disp_enabled;
    disp_mode disp_mode_list[MAX_DISP_DEVICE_MODE];
    int disp_mode_length;
} disp_class;

//most support 4 pluggable display device;
static  disp_class disp_class_list[MAX_DISP_DEVICE];

static int   str2int(char *p, int *len);
static int   disp_mode_compare( const void *arg1, const void *arg2);

static int     get_available_mode(int fbid, const char *mode_list);
static int     read_graphics_fb_mode(int fbid);

#endif
