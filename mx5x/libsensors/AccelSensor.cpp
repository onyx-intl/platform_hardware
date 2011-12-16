/*
 * Copyright (C) 2011 Freescale Semiconductor Inc.
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

#include <fcntl.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <poll.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/select.h>
#include <dlfcn.h>
#include <cutils/log.h>
#include <cutils/properties.h>

#include "AccelSensor.h"

/*****************************************************************************/
AccelSensor::AccelSensor()
: SensorBase(NULL, NULL),
      mEnabled(0),
      mPendingMask(0),
      mInputReader(32),
      mMinPollDelay(0),
      mMaxPollDelay(0),
      mHwRotation(0)
{
    char  hwrotBuf[PROPERTY_VALUE_MAX];

#if defined(ACCELEROMETER_SENSOR_MMA8451)
    data_name = "mma8451";
#elif defined(ACCELEROMETER_SENSOR_MMA8450)
    data_name = "mma8450";
#else
#error you must define accelerometer properly
    data_name = NULL;
    data_fd = -1;
#endif

    if (data_name) {
        data_fd = openInput(data_name);
        getPollFile(data_name);
    }
    memset(mPendingEvents, 0, sizeof(mPendingEvents));

    mPendingEvents[Accelerometer].version = sizeof(sensors_event_t);
    mPendingEvents[Accelerometer].sensor = ID_A;
    mPendingEvents[Accelerometer].type = SENSOR_TYPE_ACCELEROMETER;
    mPendingEvents[Accelerometer].acceleration.status = SENSOR_STATUS_ACCURACY_HIGH;

    /* sensor abs_x/y/z convertion is related to default hardware display orientation
       property and the chip position/direction of the sensor soldered in the board */
    property_get("ro.sf.hwrotation", hwrotBuf, "0");
    mHwRotation = atoi(hwrotBuf) / 90;
}

AccelSensor::~AccelSensor()
{
}

int AccelSensor::enable(int32_t handle, int en)
{
    int what = -1;

    switch (handle) {
        case ID_A: what = Accelerometer; break;
    }

    if (uint32_t(what) >= numSensors)
        return -EINVAL;

    int newState  = en ? 1 : 0;
    int err = 0;

    if ((uint32_t(newState)<<what) != (mEnabled & (1<<what))) {
        uint32_t sensor_type;
        switch (what) {
            case Accelerometer: sensor_type = SENSOR_TYPE_ACCELEROMETER;  break;
        }
        short flags = newState;
        if (en)
            err = accel_enable_sensor(sensor_type);
        else
            err = accel_disable_sensor(sensor_type);

        LOGE_IF(err, "Could not change sensor state (%s)", strerror(-err));
        if (!err) {
            mEnabled &= ~(1<<what);
            mEnabled |= (uint32_t(flags)<<what);
        }
    }
    return err;
}

int AccelSensor::getPollFile(const char* inputName)
{
    FILE *fd = NULL;
    const char *dirname = "/sys/class/input/";
    char sysfs_name[PATH_MAX], *endptr;
    char *filename = NULL, buf[32];
    DIR *dir;
    struct dirent *de;
    int n, path_len;

    poll_sysfs_file_len = 0;
    dir = opendir(dirname);
    if(dir == NULL)
        return -1;

    strcpy(sysfs_name, dirname);
    filename = sysfs_name + strlen(sysfs_name);
    while ((de = readdir(dir))) {
        if ((strlen(de->d_name) < 6) ||
            strncmp(de->d_name, "input", 5))
            continue;

        strcpy(filename, de->d_name);
        strcat(filename, "/");
        path_len = strlen(sysfs_name);
        strcat(filename, "name");
        fd = fopen(sysfs_name, "r");
        if (fd) {
            memset(buf, 0, 32);
            n = fread(buf, 1, 32, fd);
            fclose(fd);
            if ((strlen(buf) >= strlen(inputName)) &&
                !strncmp(buf, inputName, strlen(inputName))) {
                /* Try to open /sys/class/input/input?/poll */
                filename = sysfs_name + path_len;
                strcpy(filename, "poll");
                fd = fopen(sysfs_name, "r+");
                if (fd) {
                    strcpy(poll_sysfs_file,sysfs_name);
                    poll_sysfs_file_len = strlen(poll_sysfs_file);
                    fclose(fd);
                    LOGD("Found %s\n", poll_sysfs_file);

                    /* Get max poll delay time */
                    filename = sysfs_name + path_len;
                    strcpy(filename, "max");
                    fd = fopen(sysfs_name, "r");
                    if (fd) {
                        memset(buf, 0, 32);
                        n = fread(buf, 1, 6, fd);
                        if (n > 0)
                            mMaxPollDelay = strtol(buf, &endptr, 10);
                        fclose(fd);
                    }

                    /* Get min poll delay time */
                    filename = sysfs_name + path_len;
                    strcpy(filename, "min");
                    fd = fopen(sysfs_name, "r");
                    if (fd) {
                        memset(buf, 0, 32);
                        n = fread(buf, 1, 6, fd);
                        if (n > 0)
                            mMinPollDelay = strtol(buf, &endptr, 10);
                        fclose(fd);
                    }
                    LOGD("mMinPollDelay %d, mMaxPollDelay %d\n",
                           mMinPollDelay, mMaxPollDelay);

                    return 0;
                }
            }
        }
   }

   return -1;
}

