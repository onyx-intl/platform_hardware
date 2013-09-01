/*
 * Copyright (C) 2009-2012 Freescale Semiconductor, Inc. All Rights Reserved.
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


#include <hardware/hardware.h>

#include <fcntl.h>
#include <errno.h>

#include <cutils/log.h>
#include <cutils/atomic.h>
#include <cutils/properties.h>

#include <hardware/hwcomposer.h>

#include <EGL/egl.h>
#include "gralloc_priv.h"
#include "hwc_common.h"
/*****************************************************************************/
using namespace android;

struct hwc_context_t {
    hwc_composer_device_t device;
    /* our private state goes below here */
    //now the blit device may only changed in hwc_composer_device open or close.
    blit_device *blit;

    output_device *m_out[MAX_OUTPUT_DISPLAY];
    char m_using[MAX_OUTPUT_DISPLAY]; //0 indicates no output_device, 1 indicates related index;

    //the system property for dual display and overlay switch.
    int display_mode;
    char ui_refresh;
    char vd_refresh;
    int second_display;

    hwc_composer_device_t* viv_hwc;
    layer_record records[LAYER_RECORD_NUM];
};

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device);

static struct hw_module_methods_t hwc_module_methods = {
    open: hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        version_major: 1,
        version_minor: 0,
        id: HWC_HARDWARE_MODULE_ID,
        name: "Sample hwcomposer module",
        author: "The Android Open Source Project",
        methods: &hwc_module_methods,
    }
};

/*****************************************************************************/

static void dump_layer(hwc_layer_t const* l) {
    LOGD("\ttype=%d, flags=%08x, handle=%p, tr=%02x, blend=%04x, {%d,%d,%d,%d}, {%d,%d,%d,%d}",
            l->compositionType, l->flags, l->handle, l->transform, l->blending,
            l->sourceCrop.left,
            l->sourceCrop.top,
            l->sourceCrop.right,
            l->sourceCrop.bottom,
            l->displayFrame.left,
            l->displayFrame.top,
            l->displayFrame.right,
            l->displayFrame.bottom);
}

/***********************************************************************/
static void addRecord(hwc_context_t *dev, hwc_layer_list_t* list)
{
    int rec_index = 0;
    if (list && dev) {
        for(int n=0; n<LAYER_RECORD_NUM; n++) {
            dev->records[n].handle = NULL;
            memset(&(dev->records[n].outRect), 0, sizeof(dev->records[n].outRect));
            dev->records[n].outDev = 0;
        }

        for (size_t i=0 ; i<list->numHwLayers ; i++) {
            //dump_layer(&list->hwLayers[i]);
            //list->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
            hwc_layer_t *layer = &list->hwLayers[i];
            /*
             *the private_handle_t should expand to have usage and format member.
            */
            if(!layer->handle || ((private_handle_t *)layer->handle)->magic != private_handle_t::sMagic) {
                continue;//skip NULL pointer and other magic handler
            }
            if (private_handle_t::validate(layer->handle) < 0) {
                //HWCOMPOSER_LOG_ERR("it is not a valide buffer handle\n");
                continue;
            }
            //HWCOMPOSER_LOG_RUNTIME("<<<<<<<<<<<<<<<hwc_prepare---2>>>>>>>>>>>>>>>>>\n");
            //HWCOMPOSER_LOG_ERR("-------hwc_prepare----layer[%d]-----displayID = %d", i, layer->displayId);
            private_handle_t *handle = (private_handle_t *)(layer->handle);
            if(!(handle->usage & GRALLOC_USAGE_HWC_OVERLAY)) {
                //HWCOMPOSER_LOG_ERR("<<<<<<<<<<<<<<<hwc_prepare---usage=%x>>phy=%x>>>>>>>>>>>>>>>\n", handle->usage, handle->phys);
                continue;
            }

            if(rec_index >= LAYER_RECORD_NUM) {
                HWCOMPOSER_LOG_ERR("******************Error: too many video layers");
                return;
            }
            dev->records[rec_index].handle = (void*)(layer->handle);
            dev->records[rec_index].outRect = layer->displayFrame;
            dev->records[rec_index].outDev = handle->usage & GRALLOC_USAGE_OVERLAY_DISPLAY_MASK;
            rec_index ++;
        }// end for
    }//end if
}

