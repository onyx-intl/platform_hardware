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

#define LOG_TAG "lights"
//#define LOG_NDEBUG 0

#include <hardware/lights.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>

#define MAX_BRIGHTNESS 255
#define DEF_BACKLIGHT_DEV "pwm-backlight"
#define DEF_BACKLIGHT_PATH "/sys/class/backlight/"

/*****************************************************************************/
struct lights_module_t {
    struct hw_module_t common;
};

static int lights_device_open(const struct hw_module_t* module,
                              const char* name, struct hw_device_t** device);

static struct hw_module_methods_t lights_module_methods = {
    .open= lights_device_open
};

const struct lights_module_t HAL_MODULE_INFO_SYM = {
    .common= {
        .tag= HARDWARE_MODULE_TAG,
        .version_major= 1,
        .version_minor= 0,
        .id= LIGHTS_HARDWARE_MODULE_ID,
        .name= "Lights module",
        .author= "Freescale Semiconductor",
        .methods= &lights_module_methods,
    }
};

static char max_path[256], path[256];
// ****************************************************************************
// module
// ****************************************************************************
static int set_light_backlight(struct light_device_t* dev,
                               struct light_state_t const* state)
{
    int result = -1;
    unsigned int color = state->color;
    unsigned int brightness = 0, max_brightness = 0;
    FILE *file;

    brightness = ((77*((color>>16)&0x00ff)) + (150*((color>>8)&0x00ff)) +
                 (29*(color&0x00ff))) >> 8;
    LOGV("set_light, get brightness=%d", brightness);

    file = fopen(max_path, "r");
    if (!file) {
        LOGE("can not open file %s\n", max_path);
	return result;
    }
    fread(&max_brightness, 1, 3, file);
    fclose(file);

    max_brightness = atoi((char *) &max_brightness);
    brightness = brightness * max_brightness / MAX_BRIGHTNESS;
    LOGV("set_light, max_brightness=%d, target brightness=%d",
        max_brightness, brightness);

    file = fopen(path, "w");
    if (!file) {
        LOGE("can not open file %s\n", path);
        return result;
    }
    fprintf(file, "%d", brightness);
    fclose(file);

    result = 0;
    return result;
}

static int light_close_backlight(struct hw_device_t *dev)
{
    struct light_device_t *device = (struct light_device_t*)dev;
    if (device)
        free(device);
    return 0;
}

/*****************************************************************************/
static int lights_device_open(const struct hw_module_t* module,
                              const char* name, struct hw_device_t** device)
{
    int status = -EINVAL;
    LOGV("lights_device_open\n");
    if (!strcmp(name, LIGHT_ID_BACKLIGHT)) {
        struct light_device_t *dev;
        char value[PROPERTY_VALUE_MAX];

        dev = malloc(sizeof(*dev));

        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->common.tag = HARDWARE_DEVICE_TAG;
        dev->common.version = 0;
        dev->common.module = (struct hw_module_t*) module;
        dev->common.close = light_close_backlight;

        dev->set_light = set_light_backlight;

	*device = &dev->common;

        property_get("hw.backlight.dev", value, DEF_BACKLIGHT_DEV);
        strcpy(path, DEF_BACKLIGHT_PATH);
        strcat(path, value);
        strcpy(max_path, path);
        strcat(max_path, "/max_brightness");
        strcat(path, "/brightness");

        LOGI("max backlight file is %s\n", max_path);
        LOGI("backlight brightness file is %s\n", path);

        status = 0;
    }

    /* todo other lights device init */
    return status;
}
