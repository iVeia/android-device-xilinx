LOCAL_PATH := $(call my-dir)

##########################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	server.cc \
	src/message.cc \
	src/ota_manager.cc \
	src/socket_interface.cc \
	src/uboot.cc \
	src/support.cc \
	src/debug.cc \
	src/config.cc \

LOCAL_CPP_EXTENSION := cc

LOCAL_CPPFLAGS := \
	-W \
	-Wall \
	-Wextra \
	-Wunused \
	-Werror \
	-Wno-unused-parameter \
	-fexceptions \
	-I $(LOCAL_PATH)/src \

LOCAL_INIT_RC := iVeiOTA.rc
LOCAL_MODULE := iVeiOTA
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

#########################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	echo_server.cc \
	src/socket_echo_interface.cc \
	src/debug.cc \

LOCAL_CPP_EXTENSION := cc

LOCAL_CPPFLAGS := \
	-W \
	-Wall \
	-Wextra \
	-Wunused \
	-Werror \
	-Wno-unused-parameter \
	-fexceptions \
	-I $(LOCAL_PATH)/src \

LOCAL_MODULE := iecho
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

#############################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	client.cc \
	src/message.cc \
	src/ota_manager.cc \
	src/socket_interface.cc \
	src/uboot.cc \
	src/support.cc \
	src/debug.cc \
	src/config.cc \

LOCAL_CPP_EXTENSION := cc

LOCAL_CPPFLAGS := \
	-W \
	-Wall \
	-Wextra \
	-Wunused \
	-Werror \
	-Wno-unused-parameter \
	-fexceptions \
	-I $(LOCAL_PATH)/src \

LOCAL_MODULE := ciVeiOTA
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

##################################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	echo_client.cc \
	src/socket_echo_interface.cc \
	src/debug.cc \

LOCAL_CPP_EXTENSION := cc

LOCAL_CPPFLAGS := \
	-W \
	-Wall \
	-Wextra \
	-Wunused \
	-Werror \
	-Wno-unused-parameter \
	-fexceptions \
	-I $(LOCAL_PATH)/src \

LOCAL_MODULE := ciecho
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

## ##############copy config file##################
## include $(CLEAR_VARS)
## LOCAL_MODULE := iVeiOTA.conf
## LOCAL_MODULE_TAGS := optional
## LOCAL_MODULE_CLASS := ETC
## LOCAL_MODULE_PATH := $(TARGET_OUT)/etc/
## LOCAL_SRC_FILES := config/iVeiOTA.conf
## include $(BUILD_PREBUILT)
## 
## ##############copy config file##################
## include $(CLEAR_VARS)
## LOCAL_MODULE := fstab.zcu102.a
## LOCAL_MODULE_TAGS := optional
## LOCAL_MODULE_CLASS := ETC
## LOCAL_MODULE_PATH := $(ANDROID_PRODUCT_OUT)/root/
## LOCAL_SRC_FILES := config/fstab.zcu102.a
## include $(BUILD_PREBUILT)
## 
## ##############copy config file##################
## include $(CLEAR_VARS)
## LOCAL_MODULE := fstab.zcu102.b
## LOCAL_MODULE_TAGS := optional
## LOCAL_MODULE_CLASS := ETC
## LOCAL_MODULE_PATH := $(ANDROID_PRODUCT_OUT)/root
## LOCAL_SRC_FILES := config/fstab.zcu102.b
## include $(BUILD_PREBUILT)
## 
## ##############copy config file##################
## include $(CLEAR_VARS)
## LOCAL_MODULE := sample_manifest
## LOCAL_MODULE_TAGS := optional
## LOCAL_MODULE_CLASS := ETC
## LOCAL_MODULE_PATH := $(ANDROID_PRODUCT_OUT)/root/ota_tmp/
## LOCAL_SRC_FILES := test/sample_manifest
## include $(BUILD_PREBUILT)
## 
## ##############copy config file##################
## include $(CLEAR_VARS)
## LOCAL_MODULE := uEnv.txt
## LOCAL_MODULE_TAGS := optional
## LOCAL_MODULE_CLASS := ETC
## LOCAL_MODULE_PATH := $(ANDROID_PRODUCT_OUT)/boot/
## LOCAL_SRC_FILES := config/uEnv.txt
## include $(BUILD_PREBUILT)
## 
## ##############copy config file##################
## include $(CLEAR_VARS)
## LOCAL_MODULE := AB.img
## LOCAL_MODULE_TAGS := optional
## LOCAL_MODULE_CLASS := ETC
## LOCAL_MODULE_PATH := $(ANDROID_PRODUCT_OUT)/boot/
## LOCAL_SRC_FILES := config/AB.img
## include $(BUILD_PREBUILT)
## 
## ##############copy config file##################
## include $(CLEAR_VARS)
## LOCAL_MODULE := uEnvAB.txt
## LOCAL_MODULE_TAGS := optional
## LOCAL_MODULE_CLASS := ETC
## LOCAL_MODULE_PATH := $(ANDROID_PRODUCT_OUT)/bi/
## LOCAL_SRC_FILES := config/uEnvAB.txt
## include $(BUILD_PREBUILT)
## 