static int isRectEqual(hwc_rect_t* hs, hwc_rect_t* hd)
{
    return ((hs->left == hd->left) && (hs->top == hd->top)
            && (hs->right == hd->right) && (hs->bottom == hd->bottom));
}

static int isInRecord(hwc_context_t *dev, hwc_layer_t *layer)
{
    if(dev && layer) {
        private_handle_t *handle = (private_handle_t *)(layer->handle);
        for(int i=0; i<LAYER_RECORD_NUM; i++) {
            if(((int)(dev->records[i].handle) == (int)(layer->handle))
                      && isRectEqual(&(dev->records[i].outRect), &(layer->displayFrame))
                      && (dev->records[i].outDev == (handle->usage & GRALLOC_USAGE_OVERLAY_DISPLAY_MASK))) {
                return 1;
            }
        }
    }
    return 0;
}

static int hwc_check_property(hwc_context_t *dev)
{
    //bool bValue = false;
    char value[10];

    property_get("rw.VIDEO_TVOUT_DISPLAY", value, "");
    if (strcmp(value, "1") == 0) {
        property_set("sys.VIDEO_OVERLAY_DISPLAY", "0");
        property_set("sys.VIDEO_DISPLAY", "1");
    }
    else if (strcmp(value, "0") == 0)
    {
       property_set("sys.VIDEO_OVERLAY_DISPLAY", "1");
       property_set("sys.VIDEO_DISPLAY", "0");
    }

    property_get("sys.SECOND_DISPLAY_ENABLED", value, "");
    if (strcmp(value, "1") == 0) {
       property_set("sys.VIDEO_OVERLAY_DISPLAY", "2");
       property_set("sys.VIDEO_DISPLAY", "0");
       dev->display_mode &= ~(DISPLAY_MODE_OVERLAY_DISP0 | DISPLAY_MODE_OVERLAY_DISP1 |
                              DISPLAY_MODE_OVERLAY_DISP2 | DISPLAY_MODE_OVERLAY_DISP3);
       dev->display_mode |= DISPLAY_MODE_OVERLAY_DISP0;
       dev->display_mode |= DISPLAY_MODE_OVERLAY_DISP2;
       dev->second_display = 1;
       return 0;
    }
    else if (strcmp(value, "0") == 0)
    {
       dev->second_display = 0;
       property_set("sys.VIDEO_OVERLAY_DISPLAY", "1");
       property_set("sys.VIDEO_DISPLAY", "0");
    }

    /*note:sys.VIDEO_OVERLAY_DISPLAY means the overlay will be combined to which display.
     *the default value is 0 and it indicates nothing.
     *if the value is 1 and it indicates combined to display0.
     *if the value is 2 and it indicates combined to display1.
    */
    property_get("sys.VIDEO_OVERLAY_DISPLAY", value, "");
    dev->display_mode &= ~(DISPLAY_MODE_OVERLAY_DISP0 | DISPLAY_MODE_OVERLAY_DISP1 |
        				DISPLAY_MODE_OVERLAY_DISP2 | DISPLAY_MODE_OVERLAY_DISP3);
    if (strcmp(value, "1") == 0){
        dev->display_mode |= DISPLAY_MODE_OVERLAY_DISP0;
    }
    else if (strcmp(value, "2") == 0){
        dev->display_mode |= DISPLAY_MODE_OVERLAY_DISP1;
    }

    if (strcmp(value, "3") == 0){
        dev->display_mode |= DISPLAY_MODE_OVERLAY_DISP2;
    }
    else if (strcmp(value, "4") == 0){
        dev->display_mode |= DISPLAY_MODE_OVERLAY_DISP3;
    }
    /*note:rw.VIDEO_DISPLAY means the display device.
     *the default value is 0 and it indicates nothing.
     *if the value is 1 and it indicates display1.
     *if the value is 2 and it indicates display2.
    */
    property_get("sys.VIDEO_DISPLAY", value, "");
    dev->display_mode &= ~(DISPLAY_MODE_DISP1 | DISPLAY_MODE_DISP2);
    if (strcmp(value, "1") == 0){
        dev->display_mode |= DISPLAY_MODE_DISP1;
    }
    if (strcmp(value, "2") == 0){
        dev->display_mode |= DISPLAY_MODE_DISP2;
    }
    //HWCOMPOSER_LOG_ERR("************dev->display_mode=%x", dev->display_mode);
	return 0;
}

