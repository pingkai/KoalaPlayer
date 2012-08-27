/*
 * Copyright (c) 2005 Francois Revol
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <limits.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libavutil/time.h"
#include "libavutil/intreadwrite.h"

#include "libavformat/avformat.h"
#define MUXER_ES
#define AVP_BUFFSIZE 4096
#define INITIAL_BUFFER_SIZE 32768

#define PKT_CACHE_BUF_SIZE 1024*1024*5



typedef struct koala_handle_t{
	AVIOContext *in_put_pb;
	uint8_t* read_buffer; //in put buffer cache
	AVFormatContext *ctx;
	AVPacket *pkt;
	int video_stream; //selected video
	int audio_stream;  //seleceted audio


	// Meta data
	int nb_video_stream;
	int nb_audio_stream;
	char * video_stream_list;
	char * audio_stream_list;
	uint8_t * pkt_cache_buf;
	int  pkt_cache_buf_ptr;
	int  pkt_cache_buf_size;
	int pkt_cache_buf_stream_index;

	
	AVFormatContext *oc; //raw audio data to es use it
	AVStream * ast; //raw audio data to es use it
	AVCodecContext *ac;
	AVCodecContext *vc;
	uint8_t *audio_input_buffer; //raw audio data to es use it

	int a_time_base;
	int v_time_base;
	int write_h264_sps_pps;
	int write_h264_startcode;
	int mp4_vol;//mpeg4 video header
	uint8_t *pPkt_buf; //for audio muxer callback
	int *pPkt_buf_size;//for audio muxer callback

	//get in put data callback
	int (*read_packet)(void *opaque, uint8_t *buf, int buf_size);
	int64_t (*seek)(void *opaque, int64_t offset, int whence);
	void *opaque;
	
	
}koala_handle;

static int probe_buf_write(void *opaque, uint8_t *buf, int buf_size)
{
	koala_handle *pHandle = (koala_handle *)opaque;
	if ((*pHandle->pPkt_buf_size) < buf_size){
		pHandle->pkt_cache_buf_size = buf_size - *pHandle->pPkt_buf_size;
//		printf("%s:%d buf_size is %d pPkt_buf_size is %d\n",__FILE__,__LINE__,buf_size,*pHandle->pPkt_buf_size);
		pHandle->pkt_cache_buf = malloc (pHandle->pkt_cache_buf_size);
		memcpy(pHandle->pPkt_buf,buf,*pHandle->pPkt_buf_size);
		memcpy(pHandle->pkt_cache_buf,buf + *pHandle->pPkt_buf_size,pHandle->pkt_cache_buf_size);
		pHandle->pkt_cache_buf_ptr = 0;
	}else{
    	memcpy(pHandle->pPkt_buf,buf,buf_size);
		*(pHandle->pPkt_buf_size) = buf_size;
	}
    return 0;
}


void regist_input_file_func(koala_handle *pHandle,void *opaque,int (*read_packet)(void *opaque, uint8_t *buf, int buf_size),
								int64_t (*seek)(void *opaque, int64_t offset, int whence)){
	pHandle->read_packet = read_packet;
	pHandle->seek = seek;
	pHandle->opaque = opaque;
	return;
	
								
}

static int fill_stream_table(koala_handle *pHandle){
	int i;
	for(i = 0;i<pHandle->ctx->nb_streams;i++){
		if (pHandle->ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO){
			pHandle->video_stream_list[pHandle->nb_video_stream++] = i; 
		}
		if (pHandle->ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
			pHandle->audio_stream_list[pHandle->nb_audio_stream++] = i; 
		}
	}
	
	return 0;
}

int init_open(koala_handle *pHandle){
	int ret;
	AVInputFormat *in_fmt = NULL;
	pHandle->read_buffer = av_malloc(INITIAL_BUFFER_SIZE);
	if (pHandle->read_buffer == NULL)
		return -1;
	if (pHandle->read_packet == NULL){
		printf("%s:%d no read func\n",__FILE__,__LINE__);
		// TODO: open with filename
		return -1;
	}
	pHandle->ctx = avformat_alloc_context();
	av_register_all();

	pHandle->in_put_pb = avio_alloc_context(pHandle->read_buffer, INITIAL_BUFFER_SIZE,0, pHandle->opaque, pHandle->read_packet, NULL, pHandle->seek);
	if (pHandle->seek)
		pHandle->in_put_pb->seekable = 1;
	else
		pHandle->in_put_pb->seekable = 0;
	ret = av_probe_input_buffer(pHandle->in_put_pb, &in_fmt, NULL,NULL, 0, 0);
	if (ret < 0) {
		printf("%s:%d\n",__FILE__,__LINE__);
	    goto fail;
    }
	printf("in_fmt->name %s\n",in_fmt->name);
	pHandle->ctx->pb = pHandle->in_put_pb;
    ret = avformat_open_input(&pHandle->ctx, NULL, in_fmt, NULL);
    if (ret < 0){
		printf("%s:%d ret is %d\n",__FILE__,__LINE__,ret);
        goto fail;
    }
	ret = avformat_find_stream_info(pHandle->ctx, NULL);
	if (ret < 0) {
		fprintf(stderr, "avformat_find_stream_info: error %d\n", ret);
		return -1;
	}
	pHandle->pkt = (AVPacket *)av_malloc(sizeof(AVPacket));
	av_init_packet(pHandle->pkt);
	pHandle->video_stream_list = malloc(pHandle->ctx->nb_streams);
	pHandle->audio_stream_list = malloc(pHandle->ctx->nb_streams);
	fill_stream_table(pHandle);

 	return 0;
	
fail: 
	if (pHandle->ctx){
		avformat_free_context(pHandle->ctx);
	    pHandle->ctx = NULL;
	}
	return -1;
	
	
}
void close_demux(koala_handle *pHandle){
	if (pHandle->oc){
		avio_flush(pHandle->oc->pb);
		avformat_free_context(pHandle->oc);
	}
	avformat_close_input(&pHandle->ctx);
	av_free(pHandle->pkt);
	if (pHandle->audio_input_buffer)
		av_free(pHandle->audio_input_buffer);
	av_free(pHandle);

}
 
int get_nb_stream(koala_handle *pHandle,int *pNbAudio, int *pNbVideo){

	if (pNbAudio)
		*pNbAudio = pHandle->nb_audio_stream;
	if (pNbVideo)
		*pNbVideo = pHandle->nb_video_stream;
	return 0;
}

int open_audio(koala_handle *pHandle,int index){ 
	int err;
	if (index >= pHandle->nb_audio_stream || index < 0){
		printf("%s:%d No such audio\n",__FILE__,__LINE__);
		return -1;
	}
	pHandle->audio_stream = pHandle->audio_stream_list[index];
	if (pHandle->audio_input_buffer == NULL)
		pHandle->audio_input_buffer = av_malloc(AVP_BUFFSIZE);
	pHandle->ac = pHandle->ctx->streams[pHandle->audio_stream]->codec;
 
	// TODO: when switch audio please close it, if not aac or not need the filter
	if (pHandle->ac->codec_id == CODEC_ID_AAC){
	    pHandle->oc = avformat_alloc_context();
	    if (!pHandle->oc) {
	        fprintf(stderr, "Memory error\n");
	        return -1;
	    }
		pHandle->oc->oformat = av_guess_format("adts", NULL, NULL);
		pHandle->oc->pb = avio_alloc_context(pHandle->audio_input_buffer, AVP_BUFFSIZE,0, pHandle, NULL, probe_buf_write, NULL);
		if (pHandle->oc->pb == NULL){
			printf("%s:%d\n",__FILE__,__LINE__);
			return -1;
		}
    	pHandle->oc->pb->seekable = 0;		
		pHandle->ast = avformat_new_stream(pHandle->oc, NULL);
		if (pHandle->ast == NULL){
			printf("%s:%d\n",__FILE__,__LINE__);
			return -1;
		}
		err = avcodec_copy_context(pHandle->ast->codec,pHandle->ac);
		if (err < 0){
			printf("%s:%d\n",__FILE__,__LINE__);
			return -1;
		}
		err = avformat_write_header(pHandle->oc, NULL);
		if (err < 0){
			printf("%s:%d\n",__FILE__,__LINE__);
			return -1;
		}	
	}
	pHandle->a_time_base = pHandle->ctx->streams[pHandle->audio_stream]->time_base.den/pHandle->ctx->streams[pHandle->audio_stream]->time_base.num;
	return pHandle->audio_stream;
}

int open_video(koala_handle *pHandle,int index){
	int err = 0;
	if (index >= pHandle->nb_video_stream || index < 0){
		printf("%s:%d No such video\n",__FILE__,__LINE__);
		return -1;
	}
	pHandle->video_stream = pHandle->video_stream_list[index];
	pHandle->vc = pHandle->ctx->streams[pHandle->video_stream]->codec;
	pHandle->v_time_base = pHandle->ctx->streams[pHandle->video_stream]->time_base.den/pHandle->ctx->streams[pHandle->video_stream]->time_base.num;
	// TODO: when switch video please close it, if not h264 or not need the filter
	if ((pHandle->vc->codec_id == CODEC_ID_H264)
	&& pHandle->vc->extradata != NULL 
	&&(pHandle->vc->extradata[0] == 1)){
	    uint8_t *dummy_p;
	    int dummy_int;
		AVBitStreamFilterContext *bsfc= av_bitstream_filter_init("h264_mp4toannexb");
		if (!bsfc) {
			av_log(NULL, AV_LOG_ERROR, "Cannot open the h264_mp4toannexb BSF!\n");
			return -1;
		}
	    err = av_bitstream_filter_filter(bsfc, pHandle->vc, NULL, &dummy_p, &dummy_int, NULL, 0, 0);
		if (err < 0)
			printf("av_bitstream_filter_filter error\n");
	    av_bitstream_filter_close(bsfc);
		pHandle->write_h264_sps_pps = 1;
		pHandle->write_h264_startcode =1;
	}else 
	if (pHandle->vc->codec_id == CODEC_ID_MPEG4){
		pHandle->mp4_vol = 1;
	}
	if (err < 0)
		return -1;
	return pHandle->video_stream;
}

// TODO: deal with malloc error
int demux_read_packet(koala_handle *pHandle,uint8_t *pBuffer,int *pSize,int * pStream,int64_t *pPts){
	int err;
	int keyframe;
	uint8_t startcode[4] = {0,0,0,1};
	int in_buf_ptr = 0;

	pHandle->pPkt_buf = pBuffer;
	pHandle->pPkt_buf_size  = pSize;
	// when *pSize is not big enough only copy  *pSize,copy the left next time
	if (pHandle->pkt_cache_buf){
		int size = pHandle->pkt_cache_buf_size - pHandle->pkt_cache_buf_ptr;
		if (size <= *pSize){
			memcpy(pBuffer,pHandle->pkt_cache_buf + pHandle->pkt_cache_buf_ptr,size);
			free(pHandle->pkt_cache_buf);
			pHandle->pkt_cache_buf = NULL;
			pHandle->pkt_cache_buf_ptr = 0;
			pHandle->pkt_cache_buf_size = 0;
			*pSize = size;
		}else{
			memcpy(pBuffer,pHandle->pkt_cache_buf + pHandle->pkt_cache_buf_ptr,*pSize);
			pHandle->pkt_cache_buf_ptr += *pSize;
		}
		*pPts = 0;
		*pStream = pHandle->pkt_cache_buf_stream_index;
		return 0;
	}

	do{
		err = av_read_frame(pHandle->ctx, pHandle->pkt);
		if (err < 0)
			return -1;
		*pStream = pHandle->pkt->stream_index;
		pHandle->pkt_cache_buf_stream_index = *pStream;

		if (pHandle->pkt->stream_index == pHandle->video_stream){
			if (pHandle->pkt->flags &AV_PKT_FLAG_KEY)
				keyframe = 1;
			else
				keyframe = 0;
			pHandle->pkt->pts = pHandle->pkt->pts*1000 /pHandle->v_time_base;//ms
			*pPts = pHandle->pkt->pts;
			if (*pSize < (pHandle->vc->extradata_size + pHandle->pkt->size)){

				// TODO:  size = pHandle->vc->extradata_size + pHandle->pkt->size -*pSize
				pHandle->pkt_cache_buf_size = pHandle->vc->extradata_size + pHandle->pkt->size;
				pHandle->pkt_cache_buf = malloc(pHandle->pkt_cache_buf_size);
				if (pHandle->pkt_cache_buf == NULL){
					perror("malloc");
					return -1;
				}
				pHandle->pPkt_buf = pHandle->pkt_cache_buf;
			}
			if (pHandle->vc->codec_id == CODEC_ID_MPEG4 && keyframe &&pHandle->mp4_vol){
				memcpy(pHandle->pPkt_buf+ in_buf_ptr ,pHandle->vc->extradata ,pHandle->vc->extradata_size);
				in_buf_ptr += pHandle->vc->extradata_size;
			}

			if (pHandle->write_h264_sps_pps && keyframe){
				memcpy(pHandle->pPkt_buf + (in_buf_ptr),pHandle->vc->extradata ,pHandle->vc->extradata_size);
				in_buf_ptr += pHandle->vc->extradata_size;
				memcpy(pHandle->pPkt_buf + (in_buf_ptr),startcode,4);
				in_buf_ptr +=4;
				memcpy(pHandle->pPkt_buf + (in_buf_ptr),pHandle->pkt->data+4,pHandle->pkt->size-4);
				in_buf_ptr += (pHandle->pkt->size-4);
			}else{
				if (pHandle->write_h264_startcode){
					memcpy(pHandle->pPkt_buf + (in_buf_ptr),startcode,4);
					in_buf_ptr += 4;			
					memcpy(pHandle->pPkt_buf + (in_buf_ptr),pHandle->pkt->data+4,pHandle->pkt->size-4);
					in_buf_ptr += (pHandle->pkt->size-4);				
				}else{
					memcpy(pHandle->pPkt_buf + in_buf_ptr,pHandle->pkt->data,pHandle->pkt->size);
					in_buf_ptr += pHandle->pkt->size;
				}
			}

			if (pHandle->pPkt_buf == pHandle->pkt_cache_buf){
				memcpy(pBuffer,pHandle->pPkt_buf,*pSize);
				pHandle->pkt_cache_buf_ptr = *pSize;
			}else{
				*pSize = in_buf_ptr;
			}
			break;
		}
		else if (pHandle->pkt->stream_index == pHandle->audio_stream){
			if (pHandle->ac->codec_id == CODEC_ID_AAC &&((AV_RB16(pHandle->pkt->data) & 0xfff0) != 0xfff0)){
				pHandle->pkt->stream_index = pHandle->ast->index;
				av_write_frame(pHandle->oc,pHandle->pkt);
				pHandle->pkt->stream_index = pHandle->audio_stream;
			}else{
			//	*pSize = 0;
				if (*pSize < pHandle->pkt->size){
					pHandle->pkt_cache_buf_size = pHandle->pkt->size - *pSize;
					pHandle->pkt_cache_buf = malloc(pHandle->pkt_cache_buf_size);
					if (pHandle->pkt_cache_buf == NULL){
						perror("malloc");
						return -1;
					}
					memcpy(pHandle->pPkt_buf + in_buf_ptr,pHandle->pkt->data,*pSize);
					memcpy(pHandle->pkt_cache_buf,pHandle->pkt->data + *pSize,pHandle->pkt_cache_buf_size);
					
				}else{
					memcpy(pHandle->pPkt_buf + in_buf_ptr,pHandle->pkt->data,pHandle->pkt->size);
					in_buf_ptr += pHandle->pkt->size;
					*pSize = in_buf_ptr;
				}
			}
			*pPts = pHandle->pkt->pts*1000/pHandle->a_time_base;
			break;
		}else
			av_free_packet(pHandle->pkt);
	}while(1);
	av_free_packet(pHandle->pkt);

	return 0;

}

static void init_handle(koala_handle *pHandle){
	memset(pHandle,0,sizeof(koala_handle));
	pHandle->audio_stream = -1;
	pHandle->video_stream = -1;
}
koala_handle * koala_get_demux_handle(){
	koala_handle *pHandle = av_malloc(sizeof(koala_handle));
	init_handle(pHandle);
	return pHandle;
}
