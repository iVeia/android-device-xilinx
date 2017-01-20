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
    device/xilinx/zcu102/fstab.common:root/fstab.zcu102 \
    $(LOCAL_PATH)/init.ozzy.rc:root/init.zcu102.rc \
    device/xilinx/zcu102/init.common.usb.rc:root/init.zcu102.usb.rc \
    device/xilinx/zcu102/ueventd.common.rc:root/ueventd.zcu102.rc

# Copy WiFi config for framework
PRODUCT_COPY_FILES +=  \
    frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml

# Copy bootloader envs
PRODUCT_COPY_FILES += \
   $(LOCAL_PATH)/uEnv.txt:uEnv.txt

# Copy touchscreen config
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/maxtouch.cfg:/system/etc/firmware/maxtouch.cfg

PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/init_touch.sh:/system/bin/init_touch.sh

KERNEL_MODULES += \
	drivers/input/touchscreen/atmel_mxt_ts.ko \
	drivers/net/wireless/ti/wl12xx/wl12xx.ko

PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant.conf \
	$(LOCAL_PATH)/wl1271-nvs.bin:/system/etc/firmware/ti-connectivity/wl1271-nvs.bin \
	$(LOCAL_PATH)/wl127x-fw-5-sr.bin:/system/etc/firmware/ti-connectivity/wl127x-fw-5-sr.bin \
	$(LOCAL_PATH)/wl127x-fw-5-mr.bin:/system/etc/firmware/ti-connectivity/wl127x-fw-5-mr.bin \
	$(LOCAL_PATH)/wl127x-fw-5-plt.bin:/system/etc/firmware/ti-connectivity/wl127x-fw-5-plt.bin

PRODUCT_PACKAGES += \
	wpa_supplicant \
	hostapd \
	libwpa_client \
	wificond \
	wifilogd

PRODUCT_PROPERTY_OVERRIDES += \
	wifi.interface=wlan0 \
	wifi.supplicant_scan_interval=15

# Copy prebuilt BOOT.BIN if it exists
PRODUCT_COPY_FILES += $(call add-to-product-copy-files-if-exists,\
	$(LOCAL_PATH)/BOOT.BIN:BOOT.BIN)

PRODUCT_COPY_FILES += $(call add-to-product-copy-files-if-exists,\
	$(LOCAL_PATH)/BOOT0001.BIN:BOOT0001.BIN)
