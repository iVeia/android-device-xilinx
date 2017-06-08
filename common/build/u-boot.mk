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

ifeq ($(TARGET_UBOOT_BUILT_FROM_SOURCE),true)

ifeq ($(UBOOT_CFG_NAME),)
$(error cannot build u-boot, config is not specified)
endif

UBOOT_CROSS_COMP := $(notdir $(TARGET_TOOLS_PREFIX))
UBOOT_OUT_DIR := $(TARGET_OUT_INTERMEDIATES)/UBOOT_OBJ
UBOOT_CONFIG := $(UBOOT_OUT_DIR)/.config
UBOOT_IMAGE := $(PRODUCT_OUT)/boot/u-boot.elf

UBOOT_BLD_FLAGS :=$(UBOOT_BLD_FLAGS) \
      O=$(ANDROID_BUILD_TOP)/$(UBOOT_OUT_DIR)

UBOOT_BLD_ENV := CROSS_COMPILE=$(UBOOT_CROSS_COMP)

UBOOT_DEFCONFIG ?= $(UBOOT_SRC_DIR)/configs/$(UBOOT_CFG_NAME)

$(UBOOT_OUT_DIR):
	mkdir -p $@

$(UBOOT_CONFIG): $(UBOOT_DEFCONFIG) | $(UBOOT_OUT_DIR)
	$(hide) echo Regenerating uboot config $(UBOOT_OUT_DIR)
	$(hide) mkdir -p $(UBOOT_OUT_DIR)
	$(hide) $(UBOOT_BLD_ENV) $(MAKE) -C $(UBOOT_SRC_DIR) $(UBOOT_BLD_FLAGS) $(UBOOT_CFG_NAME)

build_uboot: $(ACP) $(UBOOT_CONFIG) | $(UBOOT_OUT_DIR)
	$(UBOOT_BLD_ENV) $(MAKE) -C $(UBOOT_SRC_DIR) $(UBOOT_BLD_FLAGS) $(UBOOT_ENV)

$(UBOOT_OUT_DIR)/u-boot.elf: build_uboot

$(UBOOT_IMAGE): $(UBOOT_OUT_DIR)/u-boot.elf | $(ACP)
	$(hide) mkdir -p $(PRODUCT_OUT)/boot
	$(ACP) $(UBOOT_OUT_DIR)/u-boot.elf $@

clean_uboot:
	$(hide) $(UBOOT_BLD_ENV) $(MAKE) -C $(UBOOT_SRC_DIR) $(UBOOT_BLD_FLAGS) clean

.PHONY: build_uboot clean_uboot
.PHONY: $(UBOOT_IMAGE)
droidcore: $(UBOOT_IMAGE)

URAMDISK_TARGET := $(PRODUCT_OUT)/boot/uramdisk.img

$(URAMDISK_TARGET): $(PRODUCT_OUT)/ramdisk.img build_uboot
	mkdir -p $(dir $@)
	$(UBOOT_OUT_DIR)/tools/mkimage -A arm64 -O linux -T ramdisk -n "RAM Disk" -d $< $@

build_uramdisk: $(URAMDISK_TARGET)
.PHONY: build_uramdisk
droidcore: $(URAMDISK_TARGET)

endif #TARGET_UBOOT_BUILT_FROM_SOURCE