static int hwc_modify_property(hwc_context_t *dev, private_handle_t *handle)
{
	handle->usage &= ~GRALLOC_USAGE_OVERLAY_DISPLAY_MASK;

    if(dev->display_mode & DISPLAY_MODE_DISP1){
            handle->usage |= GRALLOC_USAGE_HWC_DISP1;
            dev->display_mode &= ~DISPLAY_MODE_DISP1;
	    //return 0;
    }

    if(dev->display_mode & DISPLAY_MODE_DISP2)
            handle->usage |= GRALLOC_USAGE_HWC_DISP2;

	if(dev->display_mode & DISPLAY_MODE_OVERLAY_DISP0){
			handle->usage |= GRALLOC_USAGE_HWC_OVERLAY_DISP0;
			//dev->display_mode &= ~DISPLAY_MODE_OVERLAY_DISP0;
	}
	else if(dev->display_mode & DISPLAY_MODE_OVERLAY_DISP1)
			handle->usage |= GRALLOC_USAGE_HWC_OVERLAY_DISP1;

	if(dev->display_mode & DISPLAY_MODE_OVERLAY_DISP2)
			handle->usage |= GRALLOC_USAGE_HWC_OVERLAY_DISP2;
	else if(dev->display_mode & DISPLAY_MODE_OVERLAY_DISP3)
			handle->usage |= GRALLOC_USAGE_HWC_OVERLAY_DISP3;

    //HWCOMPOSER_LOG_ERR("************handle->usage=%x", handle->usage);
	return 0;
}

/*paramters:
 * usage: devices need to open.
 * ufg:devices not open.
 * puse:index array when device open it need set.
 *check if the output device is exist.
 *return 0 indicates exist; 1 indicates not exist.
*/
static int checkOutputDevice(struct hwc_context_t *ctx, char *puse, int usage, int *ufg)//return -1 indicate not exist.
{
	output_device *out;
	int uFlag = 0;
	int usg = 0;

	for(int i = 0; i < MAX_OUTPUT_DISPLAY; i++) {
		if(ctx->m_using[i]) {
			out = ctx->m_out[i];
			usg = out->getUsage();
			if(usg & usage) {
				uFlag |= (usg & usage);
				if(puse) puse[i] = 1;
			}
		}
	}
	if(ufg != NULL)
		*ufg = usage & ~uFlag;

	return uFlag ^ usage;
}

static int findOutputDevice(struct hwc_context_t *ctx, int *index, int usage, int *ufg)
{
	output_device *out;
	int uFlag = 0;
	int usg = 0;
    *index = -1;

	for(int i = 0; i < MAX_OUTPUT_DISPLAY; i++) {
		if(ctx->m_using[i]) {
			out = ctx->m_out[i];
			usg = out->getUsage();
			if(usg & usage) {
				uFlag = (usg & usage);
				*index = i;
				break;
			}
		}
	}
	if(ufg != NULL)
		*ufg = uFlag;

	return (*ufg) ^ usage;
}

static int findEmpytIndex(struct hwc_context_t *ctx)
{
	for(int i = 0; i < MAX_OUTPUT_DISPLAY; i++) {
		if(!ctx->m_using[i])
			return i;
	}

	HWCOMPOSER_LOG_ERR("the output device array not enough big.\n");
	return -1;
}

//check the output device and delete unused device instance.
static void deleteEmtpyIndex(struct hwc_context_t *ctx)
{
	for(int i = 0; i < MAX_OUTPUT_DISPLAY; i++) {
		if(!ctx->m_using[i]) {
			if(ctx->m_out[i]) {
				output_dev_close(ctx->m_out[i]);
				ctx->m_out[i] = NULL;
			}
		}
	}
}

