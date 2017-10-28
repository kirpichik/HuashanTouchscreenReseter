LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
LOCAL_MODULE := input-event-test.out
LOCAL_SRC_FILES := \
	input-event-test.c
include $(BUILD_EXECUTABLE)

