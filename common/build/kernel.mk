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

ifeq ($(TARGET_KERNEL_BUILT_FROM_SOURCE),true)

ifeq ($(KERNEL_CFG_NAME),)
$(error cannot build kernel, config is not specified)
endif

KERNEL_EXTRA_FLAGS := CFLAGS_MODULE="-fno-pic" ANDROID_TOOLCHAIN_FLAGS="-mno-android -Werror"
KERNEL_CROSS_COMP := $(notdir $(TARGET_TOOLS_PREFIX))
KERNEL_OUT_DIR := $(TARGET_OUT_INTERMEDIATES)/KERNEL_OBJ
KERNEL_CONFIG := $(KERNEL_OUT_DIR)/.config
KERNEL_SAVE_DEFCONFIG := $(KERNEL_OUT_DIR)/defconfig
KERNEL_IMAGE := $(PRODUCT_OUT)/boot/Image
KERNEL_MODULES_OUT := $(PRODUCT_OUT)/system/vendor/modules

ifneq ($(KERNEL_DTS_NAMES),)
TARGET_DTB_FILES := $(addprefix $(KERNEL_OUT_DIR)/arch/$(TARGET_ARCH)/boot/dts/xilinx/, $(KERNEL_DTS_NAMES:.dts=.dtb) )
SOURCE_DTS_FILES := $(addprefix $(KERNEL_SRC_DIR)/arch/$(TARGET_ARCH)/boot/dts/xilinx/, $(KERNEL_DTS_NAMES))
endif

KERNEL_BLD_FLAGS := \
    ARCH=$(TARGET_ARCH) \
    $(KERNEL_EXTRA_FLAGS) \
    KCFLAGS=$(KERNEL_CFLAGS)

KERNEL_BLD_FLAGS :=$(KERNEL_BLD_FLAGS) \
     O=$(ANDROID_BUILD_TOP)/$(KERNEL_OUT_DIR)

KERNEL_BLD_ENV := CROSS_COMPILE=$(KERNEL_CROSS_COMP) \
    PATH=$(KERNEL_PATH):$(PATH)

KERNEL_DEFCONFIG ?= $(KERNEL_SRC_DIR)/arch/$(TARGET_ARCH)/configs/$(KERNEL_CFG_NAME)
KERNEL_VERSION_FILE := $(KERNEL_OUT_DIR)/include/config/kernel.release

define copy-modules
	$(info Copying kernel modules to $(KERNEL_MODULES_OUT))
	$(hide) mkdir -p $(KERNEL_MODULES_OUT)
	for module in $(KERNEL_MODULES); do \
		$(ACP) $(KERNEL_OUT_DIR)/$$module $(KERNEL_MODULES_OUT);\
	done
endef

$(KERNEL_OUT_DIR):
	mkdir -p $@

$(KERNEL_CONFIG): $(KERNEL_DEFCONFIG) | $(KERNEL_OUT_DIR)
	$(hide) echo Regenerating kernel config $(KERNEL_OUT_DIR)
	$(hide) mkdir -p $(KERNEL_OUT_DIR)
	$(hide) $(KERNEL_BLD_ENV) $(MAKE) -C $(KERNEL_SRC_DIR) $(KERNEL_BLD_FLAGS) $(notdir $(KERNEL_DEFCONFIG))

build_kernel: $(ACP) $(KERNEL_CONFIG) | $(KERNEL_OUT_DIR)
	$(KERNEL_BLD_ENV) $(MAKE) -C $(KERNEL_SRC_DIR) $(KERNEL_BLD_FLAGS) $(KERNEL_ENV)
	$(KERNEL_BLD_ENV) $(MAKE) -C $(KERNEL_SRC_DIR) $(KERNEL_BLD_FLAGS) $(KERNEL_ENV) modules
	$(call copy-modules)

build_dtbs: build_kernel $(ACP) $(KERNEL_CONFIG) $(SOURCE_DTS_FILES) | $(KERNEL_OUT_DIR)
	$(KERNEL_BLD_ENV) $(MAKE) -C $(KERNEL_SRC_DIR) $(KERNEL_BLD_FLAGS) $(KERNEL_ENV) dtbs
	for dtbfile in $(TARGET_DTB_FILES); do \
		dtbtarget=`basename $$dtbfile` \
		$(ACP) $$dtbfile $(PRODUCT_OUT)/boot/$$dtbtarget; \
	done

$(KERNEL_OUT_DIR)/arch/$(TARGET_ARCH)/boot/Image: build_kernel build_dtbs

$(KERNEL_IMAGE): $(KERNEL_OUT_DIR)/arch/$(TARGET_ARCH)/boot/Image | $(ACP)
	$(ACP) $(KERNEL_OUT_DIR)/arch/$(TARGET_ARCH)/boot/Image $@

clean_kernel:
	$(hide) $(KERNEL_BLD_ENV) $(MAKE) -C $(KERNEL_SRC_DIR) $(KERNEL_BLD_FLAGS) clean

menuconfig xconfig gconfig: $(KERNEL_CONFIG)
	$(hide) $(KERNEL_BLD_ENV) $(MAKE) -C $(KERNEL_SRC_DIR) $(KERNEL_BLD_FLAGS) $@
	$(hide) $(KERNEL_BLD_ENV) $(MAKE) -C $(KERNEL_SRC_DIR) $(KERNEL_BLD_FLAGS) savedefconfig
	$(hide) cp -f $(KERNEL_SAVE_DEFCONFIG) $(KERNEL_DEFCONFIG)
	$(hide) echo ===========
	$(hide) echo $(KERNEL_DEFCONFIG) has been modified !
	$(hide) echo ===========

.PHONY: menuconfig xconfig gconfig
.PHONY: build_kernel clean_kernel
.PHONY: $(KERNEL_IMAGE)
droidcore: $(KERNEL_IMAGE)
endif #TARGET_KERNEL_BUILT_FROM_SOURCE