static char* getDeviceName(hwc_context_t *dev, int usage, int *pUse)
{
    if(dev->second_display) {
        if(usage & GRALLOC_USAGE_HWC_OVERLAY_DISP0) {
            *pUse = GRALLOC_USAGE_HWC_OVERLAY_DISP0;
            return (char *)FB1_DEV_NAME;
        }
        if(usage & GRALLOC_USAGE_HWC_OVERLAY_DISP2) {
            *pUse = GRALLOC_USAGE_HWC_OVERLAY_DISP2;
            return (char *)FB3_DEV_NAME;
        }
    }

    if(usage & GRALLOC_USAGE_HWC_DISP1){
    		*pUse = GRALLOC_USAGE_HWC_DISP1;
    		return (char *)FB2_DEV_NAME;
    }
    if(usage & GRALLOC_USAGE_HWC_OVERLAY_DISP0) {
    		*pUse = GRALLOC_USAGE_HWC_OVERLAY_DISP0;
    		return (char *)FB1_DEV_NAME;
    }
    if(usage & GRALLOC_USAGE_HWC_OVERLAY_DISP1) {
        *pUse = GRALLOC_USAGE_HWC_OVERLAY_DISP1;
        return (char *)FB1_DEV_NAME;
    }//end else if

    if(usage & GRALLOC_USAGE_HWC_OVERLAY_DISP2) {
            *pUse = GRALLOC_USAGE_HWC_OVERLAY_DISP2;
            return (char *)FB3_DEV_NAME;
    }

    return NULL;
}

#if 0
static void setLayerFrame(hwc_layer_t *layer, output_device *out, int usage)
{
    if(usage & GRALLOC_USAGE_HWC_DISP1){
    		layer->displayFrame.left = 0;
    		layer->displayFrame.top = 0;
    		layer->displayFrame.right = out->getWidth();
    		layer->displayFrame.bottom = out->getHeight();
    }
//    if(handle->usage & GRALLOC_USAGE_HWC_OVERLAY0_DISP0) {
//    		display_frame =;
//    }
//    if(handle->usage & GRALLOC_USAGE_HWC_OVERLAY0_DISP1) {
//        display_frame =;
//    }//end else if
}
#endif

static int validate_displayFrame(hwc_layer_t *layer)
{
    int isValid = 0;
    hwc_rect_t *disFrame = &(layer->displayFrame);
    isValid = ((disFrame->left >= 0) && (disFrame->right >= 0) && (disFrame->top >= 0) &&
            (disFrame->bottom >= 0) && ((disFrame->right - disFrame->left) >= 0) &&
            ((disFrame->bottom  - disFrame->top) >= 0));
    return isValid;
}

static void checkDisplayFrame(struct hwc_context_t *ctx, hwc_layer_t *layer, int usage)
{
    output_device *out;
    int usg;
    hwc_rect_t *disFrame = &(layer->displayFrame);

    for(int i = 0; i < MAX_OUTPUT_DISPLAY; i++) {
        if(ctx->m_using[i] && ctx->m_out[i] && (usage & (GRALLOC_USAGE_OVERLAY0_MASK | GRALLOC_USAGE_OVERLAY1_MASK))) {
            out = ctx->m_out[i];
            usg = out->getUsage();
            if(usg & usage) {
                out->setDisplayFrame(disFrame);
            }
        }
    }//end for

}

static int open_outputDevice(struct hwc_context_t *ctx, const char *dev_name, output_device **device, int flag)
{
    int usage;
    output_device *out;
    for(int i = 0; i < MAX_OUTPUT_DISPLAY; i++) {
        if(ctx->m_using[i] && ctx->m_out[i]) {
            out = ctx->m_out[i];
            usage = out->getUsage();
            if(((usage | flag) == GRALLOC_USAGE_OVERLAY0_MASK) || ((usage | flag) == GRALLOC_USAGE_OVERLAY1_MASK)) {
                output_dev_close(ctx->m_out[i]);
                ctx->m_using[i] = 0;
                ctx->m_out[i] = NULL;
            }
        }
    }
    return output_dev_open(dev_name, device, flag);
}

