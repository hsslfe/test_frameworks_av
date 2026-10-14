/*
 * SPDX-FileCopyrightText: Lunaris veux — WhatsApp third-party camera path
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "XiaomiVendorTagOverlay"

#include "common/XiaomiVendorTagOverlay.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

#include <cutils/properties.h>
#include <log/log.h>
#include <system/camera_metadata.h>

#include "common/CameraProviderManager.h"

namespace android {

status_t IdlVendorTagDescriptor::appendOverlayTag(const String8& section, const String8& name,
        int32_t type, const char* propertyOverride) {
    uint32_t existingTag = 0;
    if (lookupTag(name, section, &existingTag) == OK) {
        return OK;
    }

    uint32_t sectionId = 0;
    uint32_t maxTagIndex = 0;
    bool sectionFound = false;

    const int tagCount = getTagCount();
    std::vector<uint32_t> tags(tagCount > 0 ? tagCount : 0);
    if (tagCount > 0) {
        getTagArray(tags.data());
    }

    for (const uint32_t tag : tags) {
        const char* tagSection = getSectionName(tag);
        if (tagSection != nullptr && section == String8(tagSection)) {
            sectionFound = true;
            sectionId = tag >> 16;
            maxTagIndex = std::max(maxTagIndex, tag & 0xFFFF);
        }
    }

    if (!sectionFound) {
        for (const uint32_t tag : tags) {
            sectionId = std::max(sectionId, tag >> 16);
        }
        if (sectionId < (CAMERA_METADATA_VENDOR_TAG_BOUNDARY >> 16)) {
            sectionId = (CAMERA_METADATA_VENDOR_TAG_BOUNDARY >> 16);
        } else {
            sectionId += 1;
        }
        maxTagIndex = 0;
        if (mSections.indexOf(section) < 0) {
            mSections.add(section);
        }
    }

    uint32_t newTag = (sectionId << 16) | (maxTagIndex + 1);
    if (newTag < CAMERA_METADATA_VENDOR_TAG_BOUNDARY) {
        ALOGE("%s: computed tag %#x below vendor boundary", __FUNCTION__, newTag);
        return BAD_VALUE;
    }

    if (propertyOverride != nullptr) {
        char prop[PROP_VALUE_MAX] = {};
        if (property_get(propertyOverride, prop, "") > 0) {
            const uint32_t overrideTag = static_cast<uint32_t>(strtoul(prop, nullptr, 0));
            if (overrideTag >= CAMERA_METADATA_VENDOR_TAG_BOUNDARY) {
                newTag = overrideTag;
            }
        }
    }

    mTagToNameMap.add(newTag, name);

    ssize_t sectionIndex = mSections.indexOf(section);
    if (sectionIndex < 0) {
        sectionIndex = mSections.add(section);
    }
    mTagToSectionMap.add(newTag, static_cast<uint32_t>(sectionIndex));
    mTagToTypeMap.add(newTag, type);

    ssize_t reverseIndex = mReverseMapping.indexOfKey(section);
    if (reverseIndex < 0) {
        auto* nameMapper = new KeyedVector<String8, uint32_t>();
        reverseIndex = mReverseMapping.add(section, nameMapper);
    }
    mReverseMapping[reverseIndex]->add(name, newTag);
    mTagCount += 1;

    ALOGI("%s: added %s.%s tag %#x type %d", __FUNCTION__, section.c_str(), name.c_str(), newTag,
            type);
    return OK;
}

namespace {

struct TagDef {
    const char* section;
    const char* name;
    int32_t type;
    const char* propertyOverride;
};

const TagDef kVeuxWhatsAppTags[] = {
        {"com.xiaomi.sessionparams", "thirdPartyYUVSnapshot", TYPE_BYTE,
                "persist.vendor.cam.3pyuv_tag"},
        {"com.xiaomi.sessionparams", "clientName", TYPE_BYTE,
                "persist.vendor.cam.cname_tag"},
};

} // namespace

status_t appendXiaomiVeuxWhatsAppVendorTags(const sp<VendorTagDescriptor>& descriptor) {
    static const bool kEnabled =
            property_get_bool("persist.vendor.cam.strip3pjfif", true);
    if (!kEnabled || descriptor == nullptr) {
        return OK;
    }

    auto* idlDesc = static_cast<IdlVendorTagDescriptor*>(descriptor.get());
    if (idlDesc == nullptr) {
        return BAD_VALUE;
    }

    static bool kLoggedHalTags = false;
    if (!kLoggedHalTags) {
        kLoggedHalTags = true;
        const int tagCount = idlDesc->getTagCount();
        std::vector<uint32_t> tags(tagCount > 0 ? tagCount : 0);
        if (tagCount > 0) {
            idlDesc->getTagArray(tags.data());
        }
        for (const uint32_t tag : tags) {
            const char* section = idlDesc->getSectionName(tag);
            if (section != nullptr &&
                    strncmp(section, "com.xiaomi.", strlen("com.xiaomi.")) == 0) {
                ALOGI("%s: HAL tag %s.%s = %#x", __FUNCTION__, section, idlDesc->getTagName(tag),
                        tag);
            }
        }
    }

    status_t res = OK;
    for (const auto& tagDef : kVeuxWhatsAppTags) {
        const status_t tagRes = idlDesc->appendOverlayTag(String8(tagDef.section),
                String8(tagDef.name), tagDef.type, tagDef.propertyOverride);
        if (tagRes != OK) {
            ALOGE("%s: failed to add %s.%s: %d", __FUNCTION__, tagDef.section, tagDef.name, tagRes);
            res = tagRes;
        }
    }
    return res;
}

} // namespace android
