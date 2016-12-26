#
# Copyright (C) 2016 The Android Open-Source Project
# Copyright (C) 2016 Mentor Graphics Inc.
# Copyright (C) 2016 Xilinx Inc.
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
#

# Adjust the dalvik heap to be appropriate for a tablet.
$(call inherit-product, frameworks/native/build/tablet-10in-xhdpi-2048-dalvik-heap.mk)

# Build and run only ART
OVERRIDE_RUNTIMES := runtime_libart_default

# Copy basic config files
PRODUCT_COPY_FILES += \
    $(LOCAL_PATH)/fstab.zcu102:root/fstab.zcu102 \
    $(LOCAL_PATH)/init.zcu102.rc:root/init.zcu102.rc \
    $(LOCAL_PATH)/init.zcu102.usb.rc:root/init.zcu102.usb.rc \
    $(LOCAL_PATH)/ueventd.zcu102.rc:root/ueventd.zcu102.rc

# Copy supported hardware features config file(s)
PRODUCT_COPY_FILES +=  \
	device/xilinx/zcu102/required_hardware.xml:system/etc/permissions/required_hardware.xml

# Set custom settings for the framework
DEVICE_PACKAGE_OVERLAYS += \
    device/xilinx/zcu102/overlay

# Add libion for graphics
PRODUCT_PACKAGES += \
	libion

# Include libs for SW graphics
PRODUCT_PACKAGES += libGLES_android

# Copy bootloader envs
PRODUCT_COPY_FILES += \
   device/xilinx/zcu102/uEnv.txt:uEnv.txt

# Copy prebuilt BOOT.BIN if it exists
PRODUCT_COPY_FILES += $(call add-to-product-copy-files-if-exists,\
	$(LOCAL_PATH)/BOOT.BIN:BOOT.BIN)
