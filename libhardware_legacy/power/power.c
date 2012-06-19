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
/* Copyright (C) 2009-2011 Freescale Semiconductor, Inc.*/

#include <hardware_legacy/power.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pthread.h>
#include <linux/delay.h>
#include "cutils/properties.h"

#define LOG_TAG "power"
#include <utils/Log.h>

#include "qemu.h"
#ifdef QEMU_POWER
#include "power_qemu.h"
#endif


enum {
    ACQUIRE_PARTIAL_WAKE_LOCK = 0,
    RELEASE_WAKE_LOCK,
    REQUEST_STATE,
    OUR_FD_COUNT
};

const char * const OLD_PATHS[] = {
    "/sys/android_power/acquire_partial_wake_lock",
    "/sys/android_power/release_wake_lock",
    "/sys/android_power/request_state"
};

const char * const NEW_PATHS[] = {
    "/sys/power/wake_lock",
    "/sys/power/wake_unlock",
    "/sys/power/state"
};

const char * const DVFS_CORE_EN_PATH = "/sys/devices/platform/mxc_dvfs_core.0/enable";
const char * const BUSFREQ_EN_PATH = "/sys/devices/platform/busfreq.0/enable";
static const char SUPP_PROP_NAME[]      = "init.svc.wpa_supplicant";

const char * const AUTO_OFF_TIMEOUT_DEV = "/sys/android_power/auto_off_timeout";

//XXX static pthread_once_t g_initialized = THREAD_ONCE_INIT;
static int g_initialized = 0;
static int g_fds[OUR_FD_COUNT];
static int g_error = 1;

static const char *off_state = "mem";
static const char *on_state = "on";
static const char *eink_state = "standby";

static int64_t systemTime()
{
    struct timespec t;
    t.tv_sec = t.tv_nsec = 0;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec*1000000000LL + t.tv_nsec;
}

static int
open_file_descriptors(const char * const paths[])
{
    int i;
    for (i=0; i<OUR_FD_COUNT; i++) {
        int fd = open(paths[i], O_RDWR);
        if (fd < 0) {
            fprintf(stderr, "fatal error opening \"%s\"\n", paths[i]);
            g_error = errno;
            return -1;
        }
        g_fds[i] = fd;
    }

    g_error = 0;
    return 0;
}

static inline void
initialize_fds(void)
{
    // XXX: should be this:
    //pthread_once(&g_initialized, open_file_descriptors);
    // XXX: not this:
    if (g_initialized == 0) {
        if(open_file_descriptors(NEW_PATHS) < 0) {
            open_file_descriptors(OLD_PATHS);
            on_state = "wake";
            off_state = "standby";
        }
        g_initialized = 1;
    }
}

int
acquire_wake_lock(int lock, const char* id)
{
    initialize_fds();

//    LOGI("acquire_wake_lock lock=%d id='%s'\n", lock, id);

    if (g_error) return g_error;

    int fd;

    if (lock == PARTIAL_WAKE_LOCK) {
        fd = g_fds[ACQUIRE_PARTIAL_WAKE_LOCK];
    }
    else {
        return EINVAL;
    }

    return write(fd, id, strlen(id));
}

int
release_wake_lock(const char* id)
{
    initialize_fds();

//    LOGI("release_wake_lock id='%s'\n", id);

    if (g_error) return g_error;

    ssize_t len = write(g_fds[RELEASE_WAKE_LOCK], id, strlen(id));
    return len >= 0;
}

int
set_last_user_activity_timeout(int64_t delay)
{
//    LOGI("set_last_user_activity_timeout delay=%d\n", ((int)(delay)));

    int fd = open(AUTO_OFF_TIMEOUT_DEV, O_RDWR);
    if (fd >= 0) {
        char buf[32];
        ssize_t len;
        len = snprintf(buf, sizeof(buf), "%d", ((int)(delay)));
        buf[sizeof(buf) - 1] = '\0';
        len = write(fd, buf, len);
        close(fd);
        return 0;
    } else {
        return errno;
    }
}

int
set_screen_state(int on)
{
    QEMU_FALLBACK(set_screen_state(on));

    LOGI("*** set_screen_state %d", on);
    char supp_status[PROPERTY_VALUE_MAX] = {'\0'};

    initialize_fds();

    //LOGI("go_to_sleep eventTime=%lld now=%lld g_error=%s\n", eventTime,
    //      systemTime(), strerror(g_error));

    if (g_error) return g_error;

    char buf[32];
    int len;
    int count = 0;
    if (on == 1) {
        len = sprintf(buf, "%s", on_state);
        len = write(g_fds[REQUEST_STATE], buf, len);
        if (len < 0)
            LOGE("Failed setting last user activity: g_error=%d\n", g_error);
        if (property_get(SUPP_PROP_NAME, supp_status, NULL)
                && strcmp(supp_status, "running") == 0) {
            LOGI("*** wlan_tool:load ");
            while(count < 100){
                property_set("ctl.start", "wlan_tool:load");
                usleep(100000);
                if (property_get("wlan.driver.status", supp_status, NULL)
                        && strcmp(supp_status, "ok") == 0)
                    break;
                count++;
            }
        }
    } else if(on == 0){
        if (property_get(SUPP_PROP_NAME, supp_status, NULL)
                && strcmp(supp_status, "running") == 0) {
            property_set("ctl.start", "wlan_tool:unload");
            LOGI("*** wlan_tool:unload ");
            while(count < 1000){
            usleep(10000);
            if (property_get("wlan.driver.status", supp_status, NULL)
                && strcmp(supp_status, "unloaded") == 0)
                break;
            count++;
            }
        }
        len = sprintf(buf, "%s", off_state);
        len = write(g_fds[REQUEST_STATE], buf, len);
        if (len < 0)
            LOGE("Failed setting last user activity: g_error=%d\n", g_error);
    }else if(on == 2){
        if (property_get(SUPP_PROP_NAME, supp_status, NULL)
                && strcmp(supp_status, "running") == 0) {
            property_set("ctl.start", "wlan_tool:unload");
            LOGI("*** wlan_tool:unload ");
            while(count < 1000){
            usleep(10000);
            if (property_get("wlan.driver.status", supp_status, NULL)
                && strcmp(supp_status, "unloaded") == 0)
                break;
            count++;
            }
        }
        len = sprintf(buf, "%s", eink_state);
        len = write(g_fds[REQUEST_STATE], buf, len);
        if (len < 0)
            LOGE("Failed setting last user activity: g_error=%d\n", g_error);
    }

    return 0;
}

void
enable_dvfs_core(int on)
{
    int fd, len, i;
    char buf[2];

    len = sprintf(buf, "%s", on? "1":"0");

    /* enable dvfs core */
    fd = open(DVFS_CORE_EN_PATH, O_RDWR);
    if (fd <= 0) {
        LOGE("Failed to open file: %s\n", DVFS_CORE_EN_PATH);
        return;
    }
    write(fd, buf, len);
    close(fd);
    LOGD("DVFS core has been %s!\n", on? "enabled":"disabled");

    /* enable bus freq */
    fd = open(BUSFREQ_EN_PATH, O_RDWR);
    if (fd <= 0) {
        LOGE("Failed to open file: %s\n", BUSFREQ_EN_PATH);
        return;
    }
    write(fd, buf, len);
    close(fd);
    LOGD("Bus Frequency has been %s!\n", on? "enabled":"disabled");

}
