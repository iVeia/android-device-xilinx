LOCAL_PATH := $(call my-dir)

##########################################
include $(CLEAR_VARS)

LOCAL_SRC_FILES := \
	prog.cc \
	dsb.cc \
	util.cc

LOCAL_CPP_EXTENSION := cc

LOCAL_CPPFLAGS := \
	-W \
	-Wall \
	-Wextra \
	-Wunused \
	-Werror \
	-Wno-unused-parameter \
	-fexceptions 

LOCAL_MODULE := pdsb
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)
##################################################
