/*
 * Copyright (c) 2012 pingkai010@gmail.com
 *
 *
 */


#ifdef ANDROID
#define LOG_TAG "koala_decoder_audio"
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

int reg_audio_decoder_cb(koala_decoder_handle *pHandle,decoder_buf_callback cb,void *CbpHandle){
	if (!pHandle || !cb){
		printf("%s:%d error\n",__FILE__,__LINE__);
		return -1;
	}
	pHandle->audio_cb = cb;
	pHandle->CbpHandle = CbpHandle;
	return 0;

}

int koala_ffmpeg_decode_audio_pkt(koala_decoder_handle* pHandle,uint8_t *data,int size,long long pts){
	int out_size, len;
	int sendpts = 1;
	int err;
	if (!pHandle ||!pHandle->audioOutBuf){
		printf("%s:%d error\n",__FILE__,__LINE__);
		return -1;
	}
	pHandle->pkt->data = data;
	pHandle->pkt->size = size;
    pHandle->pkt->pts  = pts;
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
	return err;
}

int get_audio_info(void *codec_cont,audio_info *info){
    AVCodecContext *codec = (AVCodecContext *)codec_cont;
    if (codec_cont == NULL)
        return -1;
    
    if (info){
        info->nChannles   = codec->channels;
        info->sample_rate = codec->sample_rate;
        info->sample_fmt  = codec->sample_fmt;
    }
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
	if (avcodec_open2(pHandle->ac, pHandle->audio_codec,NULL) < 0) {
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

///////////////////////////////////////////////////////////
ReSampleContext * koala_resample_audio_init(int input_rate,int input_channels,int sample_fmt_in,
                                   int output_rate,int output_channels,int  sample_fmt_out){
    
    ReSampleContext *presampleContext = av_audio_resample_init(output_channels,
                           input_channels,
                           output_rate,
                           input_rate,
                           sample_fmt_out,
                           sample_fmt_in,
                           16,10,0,0.8f);
    return presampleContext;
}

int koala_resample_audio(ReSampleContext * s,short * input,short * output,int nb_samples){
    int ret;
    ret = audio_resample(s,output,input,nb_samples);
    return ret;
   
}

int koala_resample_auido_close(ReSampleContext * s){
     audio_resample_close(s);
     return 0;
}

