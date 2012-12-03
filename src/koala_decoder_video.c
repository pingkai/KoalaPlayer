/*
 * Copyright (c) 2012 pingkai010@gmail.com
 *
 *
 */
#ifdef ANDROID
#define LOG_TAG "koala_decoder_video"
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

typedef struct koala_decoder_handle_v_t{
	AVCodecContext *vc;
	AVCodec *video_codec;
	uint8_t *audioOutBuf;
	AVPacket * pkt;
    AVFrame *picture;

	decoder_buf_callback_video video_cb;
	void * CbpHandle;
}koala_decoder_handle_v;

void close_decoder_video(koala_decoder_handle_v* pHandle);

int reg_video_decoder_cb(koala_decoder_handle_v *pHandle,decoder_buf_callback_video cb,void *CbpHandle){
	if (!pHandle || !cb){
		printf("%s:%d error\n",__FILE__,__LINE__);
		return -1;
	}
	pHandle->video_cb = cb;
	pHandle->CbpHandle = CbpHandle;
	return 0;

}

int get_video_info(koala_decoder_handle_v* pHandle,video_info *info){
    
    if (info){
        info->pix_fmt = pHandle->vc->pix_fmt;
        info->width   = pHandle->vc->width;
        info->height  = pHandle->vc->height;
    }
    return 0;
}

koala_decoder_handle_v* init_decoder_video(void *codec_cont){
    koala_decoder_handle_v *pHandle = (koala_decoder_handle_v *)malloc(sizeof(koala_decoder_handle_v));
    AVCodecContext *codec = (AVCodecContext *)codec_cont;
    int ret;
	memset(pHandle,0,sizeof(koala_decoder_handle_v));
	av_register_all();
    pHandle->video_codec = avcodec_find_decoder(codec->codec_id);

	if (pHandle->video_codec == NULL){
		free(pHandle);
		return NULL;
	}
	
	pHandle->vc = avcodec_alloc_context3((const AVCodec *)pHandle->video_codec);
	if (pHandle->vc == NULL){
		free(pHandle);
		return NULL;
	}
    ret = avcodec_copy_context(pHandle->vc,codec);
    if (ret < 0)
        printf("avcodec_copy_context error\n");
	if (avcodec_open2(pHandle->vc, pHandle->video_codec,NULL) < 0) {
        printf("could not open codec\n");
    	av_free(pHandle->vc);
		free(pHandle);
 		return NULL;
    }
	pHandle->pkt = av_malloc(sizeof(AVPacket));
	if (pHandle->pkt == NULL){
		close_decoder_video(pHandle);
		return NULL;
	}
    av_init_packet(pHandle->pkt);
    pHandle->picture= avcodec_alloc_frame();
	return pHandle;


}

int koala_ffmpeg_decode_video_pkt(koala_decoder_handle_v* pHandle,uint8_t *data,int size,long long pts){
    int len,got_picture;
    static int frame = 0;
    pHandle->pkt->size = size;
    pHandle->pkt->data = data;
    pHandle->pkt->pts = pts;
    while (pHandle->pkt->size > 0) {
        len = avcodec_decode_video2(pHandle->vc, pHandle->picture, &got_picture, pHandle->pkt);
        if (len < 0) {
            fprintf(stderr, "Error while decoding frame %d\n", frame);
            return -1;
        }
        if (got_picture) {
         //   printf("saving frame %3d\n", frame);
        //    fflush(stdout);
            if (pHandle->video_cb){
                pHandle->video_cb(pHandle->picture->data,pHandle->picture->linesize,pHandle->picture->pkt_pts,pHandle->CbpHandle);
            }else{
                printf("pHandle->vc->pix_fmt is %d\n",pHandle->vc->pix_fmt);
                printf("pHandle->picture->linesize[0] is %d\n",pHandle->picture->linesize[0]);
                printf("pHandle->picture->linesize[1] is %d\n",pHandle->picture->linesize[1]);
                printf("pHandle->picture->linesize[2] is %d\n",pHandle->picture->linesize[2]);
                printf("pHandle->picture->linesize[3] is %d\n",pHandle->picture->linesize[3]);
                printf("width is %d height is %d\n",pHandle->vc->width,pHandle->vc->height);
            }
            /* the picture is allocated by the decoder. no need to
               free it */
       //    snprintf(buf, sizeof(buf), outfilename, frame);
        //    pgm_save(pHandle->picture->data[0], pHandle->picture->linesize[0],
       //              pHandle->vc->width, pHandle->vc->height, buf);
            frame++;
        }
        pHandle->pkt->size -= len;
        pHandle->pkt->data += len;
    }
    av_free_packet(pHandle->pkt);
    return 0;
}

void close_decoder_video(koala_decoder_handle_v* pHandle){
	if (!pHandle){
		printf("%s:%d error\n",__FILE__,__LINE__);
		return ;
	}
	
	if (pHandle->vc != NULL){
		avcodec_close(pHandle->vc);
    	av_free(pHandle->vc);
		pHandle->vc = NULL;
	}
	
	pHandle->video_codec= NULL;
//	if (pHandle->audioOutBuf)
//		av_free(pHandle->audioOutBuf);
	if (pHandle->pkt){
		av_free(pHandle->pkt);
	}
    avcodec_free_frame(&pHandle->picture);
	free(pHandle);
	return ;
	
}


