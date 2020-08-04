LOCAL_PATH := $(call my-dir)

##########################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	server.cc \
	src/debug.cc \
	src/message.cc \
	src/socket_interface.cc \
	src/support.cc \
	src/camera.cc

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

LOCAL_MODULE := iv_v4_hal
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

#############################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	client.cc \
	src/debug.cc \
	src/message.cc \
	src/socket_interface.cc \
	src/camera.cc

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

LOCAL_MODULE := iv_v4_hal_client
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

##################################################
