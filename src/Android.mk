LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_STATIC_LIBRARIES :=  \
                libavformat \
                libavcodec  \
                libavutil
LOCAL_SRC_FILES:=                 \
		koala_demuxer.c    \
		koala_decoder_audio.c\
		koala_decoder_video.c

LOCAL_C_INCLUDES:=

LOCAL_SHARED_LIBRARIES := libz liblog 
#LOCAL_LDLIBS += libz libc
LOCAL_MODULE:= libkoala
LOCAL_MODULE_TAGS := optional
LOCAL_COPY_HEADERS_TO := libkoala
LOCAL_COPY_HEADERS := \
		../include/koala_demuxer.h \
		../include/koala_type.h   \
		../include/koala_decoder_audio.h
ifeq ($(TARGET_ARCH),arm)
    LOCAL_CFLAGS += -Wno-psabi
endif

include $(BUILD_SHARED_LIBRARY)
