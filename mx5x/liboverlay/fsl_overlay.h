/**
 *  Copyright (C) 2011 Freescale Semiconductor Inc.,
 *  All Rights Reserved.
 */

#ifndef __FSL_OVERLAY_H__
#define __FSL_OVERLAY_H__

#include <linux/videodev.h>
#include <hardware/overlay.h>
#include <semaphore.h>
#include <pthread.h>
#include <cutils/log.h>
#include "overlay_utils.h"

extern "C" {
#include "mxc_ipu_hl_lib.h"
}

typedef struct
{
  uint32_t marker;
  uint32_t size;
  volatile int32_t refCnt;
  int instance_id;
  int crop_x;
  int crop_y;
  int crop_w;
  int crop_h;
  int num_buffer;
  int free_count;
  int free_head;
  int free_tail;
  unsigned int free_bufs[MAX_OVERLAY_BUFFER_NUM];//phy addr, FIFO queue
  int queued_count;
  int queued_head;
  int queued_tail;
  unsigned int queued_bufs[MAX_OVERLAY_BUFFER_NUM];//phy addr, FIFO queue
  unsigned int buf_displayed[MAX_OVERLAY_BUFFER_NUM];//phy addr, FIFO queue
  pthread_mutex_t obj_lock;
  pthread_cond_t free_cond;
  int wait_buf_flag;
  unsigned int buf_showed;
  int overlay_mode;
  int in_destroy;
  int buf_mixing;
  bool for_playback;
} overlay_data_shared_t;

typedef struct
{
  uint32_t marker;
  uint32_t size;
  volatile int32_t refCnt;
  sem_t overlay_sem;
} overlay_control_shared_t;


struct handle_t : public native_handle {
    /* add the data fields we need here, for instance: */
    int control_shared_fd;
    int data_shared_fd;
    int control_shared_size;
    int data_shared_size;
    int width;
    int height;
    int32_t format;
    int num_buffer;//Number of buffers for overlay
    int buf_size;
};

typedef enum {
    DISPLAY_MODE_NORMAL = 0,
    DISPLAY_MODE_TV = 1,
    DISPLAY_MODE_DUAL_DISP = 2,
}DISPLAY_MODE;

typedef enum {
    SIN_VIDEO_DUAL_UI = 0,
    DUAL_VIDEO_SIN_UI = 1,
}VIDEO_PLAY_MODE;

/*
 * This is the overlay_t object, it is returned to the user and represents
 * an overlay.
 * This handles will be passed across processes and possibly given to other
 * HAL modules (for instance video decode modules).
 */

class overlay_object : public overlay_t {
    
    static overlay_handle_t getHandleRef(struct overlay_t* overlay) {
        /* returns a reference to the handle, caller doesn't take ownership */
        overlay_handle_t retPtr = &(static_cast<overlay_object *>(overlay)->mHandle);
        LOGI("getHandleRef return overlay_handle_t 0x%p",retPtr);
        return retPtr;
    }
    
public:
    handle_t mHandle;
    overlay_data_shared_t *mDataShared;
    int rotation;
    int outX;
    int outY;
    int outW;
    int outH;
    int zorder;//zorder for mutl-overlays
    //Maybe need a lock here
    bool out_changed;
    int visible;
    overlay_object(uint32_t w, uint32_t h, int32_t format,int control_fd, int control_size);
    ~overlay_object();
};

class OverlayThread;

struct overlay_control_context_t {
    struct overlay_control_device_t device;
    /* our private state goes below here */
    pthread_mutex_t control_lock;
    overlay_object *overlay_intances[MAX_OVERLAY_INSTANCES];//Overlay device instance arrays
    int overlay_instance_valid[MAX_OVERLAY_INSTANCES];
    int overlay_number;//Overlay device instance number valid in instance arrays
    OverlayThread *overlay_thread;//Overlay mixer thread
    bool overlay_running;//Overlay mixer thread state
    int control_shared_fd;
    int control_shared_size;
    overlay_control_shared_t *control_shared;
    __u32 outpixelformat;
    //foreground setting for fb0/1
    int fb_dev;
    int xres;//fb0/1's resolution
    int yres;//fb0/1's resolution
    //fb0/1's colorkey
    int alpha_buf_size;
    int cur_alpha_buffer;
    OVERLAY_BUFFER alpha_buffers[DEFAULT_ALPHA_BUFFERS];//fb0/1's local alpha buffer

    //tvout setting

    //V4L setting
    int v4l_id;//V4L Handler
    bool stream_on;
    char *v4lbuf_addr[MAX_V4L_BUFFERS];//V4L buffer arrays
    struct v4l2_buffer v4l_buffers[MAX_V4L_BUFFERS];
    int v4l_bufcount;//V4L buffer numbers
    int video_frames;
    RECTTYPE default_fb_resolution;

    RECTTYPE fb1_resolution;    
    RECTTYPE user_property;

    DISPLAY_MODE display_mode;
    VIDEO_PLAY_MODE video_play_mode;
    bool sec_disp_enable;
    ipu_lib_handle_t sec_video_IPUHandle;
};


int overlay_init_fbdev(struct overlay_control_context_t *dev, const char* dev_name);

int overlay_deinit_fbdev(struct overlay_control_context_t *dev);

int overlay_init_v4l(struct overlay_control_context_t *dev, int layer);

int overlay_deinit_v4l(struct overlay_control_context_t *dev);

int fill_frame_back(char * frame,int frame_size, int xres,
                           int yres, unsigned int pixelformat);

#endif

