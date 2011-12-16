LOCAL_PATH := $(call my-dir)
ifneq ($(TARGET_PRODUCT),sim)
# HAL module implemenation, not prelinked and stored in
# hw/<GPS_HARDWARE_MODULE_ID>.<ro.hardware>.so
    ifeq ($(USE_ATHR_GPS_HARDWARE),true)
        include $(CLEAR_VARS)
        LOCAL_PRELINK_MODULE := false
        LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
        LOCAL_SHARED_LIBRARIES := liblog libcutils libhardware
        LOCAL_SRC_FILES := athr_gps.c
        LOCAL_SRC_FILES += gps.c
        LOCAL_MODULE := gps.default
        LOCAL_MODULE_TAGS := eng
        include $(BUILD_SHARED_LIBRARY)
    endif
endif
