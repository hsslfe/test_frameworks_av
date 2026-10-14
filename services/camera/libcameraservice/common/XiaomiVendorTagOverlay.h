/*
 * SPDX-FileCopyrightText: Lunaris veux — WhatsApp third-party camera path
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ANDROID_SERVICES_CAMERA_XIAOMI_VENDOR_TAG_OVERLAY_H
#define ANDROID_SERVICES_CAMERA_XIAOMI_VENDOR_TAG_OVERLAY_H

#include <camera/VendorTagDescriptor.h>

namespace android {

status_t appendXiaomiVeuxWhatsAppVendorTags(const sp<VendorTagDescriptor>& descriptor);

} // namespace android

#endif
