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
    device/xilinx/zcu102/fstab.common:root/fstab.zcu102.noota \
    device/xilinx/zcu102/init.common.rc:root/init.zcu102.rc \
    device/xilinx/zcu102/init.common.usb.rc:root/init.zcu102.usb.rc \
    device/xilinx/zcu102/ueventd.common.rc:root/ueventd.zcu102.rc

# Copy bootloader envs
PRODUCT_COPY_FILES += \
   $(LOCAL_PATH)/uEnv-noota.txt:boot/uEnv-noota.txt

# Copy prebuilt binaries
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/fsbl.bin:fsbl.bin \
	$(LOCAL_PATH)/xilinx.bit:boot/xilinx.bit

# Remove nav bar
PRODUCT_PROPERTY_OVERRIDES += qemu.hw.mainkeys=1

# Set audio volume persistent default
PRODUCT_PROPERTY_OVERRIDES += persist.audio.volume=118