static int hwc_prepare(hwc_composer_device_t *dev, hwc_layer_list_t* list) {
    char out_using[MAX_OUTPUT_DISPLAY] = {0};

    struct hwc_context_t *ctx = (struct hwc_context_t *)dev;
    if(ctx) {
        if(ctx->viv_hwc)
            ctx->viv_hwc->prepare(ctx->viv_hwc, list);
	//hwc_check_property(ctx);
    }
    if (list && dev) {
        for (size_t i=0 ; i<list->numHwLayers ; i++) {
            //dump_layer(&list->hwLayers[i]);
            //list->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
            hwc_layer_t *layer = &list->hwLayers[i];
            /*
             *the private_handle_t should expand to have usage and format member.
            */
            if(!layer->handle || ((private_handle_t *)layer->handle)->magic != private_handle_t::sMagic) {
                continue;//skip NULL pointer and other magic handler
            }
	    if (private_handle_t::validate(layer->handle) < 0) {
		//HWCOMPOSER_LOG_ERR("it is not a valide buffer handle\n");
		continue;
	    }
	    //HWCOMPOSER_LOG_RUNTIME("<<<<<<<<<<<<<<<hwc_prepare---2>>>>>>>>>>>>>>>>>\n");
            private_handle_t *handle = (private_handle_t *)(layer->handle);
            if(!(handle->usage & GRALLOC_USAGE_HWC_OVERLAY)) {
                //HWCOMPOSER_LOG_ERR("<<<<<<<<<<<<<<<hwc_prepare---usage=%x>>phy=%x>>>>>>>>>>>>>>>\n", handle->usage, handle->phys);
            	continue;
            }
            HWCOMPOSER_LOG_RUNTIME("<<<<<<<<<<<<<<<hwc_prepare---3>usage=%x, phy=%x>>>>>>>>>>>>>>>>\n", handle->usage, handle->phys);
            hwc_check_property(ctx);
	    layer->compositionType = HWC_OVERLAY;
	    //if(handle->usage & GRALLOC_USAGE_HWC_DISP1)
	    //handle the display frame position for tv out.
	    hwc_modify_property(ctx, handle);

            if(!validate_displayFrame(layer)) {
                HWCOMPOSER_LOG_INFO("<<<<<<<<<<<<<<<hwc_prepare---3-2>>>>>>>>>>>>>>>>\n");
                continue;
            }

            int status = -EINVAL;
            int index = 0;
            int retv = 0;
            int m_usage = 0;
            int i_usage = handle->usage & GRALLOC_USAGE_OVERLAY_DISPLAY_MASK;
            //HWCOMPOSER_LOG_ERR("<<<<<<<<<<<<<<<hwc_prepare---3-3>>>>>usage=%x>>>i_usage=%x>>>>>>>>\n", handle->usage, i_usage);
            retv = checkOutputDevice(ctx, out_using, i_usage, &m_usage);
            while(retv && m_usage) {
		    int ruse = 0;
		    char *dev_name = NULL;
		    dev_name = getDeviceName(ctx, m_usage, &ruse);
	            m_usage &= ~ruse;
	            HWCOMPOSER_LOG_RUNTIME("<<<<<<<<<<<<<<<hwc_prepare---4>>>>>>>>>>>>>>>>>\n");
	            if(dev_name == NULL) {
			HWCOMPOSER_LOG_INFO("****Warnning: layer buffer usage(%x) does not support!", handle->usage);
			HWCOMPOSER_LOG_INFO("****Warnning:  the layer buffer will be handled in surfaceflinger");
			layer->compositionType = HWC_FRAMEBUFFER;
			continue;
	            }//end else

	            index = findEmpytIndex(ctx);
	            if(index == -1) {
            		HWCOMPOSER_LOG_ERR("Error:findEmpytIndex failed");
            		return HWC_EGL_ERROR;
	            }
	            if(ctx->m_out[index])
			deleteEmtpyIndex(ctx);

		        status = open_outputDevice(ctx, dev_name, &(ctx->m_out[index]), ruse);//output_dev_open(dev_name, &(ctx->m_out[index]), ruse);
		        if(status < 0){
		            HWCOMPOSER_LOG_ERR("Error! open output device failed!");
		            continue;
		        }//end if
		        out_using[index] = 1;
		        ctx->m_using[index] = 1;
		        //setLayerFrame(layer, ctx->m_out[index], ruse);
            }//end while
            checkDisplayFrame(ctx, layer, i_usage);
        }//end for
        for(int i = 0; i < MAX_OUTPUT_DISPLAY; i++) {
		if(!out_using[i] && ctx->m_using[i]) {
			ctx->m_using[i] = 0;
			deleteEmtpyIndex(ctx);
		}
		//ctx->m_using[i] = out_using[i];
	}
    }//end if
    return 0;
}

