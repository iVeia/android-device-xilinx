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

# Inherit 64-bit device configuration
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)

# Inherit the full_base and device configurations
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)
$(call inherit-product, device/xilinx/zcu102/device.mk)

# Include vendor binaries
$(call inherit-product-if-exists, vendor/xilinx/zcu102/device-vendor.mk)

# Specify product details
PRODUCT_NAME := zcu102
PRODUCT_DEVICE := zcu102
PRODUCT_BRAND := Android
PRODUCT_MODEL := AOSP on ZynqMP ZCU102
PRODUCT_MANUFACTURER := Xilinx

# Include gralloc for Mali GPU
PRODUCT_PACKAGES += \
	gralloc.zynqmp
