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
    device/xilinx/aurora/fstab.common:root/fstab.aurora.noota \
    device/xilinx/aurora/fstab.common:root/fstab.aurora \
    device/xilinx/aurora/init.common.rc:root/init.aurora.rc \
    device/xilinx/aurora/init.common.usb.rc:root/init.aurora.usb.rc \
    device/xilinx/aurora/ueventd.common.rc:root/ueventd.aurora.rc

# Copy prebuilt binaries
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/BOOT.BIN:boot/BOOT.BIN \
	$(LOCAL_PATH)/boot.scr:boot/boot.scr \
	$(LOCAL_PATH)/uboot.env:boot/uboot.env \
	$(LOCAL_PATH)/bd_wrapper.bit:boot/bd_wrapper.bit

# Remove nav bar
#PRODUCT_PROPERTY_OVERRIDES += qemu.hw.mainkeys=1

# Set audio volume persistent default
#PRODUCT_PROPERTY_OVERRIDES += persist.audio.volume=118
