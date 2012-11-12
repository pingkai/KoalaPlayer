/*
 * Copyright (c) 2012 pingkai010@gmail.com
 *
 *
 */


#ifdef ANDROID
#define LOG_TAG "koala_demuxer"
#include <utils/Log.h>
#define printf ALOGE
#endif
#include <limits.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libavutil/time.h>
#include <libavutil/intreadwrite.h>

#include <libavformat/avformat.h>
#include "../include/koala_type.h"

typedef struct koala_decoder_handle_t{
	AVCodecContext *ac;
	AVCodec *audio_codec;
	uint8_t *audioOutBuf;
	AVPacket * pkt;

	decoder_buf_callback audio_cb;
	void * CbpHandle;
}koala_decoder_handle;

#define MAX_AUDIO_FRAME_SIZE 192000 

int reg_audio_decoder_cb(koala_decoder_handle *pHandle,decoder_buf_callback cb,void *CbpHandle){
	if (!pHandle || !cb){
		printf("%s:%d error\n",__FILE__,__LINE__);
		return -1;
	}
	pHandle->audio_cb = cb;
	pHandle->CbpHandle = CbpHandle;
	return 0;

}

int koala_ffmpeg_decode_audio_pkt(koala_decoder_handle* pHandle,uint8_t *data,int size){
	int out_size, len;
	int sendpts = 1;
	int err;
	if (!pHandle ||!pHandle->audioOutBuf){
		printf("%s:%d error\n",__FILE__,__LINE__);
		return -1;
	}
	pHandle->pkt->data = data;
	pHandle->pkt->size = size;
	while (pHandle->pkt->size > 0) {
		out_size = MAX_AUDIO_FRAME_SIZE;
		len = avcodec_decode_audio3(pHandle->ac, pHandle->audioOutBuf, &out_size, pHandle->pkt);
		if (len < 0) {
			printf("Error while decoding\n");
			break;
		}
		pHandle->pkt->size -= len;
		pHandle->pkt->data += len;
		if (pHandle->audio_cb){
			if (sendpts){
				err = pHandle->audio_cb((unsigned char *)pHandle->audioOutBuf, out_size,pHandle->pkt->pts,pHandle->CbpHandle);
				sendpts = 0;
			}else
				err = pHandle->audio_cb((unsigned char *)pHandle->audioOutBuf, out_size,-1,pHandle->CbpHandle);
			if (err < 0){
				printf("%s:%d error\n",__FILE__,__LINE__);
				break;
			}
		}
	}
	pHandle->pkt->data = data;
	av_free_packet(pHandle->pkt);
	return 0;
}

koala_decoder_handle* init_decoder_audio(void *codec_cont){
	koala_decoder_handle *pHandle = (koala_decoder_handle *)malloc(sizeof(koala_decoder_handle));
    AVCodecContext *codec = (AVCodecContext *)codec_cont;
    int ret;
    enum AVCodecID id = AV_CODEC_ID_NONE;
	if (pHandle == NULL)
		return NULL;
	memset(pHandle,0,sizeof(koala_decoder_handle));
	av_register_all();
  
	pHandle->audio_codec = avcodec_find_decoder(codec->codec_id);

	if (pHandle->audio_codec == NULL){
		free(pHandle);
		return NULL;
	}
	
	pHandle->ac = avcodec_alloc_context3((const AVCodec *)pHandle->audio_codec);
	if (pHandle->ac == NULL){
		free(pHandle);
		return NULL;
	}
    ret = avcodec_copy_context(pHandle->ac,codec);
    if (ret < 0)
        printf("avcodec_copy_context error\n");
	if (avcodec_open(pHandle->ac, pHandle->audio_codec) < 0) {
        printf("could not open codec\n");
    	av_free(pHandle->ac);
		free(pHandle);
 		return NULL;
    }
	pHandle->audioOutBuf  = av_malloc(MAX_AUDIO_FRAME_SIZE);
	if (pHandle->audioOutBuf == NULL){
		avcodec_close(pHandle->ac);
		av_free(pHandle->ac);
		free(pHandle);
 		return NULL;
	}
	pHandle->pkt = av_malloc(sizeof(AVPacket));
	if (pHandle->pkt == NULL){
		close_decoder_audio(pHandle);
		return NULL;
	}
    av_init_packet(pHandle->pkt);
	return pHandle;
}

void close_decoder_audio(koala_decoder_handle* pHandle){
	if (!pHandle){
		printf("%s:%d error\n",__FILE__,__LINE__);
		return ;
	}
	
	if (pHandle->ac != NULL){
		avcodec_close(pHandle->ac);
    	av_free(pHandle->ac);
		pHandle->ac = NULL;
	}
	
	pHandle->audio_codec = NULL;
	if (pHandle->audioOutBuf)
		av_free(pHandle->audioOutBuf);
	if (pHandle->pkt){
		av_free(pHandle->pkt);
	}
	
	free(pHandle);
	return ;
	
}