int AccelSensor::setDelay(int32_t handle, int64_t ns)
{
    FILE *fd = NULL;
    int n, len, ms, ret = -1;
    char buf[6];

    ms = ns / 1000 / 1000;
    LOGD("AccelSensor....setDelay, ms=%d\n", ms);

    if (poll_sysfs_file_len &&
        (ms >= mMinPollDelay) &&
        (ms <= mMaxPollDelay)) {
       fd = fopen(poll_sysfs_file, "r+");
       if (fd) {
           len = 6;
           memset(buf, 0, len);
           snprintf(buf, len, "%d", ms);
           n = fwrite(buf, 1, len, fd);
           fclose(fd);
           ret = 0;
       }else
           LOGE("file %s open failure\n", poll_sysfs_file);
    }else
        LOGE("Error in setDelay %d ms\n", ms);

    return ret;
}

int AccelSensor::readEvents(sensors_event_t* data, int count)
{

    if (count < 1)
        return -EINVAL;

    ssize_t n = mInputReader.fill(data_fd);
    if (n < 0)
        return n;

    int numEventReceived = 0;
    input_event const* event;

    while (count && mInputReader.readEvent(&event)) {
        int type = event->type;
        if (type == EV_ABS) {
            processEvent(event->code, event->value);
            mInputReader.next();
        } else if (type == EV_SYN) {
            int64_t time = timevalToNano(event->time);
            for (int j=0 ; count && mPendingMask && j<numSensors ; j++) {
                if (mPendingMask & (1<<j)) {
                    mPendingMask &= ~(1<<j);
                    mPendingEvents[j].timestamp = time;
                    if (mEnabled & (1<<j)) {
                        *data++ = mPendingEvents[j];
                        count--;
                        numEventReceived++;
                    }
                }
            }
            if (!mPendingMask) {
                mInputReader.next();
            }
        } else {
            LOGE("AccelSensor: unknown event (type=%d, code=%d)",
                    type, event->code);
            mInputReader.next();
        }
    }

    return numEventReceived;
}

void AccelSensor::processEvent(int code, int value)
{
    float temp_x = 0.0f, temp_y = 0.0f, raw_x_ev = 0.0f, raw_y_ev = 0.0f;

    switch (code) {
        case EVENT_TYPE_ACCEL_X:
            mPendingMask |= 1<<Accelerometer;
            temp_x = value * CONVERT_A_X;
            raw_x_ev = 1;
            break;
        case EVENT_TYPE_ACCEL_Y:
            mPendingMask |= 1<<Accelerometer;
            temp_y = value * CONVERT_A_Y;
            raw_y_ev = 1;
            break;
        case EVENT_TYPE_ACCEL_Z:
            mPendingMask |= 1<<Accelerometer;
            mPendingEvents[Accelerometer].acceleration.z = value * CONVERT_A_Z;
            break;
    }

    /* Transfer to correct abs_x/y per hw orientation */
    switch (mHwRotation) {
        case HWROTATION_0:
            if (raw_y_ev)
                mPendingEvents[Accelerometer].acceleration.x = temp_y;
            if (raw_x_ev)
                mPendingEvents[Accelerometer].acceleration.y = -temp_x;
            break;
        case HWROTATION_90:
            if (raw_x_ev)
                mPendingEvents[Accelerometer].acceleration.x = temp_x;
            if (raw_y_ev)
                mPendingEvents[Accelerometer].acceleration.y = temp_y;
            break;
        case HWROTATION_180:
            if (raw_y_ev)
                mPendingEvents[Accelerometer].acceleration.x = -temp_y;
            if (raw_x_ev)
                mPendingEvents[Accelerometer].acceleration.y = temp_x;
            break;
        case HWROTATION_270:
            if (raw_x_ev)
                mPendingEvents[Accelerometer].acceleration.x = -temp_x;
            if (raw_y_ev)
                mPendingEvents[Accelerometer].acceleration.y = -temp_y;
            break;
    }
}
