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

# Inherit device specific configurations
$(call inherit-product, device/xilinx/zcu102/device-common.mk)
$(call inherit-product, device/xilinx/zcu102/zcu102/device-zcu102.mk)

# Inherit full base product
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)

# Include vendor binaries
$(call inherit-product-if-exists, vendor/xilinx/zynqmp/device-vendor.mk)

# Specify product details
PRODUCT_NAME := zcu102
PRODUCT_DEVICE := zcu102
PRODUCT_BRAND := Android
PRODUCT_MODEL := AOSP on ZynqMP ZCU102
PRODUCT_MANUFACTURER := Xilinx

# Specify variables for kernel build
KERNEL_SRC_DIR ?= $(ANDROID_BUILD_TOP)/linux-iveia
KERNEL_CFG_NAME ?= iveia_atlas_ii_z8_defconfig
KERNEL_DTS_NAMES ?= \
	iveia-atlas-ii-z8.dts

# Check for availability of kernel source
ifneq ($(wildcard $(KERNEL_SRC_DIR)/Makefile),)
	# Give precedence to TARGET_PREBUILT_KERNEL
	ifeq ($(TARGET_PREBUILT_KERNEL),)
		TARGET_KERNEL_BUILT_FROM_SOURCE := true
	endif
endif

# Include gralloc for Mali GPU
PRODUCT_PACKAGES += \
	gralloc.zynqmp

UBOOT_SRC_DIR ?= bootable/u-boot-xlnx
UBOOT_CFG_NAME ?= xilinx_zynqmp_zcu102_rev1_0_defconfig
TARGET_UBOOT_BUILT_FROM_SOURCE := false
