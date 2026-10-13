/*
 * Copyright 2024 The LibreMobileOS Foundation
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

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <android/log.h>
#include <cutils/properties.h>

#include "common/CameraProviderExtension.h"

#define LOG_TAG "CameraProviderExtension"
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Verify these sysfs nodes on the target device before enabling torch strength control.
static const char* kTorchStrengthPaths[] = {
    "/sys/devices/platform/soc/5c1b000.qcom,cci0/5c1b000.qcom,cci0:qcom,camera-flash@0/torch_strength",
    "/sys/devices/platform/soc/5c1b000.qcom,cci0/5c1b000.qcom,cci0:qcom,camera-flash@1/torch_strength",
    "/sys/devices/platform/soc/5c1b000.qcom,cci0/5c1b000.qcom,cci0:qcom,camera-flash@3/torch_strength",
};

static void writeTorchStrengthPath(const char* path, int32_t torchStrength, bool* found) {
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        ALOGE("Failed to open %s (errno: %d)", path, errno);
        return;
    }

    char buf[16];
    int len = snprintf(buf, sizeof(buf), "%d", torchStrength);
    ssize_t written = write(fd, buf, len);
    if (written == len) {
        if (found != nullptr) *found = true;
    } else {
        ALOGE("Failed to write %d to %s (errno: %d)", torchStrength, path, errno);
    }
    close(fd);
}

static void setTorchStrengthSysfs(int32_t torchStrength) {
    bool found = false;
    for (const char* path : kTorchStrengthPaths) {
        if (access(path, F_OK) == 0) {
            writeTorchStrengthPath(path, torchStrength, &found);
        }
    }
    if (!found) {
        ALOGE("Could not write any torch_strength sysfs node");
    }
}

bool supportsTorchStrengthControlExt() {
    return true;
}

int32_t getTorchDefaultStrengthLevelExt() {
    return 86;
}

int32_t getTorchMaxStrengthLevelExt() {
    return 127;
}

int32_t getTorchStrengthLevelExt() {
    char value[16] = {};
    int fd = open(kTorchStrengthPaths[0], O_RDONLY);
    if (fd < 0) return 0;
    ssize_t n = read(fd, value, sizeof(value) - 1);
    close(fd);
    if (n <= 0) return 0;
    return static_cast<int32_t>(strtol(value, nullptr, 10));
}

void setTorchStrengthLevelExt(int32_t torchStrength, bool enabled) {
    if (!enabled) torchStrength = 0;
    if (torchStrength > 127) torchStrength = 127;
    if (torchStrength < 0) torchStrength = 0;
    setTorchStrengthSysfs(torchStrength);
}

int32_t getCameraCaptureFlashStrengthLevelExt() {
    return getTorchMaxStrengthLevelExt();
}

void applyCameraCaptureFlashStrengthToSysfs() {
    setTorchStrengthLevelExt(getCameraCaptureFlashStrengthLevelExt(), true);
}

void restoreTorchStrengthSysfsFromPersist() {
    char value[PROPERTY_VALUE_MAX] = {};
    if (property_get("persist.flashlight.strength", value, "") <= 0) return;

    char* end = nullptr;
    long parsed = strtol(value, &end, 10);
    if (end == value || parsed < 0 || parsed > 127) return;
    setTorchStrengthLevelExt(static_cast<int32_t>(parsed), parsed > 0);
}
