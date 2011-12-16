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

ifeq ($(BOARD_SOC_CLASS),IMX5X)
ifeq ($(HAVE_FSL_IMX_IPU),true)

LOCAL_PATH := $(call my-dir)

# HAL module implemenation, not prelinked and stored in
# hw/<OVERLAY_HARDWARE_MODULE_ID>.<ro.product.board>.so
include $(CLEAR_VARS)
LOCAL_PRELINK_MODULE := false

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw

LOCAL_SHARED_LIBRARIES:= \
    libui \
    libutils \
    libcutils \
    libc \
    libipu

LOCAL_SRC_FILES := \
		overlay.cpp \
		overlay_pmem.cpp \
		overlay_thread.cpp

LOCAL_C_INCLUDES := \
	external/linux-lib/ipu

LOCAL_MODULE := overlay.$(TARGET_BOARD_PLATFORM)

LOCAL_CFLAGS:= -DPMEM_ALLOCATOR -DMULTI_INSTANCES -DOVERLAY_DEBUG_LOGxx

LOCAL_MODULE_TAGS := eng

include $(BUILD_SHARED_LIBRARY)


include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	overlays_test.cpp

LOCAL_SHARED_LIBRARIES := \
	libcutils \
	libutils \
	libsurfaceflinger \
	libsurfaceflinger_client \
	libbinder \
	libui

LOCAL_MODULE:= mxc-test-overlays

LOCAL_MODULE_TAGS := tests

include $(BUILD_EXECUTABLE)
endif
endif
