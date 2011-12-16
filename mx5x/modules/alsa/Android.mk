# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# This is the Freescale ALSA module for i.MX5x

ifeq ($(strip $(BOARD_USES_ALSA_AUDIO)),true)

  LOCAL_PATH := $(call my-dir)

  include $(CLEAR_VARS)

  LOCAL_PRELINK_MODULE := false

  LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

  LOCAL_CFLAGS := -D_POSIX_SOURCE -Wno-multichar

  LOCAL_C_INCLUDES += hardware/alsa_sound external/alsa-lib/include

  LOCAL_SRC_FILES:= alsa_imx5x.cpp

  LOCAL_SHARED_LIBRARIES := \
	libaudio \
  	libasound \
  	liblog   \
    libcutils

  LOCAL_MODULE:= alsa.$(TARGET_BOARD_PLATFORM)

  LOCAL_MODULE_TAGS := eng

  include $(BUILD_SHARED_LIBRARY)

endif
