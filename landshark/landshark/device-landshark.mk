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

# Copy basic config files
PRODUCT_COPY_FILES += \
    device/xilinx/landshark/fstab.common:root/fstab.landshark.noota \
    device/xilinx/landshark/fstab.common:root/fstab.landshark \
    device/xilinx/landshark/init.common.rc:root/init.landshark.rc \
    device/xilinx/landshark/init.common.usb.rc:root/init.landshark.usb.rc \
    device/xilinx/landshark/ueventd.common.rc:root/ueventd.landshark.rc

# Copy media codec settings
PRODUCT_COPY_FILES +=  \
    frameworks/av/media/libstagefright/data/media_codecs_google_audio.xml:system/etc/media_codecs_google_audio.xml \
    frameworks/av/media/libstagefright/data/media_codecs_google_video.xml:system/etc/media_codecs_google_video.xml \
    device/xilinx/common/media_codecs.xml:system/etc/media_codecs.xml

# Copy prebuilt binaries
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/BOOT.BIN:boot/BOOT.BIN \
	$(LOCAL_PATH)/boot.scr:boot/boot.scr \
	$(LOCAL_PATH)/uEnv.txt:boot/uEnv.txt \
	$(LOCAL_PATH)/empty.txt:boot/iveia-helios-z8.dtb \
	$(LOCAL_PATH)/bd_wrapper.bit:boot/bd_wrapper.bit

# Remove nav bar
#PRODUCT_PROPERTY_OVERRIDES += qemu.hw.mainkeys=1

# Set audio volume persistent default
PRODUCT_PROPERTY_OVERRIDES += persist.audio.volume=118
