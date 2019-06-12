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

# Common device configuration.

# Inherit 64-bit device configuration
$(call inherit-product, $(SRC_TARGET_DIR)/product/core_64_bit.mk)

# Adjust the dalvik heap to be appropriate for a tablet.
$(call inherit-product, frameworks/native/build/tablet-10in-xhdpi-2048-dalvik-heap.mk)

# Build and run only ART
OVERRIDE_RUNTIMES := runtime_libart_default

# Set custom settings for the framework
DEVICE_PACKAGE_OVERLAYS += \
    device/xilinx/zcu102/overlay

# Copy supported hardware features config file(s)
PRODUCT_COPY_FILES +=  \
    frameworks/native/data/etc/android.hardware.screen.landscape.xml:system/etc/permissions/android.hardware.screen.landscape.xml \
    frameworks/native/data/etc/android.hardware.ethernet.xml:system/etc/permissions/android.hardware.ethernet.xml \
    frameworks/native/data/etc/android.software.app_widgets.xml:system/etc/permissions/android.software.app_widgets.xml \
    frameworks/native/data/etc/android.software.backup.xml:system/etc/permissions/android.software.backup.xml \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
    device/xilinx/zcu102/atmel-maxtouch.idc:system/usr/idc/atmel-maxtouch.idc \
    device/xilinx/zcu102/wpa_supplicant.conf:system/etc/wifi/wpa_supplicant.conf \
    device/xilinx/zcu102/wpa_supplicant_overlay.conf:system/etc/wifi/wpa_supplicant_overlay.conf \
    device/xilinx/zcu102/scripts/app_update.sh:system/bin/app_update.sh \

# Add sshd
PRODUCT_PACKAGES += ssh sftp scp sshd ssh-keygen sshd_config start-ssh
PRODUCT_COPY_FILES +=  \
    device/xilinx/zcu102/ssh/authorized_keys:system/etc/ssh/authorized_keys \
    device/xilinx/zcu102/ssh/sshd_config:system/etc/ssh/sshd_config \
    device/xilinx/zcu102/ssh/ssh_host_dsa_key:system/etc/ssh/ssh_host_dsa_key \
    device/xilinx/zcu102/ssh/ssh_host_dsa_key.pub:system/etc/ssh/ssh_host_dsa_key.pub \
    device/xilinx/zcu102/ssh/ssh_host_rsa_key:system/etc/ssh/ssh_host_rsa_key \
    device/xilinx/zcu102/ssh/ssh_host_rsa_key.pub:system/etc/ssh/ssh_host_rsa_key.pub \
    device/xilinx/zcu102/ssh/empty:root/var/run/ssh/empty/empty \

# OTA stuff
PRODUCT_PACKAGES += iVeiOTA ciVeiOTA iecho ciecho
PRODUCT_COPY_FILES += \
	device/xilinx/zcu102/iVeiOTA/config/iVeiOTA.conf:system/etc/iVeiOTA.conf \
	device/xilinx/zcu102/iVeiOTA/config/fstab.zcu102.a:root/fstab.zcu102.a \
	device/xilinx/zcu102/iVeiOTA/config/fstab.zcu102.b:root/fstab.zcu102.b \
	device/xilinx/zcu102/iVeiOTA/test/sample_manifest:root/ota_tmp/sample_manifest \
	device/xilinx/zcu102/iVeiOTA/config/uEnv.txt:boot/uEnv.txt \
	device/xilinx/zcu102/iVeiOTA/config/AB.img:boot/AB.img \
	device/xilinx/zcu102/iVeiOTA/config/uEnvAB.txt:bi/uEnvAB.txt \
	device/xilinx/zcu102/iVeiOTA/config/ident:system/iVeiOTA_ident \
	device/xilinx/zcu102/iVeiOTA/config/ident:root/iVeiOTA_ident


# Add tinyalsa
PRODUCT_PACKAGES += \
    tinyplay \
    tinycap \
    tinymix \
    tinypcminfo \
    cplay
PRODUCT_COPY_FILES +=  \
    device/xilinx/zcu102/audio/wav/piano2.wav:system/media/audio/wav/piano2.wav \

# Add libion for graphics
PRODUCT_PACKAGES += \
	libion \
	libdrm \
	modetest 

# Add wifi-related packages
PRODUCT_PACKAGES += \
    libwpa_client \
    hostapd \
    wpa_supplicant \
    wpa_supplicant.conf

PRODUCT_COPY_FILES +=  \
    device/xilinx/zcu102/brcm/4343w.hcd:system/etc/firmware/brcm/4343w.hcd \
    device/xilinx/zcu102/brcm/brcmfmac43430-sdio.bin:system/etc/firmware/brcm/brcmfmac43430-sdio.bin \
    device/xilinx/zcu102/brcm/brcmfmac43430-sdio.txt:system/etc/firmware/brcm/brcmfmac43430-sdio.txt \
    device/xilinx/zcu102/brcm/bcm4343w/4343w.hcd:system/etc/firmware/brcm/bcm4343w/4343w.hcd \
    device/xilinx/zcu102/brcm/bcm4343w/brcmfmac43430-sdio.bin:system/etc/firmware/brcm/bcm4343w/brcmfmac43430-sdio.bin \
    device/xilinx/zcu102/brcm/bcm4343w/brcmfmac43430-sdio.txt:system/etc/firmware/brcm/bcm4343w/brcmfmac43430-sdio.txt \
    device/xilinx/zcu102/brcm/bcm4343w/brcmfmac43430-sdio-fcc.txt:system/etc/firmware/brcm/bcm4343w/brcmfmac43430-sdio-fcc.txt \
    device/xilinx/zcu102/brcm/bcm4343w/brcmfmac43430-sdio-prod.bin:system/etc/firmware/brcm/bcm4343w/brcmfmac43430-sdio-prod.bin \

BRCM_MODULES_DIR=$(ANDROID_PRODUCT_OUT)/obj/KERNEL_OBJ/drivers/net/wireless/broadcom/brcm80211
PRODUCT_COPY_FILES += \
    $(BRCM_MODULES_DIR)/brcmfmac/brcmfmac.ko:system/lib/modules/brcmfmac.ko \
    $(BRCM_MODULES_DIR)/brcmutil/brcmutil.ko:system/lib/modules/brcmutil.ko

# eMMC install script
PRODUCT_COPY_FILES += \
    device/xilinx/zcu102/scripts/release:release \
    device/xilinx/common/scripts/mksdcard.sh:mksdcard.sh

# Include libs for SW graphics
PRODUCT_PACKAGES += libGLES_android

# Graphics HAL
PRODUCT_PACKAGES += \
    android.hardware.graphics.allocator@2.0-impl \
    android.hardware.graphics.mapper@2.0-impl \

PRODUCT_PACKAGES += \
    android.hardware.audio@2.0-impl \
	android.hardware.audio.effect@2.0-impl \

# Keymaster HAL
PRODUCT_PACKAGES += \
    android.hardware.keymaster@3.0-impl

