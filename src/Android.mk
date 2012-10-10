LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_STATIC_LIBRARIES :=  \
                libavformat \
                libavcodec  \
                libavutil
LOCAL_SRC_FILES:=                 \
		koala_demuxer.c

LOCAL_C_INCLUDES:=

LOCAL_SHARED_LIBRARIES := libz 
#LOCAL_LDLIBS += libz libc
LOCAL_MODULE:= libkoala
LOCAL_MODULE_TAGS := optional
LOCAL_COPY_HEADERS_TO := libkoala
LOCAL_COPY_HEADERS := \
		../include/koala_demuxer.h
ifeq ($(TARGET_ARCH),arm)
    LOCAL_CFLAGS += -Wno-psabi
endif

include $(BUILD_SHARED_LIBRARY)
