LOCAL_PATH:= $(call my-dir)

##### libmediactl ######
include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
    libmediactl.c

LOCAL_CFLAGS += -Wno-missing-field-initializers
LOCAL_CFLAGS += -Wno-sign-compare -Wunused-parameter

LOCAL_C_INCLUDES := \
     $(LOCAL_PATH)/../..


LOCAL_MODULE := libmediactl
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

##### libv4l2subdev ######
include $(CLEAR_VARS)
LOCAL_SRC_FILES := \
    libv4l2subdev.c

LOCAL_CFLAGS += -Wno-missing-field-initializers
LOCAL_CFLAGS += -Wno-sign-compare -Wunused-parameter

LOCAL_SHARED_LIBRARIES := \
    libmediactl

LOCAL_MODULE := libv4l2subdev
LOCAL_MODULE_TAGS := optional
include $(BUILD_SHARED_LIBRARY)

##### media-ctl ######
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
    media-ctl.c \
    options.c

LOCAL_CFLAGS += -Wno-missing-field-initializers
LOCAL_CFLAGS += -Wno-sign-compare -Wunused-parameter

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH)/../include \
    $(LOCAL_PATH)/../../include \
    $(LOCAL_PATH)/../..

LOCAL_SHARED_LIBRARIES := \
    libutils \
    libcutils \
    libmediactl \
    libv4l2subdev

LOCAL_MODULE := media-ctl
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