static int releaseAllOutput(struct hwc_context_t *ctx)
{
		for(int i = 0; i < MAX_OUTPUT_DISPLAY; i++) {
				if(ctx->m_using[i]) {
						output_dev_close(ctx->m_out[i]);
						ctx->m_using[i] = 0;
						ctx->m_out[i] = NULL;
				}
		}

		return 0;
}

static int getActiveOuputDevice(struct hwc_context_t *ctx)
{
		int num = 0;
		for(int i = 0; i < MAX_OUTPUT_DISPLAY; i++) {
				if(ctx->m_out[i] && ctx->m_using[i])
						num ++;
		}

		return num;
}

static int hwc_setUpdateMode(hwc_composer_device_t *dev, char refresh, char vRefresh)
{
    struct hwc_context_t *ctx = (struct hwc_context_t *)dev;
    if(ctx == NULL) return 0;

    ctx->ui_refresh = refresh;
    ctx->vd_refresh = vRefresh;
    return 0;
}

static int hwc_set(hwc_composer_device_t *dev,
        hwc_display_t dpy,
        hwc_surface_t sur,
        hwc_layer_list_t* list)
{
    struct hwc_context_t *ctx = (struct hwc_context_t *)dev;
    //for (size_t i=0 ; i<list->numHwLayers ; i++) {
    //    dump_layer(&list->hwLayers[i]);
    //}
    //hwc_buffer *outBuff[MAX_OUTPUT_DISPLAY];
    //when displayhardware do releas function, it will come here.
    if(ctx && (dpy == NULL) && (sur == NULL) && (list == NULL)) {
	//close the output device.
        if(ctx->viv_hwc)
            ctx->viv_hwc->set(ctx->viv_hwc, dpy, sur, list);
	releaseAllOutput(ctx);
	//ctx->display_mode_changed = 1;

	return 0;
    }
    ctx->ui_refresh = 1;
    ctx->vd_refresh = 1;
    if((ctx == NULL) || (ctx && ctx->ui_refresh)) {
        EGLBoolean sucess;
        if(ctx->viv_hwc)
            sucess = !ctx->viv_hwc->set(ctx->viv_hwc, dpy, sur, list);
        else
            sucess = eglSwapBuffers((EGLDisplay)dpy, (EGLSurface)sur);
        if (!sucess) {
            return HWC_EGL_ERROR;
        }
    }
    if(list == NULL || dev == NULL || !ctx->vd_refresh) {
    	return 0;
    }
    if(getActiveOuputDevice(ctx) == 0) {return 0;}//eglSwapBuffers((EGLDisplay)dpy, (EGLSurface)sur); return 0;}
    HWCOMPOSER_LOG_RUNTIME("%s,%d", __FUNCTION__, __LINE__);

    int status = -EINVAL;
    hwc_buffer out_buffer[MAX_OUTPUT_DISPLAY];
    char bufs_state[MAX_OUTPUT_DISPLAY];
    memset(bufs_state, 0, sizeof(bufs_state));
    memset(out_buffer, 0, sizeof(out_buffer));
    blit_device *bltdev = ctx->blit;
    for (size_t i=0 ; i<list->numHwLayers ; i++){
	hwc_layer_t *layer = &list->hwLayers[i];
        if(!layer->handle || ((private_handle_t *)layer->handle)->magic != private_handle_t::sMagic) {
    	    HWCOMPOSER_LOG_RUNTIME("%s,%d", __FUNCTION__, __LINE__);
            continue;
        }
	if (private_handle_t::validate(layer->handle) < 0) {
    	    HWCOMPOSER_LOG_RUNTIME("%s,%d, not a valide buffer handle", __FUNCTION__, __LINE__);
    	    continue;
	}

        if(!validate_displayFrame(layer)) {
    	    HWCOMPOSER_LOG_RUNTIME("%s,%d", __FUNCTION__, __LINE__);
            continue;
        }
        HWCOMPOSER_LOG_RUNTIME("%s,%d", __FUNCTION__, __LINE__);

        //when GM do seek, it always queue the same buffer.
        //so, we can not judge the reduplicated buffer by buffer handle now.
        if(isInRecord(ctx, layer)) {
            HWCOMPOSER_LOG_RUNTIME("%s,%d, lost frames", __FUNCTION__, __LINE__);
            //continue;
        }

	private_handle_t *handle = (private_handle_t *)(layer->handle);
	if(handle->usage & GRALLOC_USAGE_HWC_OVERLAY){
            int retv = 0;
            int m_usage = 0;
            int i_usage = handle->usage & GRALLOC_USAGE_OVERLAY_DISPLAY_MASK;
            if(!i_usage) continue;
            do {
    		output_device *outdev = NULL;
    		int index = 0;
        	retv = findOutputDevice(ctx, &index, i_usage, &m_usage);
                i_usage &= ~m_usage;
                if((index >= 0) && (index < MAX_OUTPUT_DISPLAY)) {
                    outdev = ctx->m_out[index];
                }else {
                    break;
                }

    		if(outdev != NULL) {
			if(!bufs_state[index] && ctx->m_using[index]) {
				outdev->fetch(&out_buffer[index]);
				bufs_state[index] = 1;
			}
			if(!bufs_state[index])
				continue;
			status = bltdev->blit(layer, &(out_buffer[index]));
			if(status < 0){
				HWCOMPOSER_LOG_ERR("Error! bltdev->blit() failed!");
				continue;
			}
    		}//end if(outdev != NULL)
            }while(retv);

		}//end if
    }//end for
    for(int i = 0; i < MAX_OUTPUT_DISPLAY; i++) {
	if(ctx->m_using[i] && bufs_state[i]) {
		status = ctx->m_out[i]->post(&out_buffer[i]);
		if(status < 0){
			HWCOMPOSER_LOG_ERR("Error! output device post buffer failed!");
			continue;
		}
	}
    }
    addRecord(ctx, list);

    return 0;
}

