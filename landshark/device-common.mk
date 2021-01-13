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
    device/xilinx/landshark/overlay

# Copy supported hardware features config file(s)
PRODUCT_COPY_FILES +=  \
    frameworks/native/data/etc/android.hardware.screen.landscape.xml:system/etc/permissions/android.hardware.screen.landscape.xml \
    frameworks/native/data/etc/android.hardware.ethernet.xml:system/etc/permissions/android.hardware.ethernet.xml \
    frameworks/native/data/etc/android.software.app_widgets.xml:system/etc/permissions/android.software.app_widgets.xml \
    frameworks/native/data/etc/android.software.backup.xml:system/etc/permissions/android.software.backup.xml \
    frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
    frameworks/native/data/etc/android.hardware.usb.host.xml:system/etc/permissions/android.hardware.usb.host.xml \
    frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \

# We also need video 4 linux
PRODUCT_PACKAGES += libv4l_convert
PRODUCT_PACKAGES += libv4l2 v4l2-ctl libv4l2subdev libmediactl media-ctl 

# Add sshd
#PRODUCT_PACKAGES += ssh sftp scp sshd ssh-keygen sshd_config start-ssh
#PRODUCT_COPY_FILES +=  \
#    device/xilinx/landshark/ssh/sshd_config:system/etc/ssh/sshd_config \
#    device/xilinx/landshark/ssh/empty:root/var/run/ssh/empty/empty \

PRODUCT_PACKAGES += iv_v4_hal iv_v4_hal_client
PRODUCT_COPY_FILES += \
	device/xilinx/landshark/iv4hal/config/basler_14MP.playback:system/etc/basler_14MP.playback \
	device/xilinx/landshark/iv4hal/config/basler_5MP.playback:system/etc/basler_5MP.playback

PRODUCT_PACKAGES += pdsb


## OTA stuff -- disable until we get more stuff working
#PRODUCT_PACKAGES += iVeiOTA ciVeiOTA iecho ciecho
#PRODUCT_COPY_FILES += \
#	device/xilinx/landshark/iVeiOTA/config/iVeiOTA.conf:system/etc/iVeiOTA.conf \
#	device/xilinx/landshark/iVeiOTA/config/iVeiOTA-noota.conf:system/etc/iVeiOTA-noota.conf \
#	device/xilinx/landshark/iVeiOTA/config/fstab.landshark.a:root/fstab.landshark.a \
#	device/xilinx/landshark/iVeiOTA/config/fstab.landshark.b:root/fstab.landshark.b \
#	device/xilinx/landshark/iVeiOTA/config/uEnv.txt:boot/uEnv.txt \
#	device/xilinx/landshark/iVeiOTA/config/AB.img:boot/AB.img \
#	device/xilinx/landshark/iVeiOTA/config/uEnvAB.txt:bi/uEnvAB.txt

# Add i2c tools in there
PRODUCT_PACKAGES += i2c-tools i2cget i2cdetect i2cset i2cdump dhcpcd-6.8.2


# Add tinyalsa
PRODUCT_PACKAGES += \
    tinyplay \
    tinycap \
    tinymix \
    tinypcminfo \
    cplay
PRODUCT_COPY_FILES +=  \
    device/xilinx/landshark/audio/wav/piano2.wav:system/media/audio/wav/piano2.wav \

# Add libion for graphics
PRODUCT_PACKAGES += \
	libion \
	libdrm \
	modetest 

# eMMC install script
PRODUCT_COPY_FILES += \
    device/xilinx/landshark/scripts/release:release \
    device/xilinx/common/scripts/mksdcard.sh:mksdcard.sh \
    device/xilinx/common/scripts/mksdcard-noota.sh:mksdcard-noota.sh

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

