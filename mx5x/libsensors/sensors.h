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

#ifndef ANDROID_SENSORS_H
#define ANDROID_SENSORS_H

#include <stdint.h>
#include <errno.h>
#include <sys/cdefs.h>
#include <sys/types.h>

#include <linux/input.h>

#include <hardware/hardware.h>
#include <hardware/sensors.h>

__BEGIN_DECLS

/*****************************************************************************/

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

#define ID_A  (0)
#define ID_M  (1)
#define ID_O  (2)
#define ID_L  (3)
#define ID_P  (4)
#define ID_GY (5)

#define HWROTATION_0   (0)
#define HWROTATION_90  (1)
#define HWROTATION_180 (2)
#define HWROTATION_270 (3)

/*****************************************************************************/

/*
 * The SENSORS Module
 */

/*****************************************************************************/

#define EVENT_TYPE_ACCEL_X          ABS_X
#define EVENT_TYPE_ACCEL_Y          ABS_Y
#define EVENT_TYPE_ACCEL_Z          ABS_Z

#define EVENT_TYPE_YAW              ABS_RX
#define EVENT_TYPE_PITCH            ABS_RY
#define EVENT_TYPE_ROLL             ABS_RZ
#define EVENT_TYPE_ORIENT_STATUS    ABS_WHEEL

#define EVENT_TYPE_MAGV_X           ABS_X
#define EVENT_TYPE_MAGV_Y           ABS_Y
#define EVENT_TYPE_MAGV_Z           ABS_Z

#define EVENT_TYPE_LIGHT            ABS_MISC

#if defined(ACCELEROMETER_SENSOR_MMA8451)
  #define LSG                         (4096.0f) // 4096 LSG = 1G for MMA8451
#elif defined(ACCELEROMETER_SENSOR_MMA8450)
  #define LSG                         (1024.0f) // 1024 LSG = 1G for MMA8450
#else
  #define LSG                         (720.0f)
#endif

// conversion of acceleration data to SI units (m/s^2)
#define RANGE_A                     (2*GRAVITY_EARTH)
#define CONVERT_A                   (GRAVITY_EARTH / LSG)
#define CONVERT_A_X                 (CONVERT_A)
#define CONVERT_A_Y                 (-CONVERT_A)
#define CONVERT_A_Z                 (-CONVERT_A)

// conversion of magnetic data to uT units
#define CONVERT_M                   (1.0f/20.0f)
#define CONVERT_M_X                 (CONVERT_M)
#define CONVERT_M_Y                 (CONVERT_M)
#define CONVERT_M_Z                 (-CONVERT_M)

/* conversion of orientation data to degree units */
#define CONVERT_O                   (1.0f/100.0f)
#define CONVERT_O_Y                 (CONVERT_O)
#define CONVERT_O_P                 (CONVERT_O)
#define CONVERT_O_R                 (CONVERT_O)

#define SENSOR_STATE_MASK           (0x7FFF)

/*****************************************************************************/

__END_DECLS

#endif  // ANDROID_SENSORS_H
