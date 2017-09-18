LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := main.out
LOCAL_SRC_FILES := \
	main.cpp \
	config-manager.cpp
include $(BUILD_EXECUTABLE)

