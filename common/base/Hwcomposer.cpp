/*
 * Copyright © 2012 Intel Corporation
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Jackie Li <yaodong.li@intel.com>
 *
 */
#include <HwcTrace.h>
#include <Hwcomposer.h>
#include <Dump.h>

namespace android {
namespace intel {

Hwcomposer* Hwcomposer::sInstance(0);

Hwcomposer::Hwcomposer()
    : mProcs(0),
      mDrm(0),
      mPlaneManager(0),
      mBufferManager(0),
      mDisplayContext(0),
      mInitialized(false)
{
    CTRACE();

    mDisplayDevices.setCapacity(IDisplayDevice::DEVICE_COUNT);
}

Hwcomposer::~Hwcomposer()
{
    CTRACE();
    deinitialize();
}

bool Hwcomposer::initCheck() const
{
    return mInitialized;
}

bool Hwcomposer::prepare(size_t numDisplays,
                          hwc_display_contents_1_t** displays)
{
    bool ret = true;

    //Mutex::Autolock _l(mLock);
    RETURN_FALSE_IF_NOT_INIT();
    ATRACE("display count = %d", numDisplays);

    if (!numDisplays || !displays) {
        ETRACE("invalid parameters");
        return false;
    }

    // disable reclaimed planes
    if (mPlaneManager)
        mPlaneManager->disableReclaimedPlanes();

    // reclaim all allocated planes if possible
    for (size_t i = 0; i < numDisplays; i++) {
        IDisplayDevice *device = mDisplayDevices.itemAt(i);
        if (!device) {
            VTRACE("device %d doesn't exist", i);
            continue;
        }

        if (!device->isConnected()) {
            VTRACE("device %d is disconnected", i);
            continue;
        }

        device->prePrepare(displays[i]);
    }

    for (size_t i = 0; i < numDisplays; i++) {
        IDisplayDevice *device = mDisplayDevices.itemAt(i);
        if (!device) {
            VTRACE("device %d doesn't exist", i);
            continue;
        }

        if (!device->isConnected()) {
            VTRACE("device %d is disconnected", i);
            continue;
        }

        ret = device->prepare(displays[i]);
        if (ret == false) {
            ETRACE("failed to do prepare for device %d", i);
            continue;
        }
    }

    return ret;
}

bool Hwcomposer::commit(size_t numDisplays,
                         hwc_display_contents_1_t **displays)
{
    bool ret = true;

    //Mutex::Autolock _l(mLock);
    RETURN_FALSE_IF_NOT_INIT();
    ATRACE("display count = %d", numDisplays);

    if (!numDisplays || !displays) {
        ETRACE("invalid parameters");
        return false;
    }

    mDisplayContext->commitBegin();

    for (size_t i = 0; i < numDisplays; i++) {
        IDisplayDevice *device = mDisplayDevices.itemAt(i);
        if (!device) {
            VTRACE("device %d doesn't exist", i);
            continue;
        }

        if (!device->isConnected()) {
            VTRACE("device %d is disconnected", i);
            continue;
        }

        ret = device->commit(displays[i], mDisplayContext);
        if (ret == false) {
            ETRACE("failed to do commit for device %d", i);
            continue;
        }
    }

    mDisplayContext->commitEnd();

    return ret;
}

bool Hwcomposer::vsyncControl(int disp, int enabled)
{
    RETURN_FALSE_IF_NOT_INIT();
    ATRACE("disp = %d, enabled = %d", disp, enabled);

    if (disp < 0 || disp >= IDisplayDevice::DEVICE_COUNT) {
        ETRACE("invalid disp %d", disp);
        return false;
    }

    IDisplayDevice *device = mDisplayDevices.itemAt(disp);
    if (!device) {
        ETRACE("no device found");
        return false;
    }

    return device->vsyncControl(enabled);
}

bool Hwcomposer::blank(int disp, int blank)
{
    RETURN_FALSE_IF_NOT_INIT();
    ATRACE("disp = %d, blank = %d", disp, blank);

    if (disp < 0 || disp >= IDisplayDevice::DEVICE_COUNT) {
        ETRACE("invalid disp %d", disp);
        return false;
    }

    IDisplayDevice *device = mDisplayDevices.itemAt(disp);
    if (!device) {
        ETRACE("no device found");
        return false;
    }

    return device->blank(blank);
}

bool Hwcomposer::getDisplayConfigs(int disp,
                                      uint32_t *configs,
                                      size_t *numConfigs)
{
    RETURN_FALSE_IF_NOT_INIT();

    if (disp < 0 || disp >= IDisplayDevice::DEVICE_COUNT) {
        ETRACE("invalid disp %d", disp);
        return false;
    }

    IDisplayDevice *device = mDisplayDevices.itemAt(disp);
    if (!device) {
        ETRACE("no device %d found", disp);
        return false;
    }

    return device->getDisplayConfigs(configs, numConfigs);
}

bool Hwcomposer::getDisplayAttributes(int disp,
                                         uint32_t config,
                                         const uint32_t *attributes,
                                         int32_t *values)
{
    RETURN_FALSE_IF_NOT_INIT();

    if (disp < 0 || disp >= IDisplayDevice::DEVICE_COUNT) {
        ETRACE("invalid disp %d", disp);
        return false;
    }

    IDisplayDevice *device = mDisplayDevices.itemAt(disp);
    if (!device) {
        ETRACE("no device found");
        return false;
    }

    return device->getDisplayAttributes(config, attributes, values);
}

bool Hwcomposer::compositionComplete(int disp)
{
    RETURN_FALSE_IF_NOT_INIT();

    if (disp < 0 || disp >= IDisplayDevice::DEVICE_COUNT) {
        ETRACE("invalid disp %d", disp);
        return false;
    }

    mDisplayContext->compositionComplete();

    IDisplayDevice *device = mDisplayDevices.itemAt(disp);
    if (!device) {
        ETRACE("no device found");
        return false;
    }

    return device->compositionComplete();
}

void Hwcomposer::vsync(int disp, int64_t timestamp)
{
    //Mutex::Autolock _l(mLock);

    RETURN_VOID_IF_NOT_INIT();

    if (mProcs && mProcs->vsync) {
        VTRACE("report vsync on disp %d, timestamp %llu", disp, timestamp);
        mProcs->vsync(const_cast<hwc_procs_t*>(mProcs), disp, timestamp);
    }
}

void Hwcomposer::hotplug(int disp, int connected)
{
    //Mutex::Autolock _l(mLock);
    RETURN_VOID_IF_NOT_INIT();

    if (mProcs && mProcs->hotplug) {
        VTRACE("report hotplug on disp %d, connected %d", disp, connected);
        mProcs->hotplug(const_cast<hwc_procs_t*>(mProcs), disp, connected);
    }
}

bool Hwcomposer::release()
{
    RETURN_FALSE_IF_NOT_INIT();

    return true;
}

bool Hwcomposer::dump(char *buff, int buff_len, int *cur_len)
{
    RETURN_FALSE_IF_NOT_INIT();

    Dump d(buff, buff_len);

    // dump composer status
    d.append("Hardware Composer state:");
    // dump device status
    for (size_t i= 0; i < mDisplayDevices.size(); i++) {
        IDisplayDevice *device = mDisplayDevices.itemAt(i);
        if (device)
            device->dump(d);
    }

    // dump plane manager status
    if (mPlaneManager)
        mPlaneManager->dump(d);

    return true;
}

void Hwcomposer::registerProcs(hwc_procs_t const *procs)
{
    CTRACE();

    if (!procs) {
        WTRACE("procs is NULL");
    }
    mProcs = procs;
}

bool Hwcomposer::initialize()
{
    CTRACE();

    // create drm
    mDrm = new Drm();
    if (!mDrm) {
        ETRACE("failed to create DRM");
        return false;
    }

    // create display plane manager
    mPlaneManager = createDisplayPlaneManager();
    if (!mPlaneManager || !mPlaneManager->initialize()) {
        DEINIT_AND_RETURN_FALSE("failed to create display plane manager");
    }

    // create buffer manager
    mBufferManager = createBufferManager();
    if (!mBufferManager || !mBufferManager->initialize()) {
        DEINIT_AND_RETURN_FALSE("failed to create buffer manager");
    }

    mDisplayContext = createDisplayContext();
    if (!mDisplayContext || !mDisplayContext->initialize()) {
        DEINIT_AND_RETURN_FALSE("failed to create display context");
    }

    // create display device
    for (int i = 0; i < IDisplayDevice::DEVICE_COUNT; i++) {
        IDisplayDevice *device = createDisplayDevice(i, *mPlaneManager);
        if (!device || !device->initialize()) {
            ETRACE("failed to create device %d", i);
            delete device;
            continue;
        }
        // add this device
        mDisplayDevices.insertAt(device, i, 1);
    }

    mInitialized = true;
    return true;
init_err:
    deinitialize();
    return false;
}

void Hwcomposer::deinitialize()
{
    // destroy display devices
    for (size_t i = 0; i < mDisplayDevices.size(); i++) {
        IDisplayDevice *device = mDisplayDevices.itemAt(i);
        if (device)
            delete device;
    }
    mDisplayDevices.clear();

    // destroy buffer manager
    if (mBufferManager) {
        delete mBufferManager;
        mBufferManager = 0;
    }

    // destroy display plane manager
    if (mPlaneManager) {
        delete mPlaneManager;
        mPlaneManager = 0;
    }

    if (mDisplayContext) {
        delete mDisplayContext;
        mDisplayContext = 0;
    }

    // destroy drm
    if (mDrm) {
        delete mDrm;
        mDrm = 0;
    }

    mInitialized = false;
}

Drm* Hwcomposer::getDrm()
{
    return mDrm;
}

DisplayPlaneManager* Hwcomposer::getPlaneManager()
{
    return mPlaneManager;
}

BufferManager* Hwcomposer::getBufferManager()
{
    return mBufferManager;
}

IDisplayContext* Hwcomposer::getDisplayContext()
{
    return mDisplayContext;
}

} // namespace intel
} // namespace android
