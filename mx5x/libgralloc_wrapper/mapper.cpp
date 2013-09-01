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

/* Copyright 2009-2012 Freescale Semiconductor, Inc. All Rights Reserved.*/

#include <limits.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <hardware/hardware.h>
#include <hardware/gralloc.h>

#include "gralloc_priv.h"


/* desktop Linux needs a little help with gettid() */
#if defined(ARCH_X86) && !defined(HAVE_ANDROID_OS)
#define __KERNEL__
# include <linux/unistd.h>
pid_t gettid() { return syscall(__NR_gettid);}
#undef __KERNEL__
#endif

/*****************************************************************************/

static pthread_mutex_t sMapLock = PTHREAD_MUTEX_INITIALIZER; 

/*****************************************************************************/
static gralloc_module_t const*gralloc_get(gralloc_module_t const* module, buffer_handle_t handle)
{
    hw_module_t const* pModule;

    if(!handle)
       return NULL;

    if (((private_handle_t*)handle)->magic == private_handle_t::sMagic)
       return NULL;

    gralloc_module_t *gr = const_cast<gralloc_module_t *>(module);
    private_module_t* m = reinterpret_cast<private_module_t*>(gr);

    if(m->gralloc_viv)
        return m->gralloc_viv;

    if(hw_get_module(GRALLOC_VIV_HARDWARE_MODULE_ID, &pModule))
       return NULL;

    gralloc_module_t const* gralloc_viv = reinterpret_cast<gralloc_module_t const*>(pModule);
    m->gralloc_viv = (gralloc_module_t*)gralloc_viv;

    return gralloc_viv;
}
int gralloc_register_buffer(gralloc_module_t const* module,
        buffer_handle_t handle)
{
    gralloc_module_t const* gralloc_viv = gralloc_get(module, handle);
    if(gralloc_viv){
        return gralloc_viv->registerBuffer(gralloc_viv, handle);
    }

    return -EINVAL;
}

int gralloc_unregister_buffer(gralloc_module_t const* module,
        buffer_handle_t handle)
{
    gralloc_module_t const* gralloc_viv = gralloc_get(module, handle);
    if(gralloc_viv){
        return gralloc_viv->unregisterBuffer(gralloc_viv, handle);
    }

    return -EINVAL;
}

int gralloc_lock(gralloc_module_t const* module,
        buffer_handle_t handle, int usage,
        int l, int t, int w, int h,
        void** vaddr)
{
    gralloc_module_t const* gralloc_viv = gralloc_get(module, handle);
    if(gralloc_viv){
        return gralloc_viv->lock(gralloc_viv, handle, usage, l, t, w, h, vaddr);
    }

    return -EINVAL;
}

int gralloc_unlock(gralloc_module_t const* module, 
        buffer_handle_t handle)
{
    gralloc_module_t const* gralloc_viv = gralloc_get(module, handle);
    if(gralloc_viv){
        return gralloc_viv->unlock(gralloc_viv, handle);
    }

    return -EINVAL;
}