static int hwc_device_close(struct hw_device_t *dev)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    if (ctx) {
    		if(ctx->blit)
    				blit_dev_close(ctx->blit);
        releaseAllOutput(ctx);
        if(ctx->viv_hwc)
            hwc_close(ctx->viv_hwc);

        free(ctx);
    }
    return 0;
}

/*****************************************************************************/

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int status = -EINVAL;
    if (!strcmp(name, HWC_HARDWARE_COMPOSER)) {
        struct hwc_context_t *dev;
        dev = (hwc_context_t*)malloc(sizeof(*dev));

        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = 0;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = hwc_device_close;

        dev->device.prepare = hwc_prepare;
        dev->device.set = hwc_set;
        dev->device.setUpdateMode = hwc_setUpdateMode;

        *device = &dev->device.common;

        /* our private state goes below here */
        status = blit_dev_open(BLIT_IPU, &(dev->blit));
        if(status < 0){
        	  HWCOMPOSER_LOG_ERR("Error! blit_dev_open failed!");
        	  goto err_exit;
        }

        const hw_module_t *hwc_module;
        if(hw_get_module(HWC_VIV_HARDWARE_MODULE_ID,
                        (const hw_module_t**)&hwc_module) < 0) {
            HWCOMPOSER_LOG_ERR("Error! hw_get_module viv_hwc failed");
            goto nor_exit;
        }
        if(hwc_open(hwc_module, &(dev->viv_hwc)) != 0) {
            HWCOMPOSER_LOG_ERR("Error! viv_hwc open failed");
            goto nor_exit;
        }
nor_exit:
	HWCOMPOSER_LOG_RUNTIME("%s,%d", __FUNCTION__, __LINE__);
        return 0;
err_exit:
	if(dev){
		if(dev->blit) {
			blit_dev_close(dev->blit);
		}
		free(dev);
	}
				//status = -EINVAL;
        /****************************************/
    }
    return status;
}
