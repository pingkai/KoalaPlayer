/*
 * Copyright (c) 2012 pingkai010@gmail.com
 *
 *
 */

//#define ANDROID
#ifdef ANDROID
#define LOG_TAG "koala_demuxer"
#include <utils/Log.h>
#define printf ALOGI
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


#define MUXER_ES
#define AVP_BUFFSIZE 4096
#define INITIAL_BUFFER_SIZE 32768

#define PKT_CACHE_BUF_SIZE 1024*1024*5



typedef struct koala_handle_t{
	AVIOContext *in_put_pb;
	int  interruptIO;
	demux_mode_e demux_mode;
	uint8_t* read_buffer; //in put buffer cache
	AVFormatContext *ctx;
	AVPacket *pkt;
	int video_stream; //selected video
	int audio_stream;  //seleceted audio
	AVBitStreamFilterContext *avcbsfc;


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
	void (*log_callback)(void*, int, const char*, va_list);

	int wrape_aac2adts;
	
	
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
	if (pHandle){
		pHandle->read_packet = read_packet;
		pHandle->seek = seek;
		pHandle->opaque = opaque;
	}
	return;								
}

void regist_log_call_back(koala_handle *pHandle,void (*callback)(void*, int, const char*, va_list)){
	if (pHandle)
		pHandle->log_callback = callback;
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
static int interrupt_cb(void *opaque){
	koala_handle *pHandle = (koala_handle *)opaque;
	// TODO: use a lock?
	if (pHandle->interruptIO){
		pHandle->interruptIO = 0;
		printf("interrupted\n");
		return 1;
	}else
    	return 0;
}
void interrupt_demuxer(koala_handle *pHandle){
	if (pHandle)
		pHandle->interruptIO = 1;
	return;
}

// TODO: How to deal with  interrupt ?

int init_open(koala_handle *pHandle,const char *filename){
	int ret;
	AVInputFormat *in_fmt = NULL;
	int use_filename = 0;
	if (pHandle == NULL)
		return -1;
	pHandle->read_buffer = av_malloc(INITIAL_BUFFER_SIZE);
	if (pHandle->read_packet == NULL ){
		printf("%s:%d no read func\n",__FILE__,__LINE__);
		if (filename == NULL)
			return -1;
		else{
			use_filename = 1;
		}
	}
	pHandle->ctx = avformat_alloc_context();
	pHandle->ctx->interrupt_callback.callback = interrupt_cb;
	pHandle->ctx->interrupt_callback.opaque = pHandle;
	if (pHandle->log_callback)
		av_log_set_callback(pHandle->log_callback);
	av_register_all();
	avformat_network_init();
	if (!use_filename){
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
	}
	if (!use_filename)
    	ret = avformat_open_input(&pHandle->ctx, NULL, in_fmt, NULL);
	else
		ret = avformat_open_input(&pHandle->ctx, filename, NULL, NULL);
    if (ret < 0){
		printf("%s:%d ret is %d\n",__FILE__,__LINE__,ret);
        goto fail;
    }
	ret = avformat_find_stream_info(pHandle->ctx, NULL);
	if (ret < 0) {
		printf("avformat_find_stream_info: error %d\n", ret);
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
	if (pHandle == NULL)
		return;

	if (pHandle->oc){
		avio_flush(pHandle->oc->pb);
		avformat_free_context(pHandle->oc);
	}
	if (pHandle->ctx){
		avformat_close_input(&pHandle->ctx);
		pHandle->ctx = NULL;
	}
	if (pHandle->pkt){
		av_free(pHandle->pkt);
		pHandle->pkt = NULL;
	}
	if (pHandle->audio_input_buffer){
		av_free(pHandle->audio_input_buffer);
		pHandle->audio_input_buffer = NULL;
	}
	if (pHandle->video_stream_list)
		free(pHandle->video_stream_list);
	if (pHandle->audio_stream_list)
		free(pHandle->audio_stream_list);
	if (pHandle->avcbsfc)
	av_bitstream_filter_close(pHandle->avcbsfc);

	av_free(pHandle);

}
 
int get_nb_stream(koala_handle *pHandle,int *pNbAudio, int *pNbVideo){
	if (pHandle == NULL)
		return -1;

	if (pNbAudio)
		*pNbAudio = pHandle->nb_audio_stream;
	if (pNbVideo)
		*pNbVideo = pHandle->nb_video_stream;
	return pHandle->ctx->nb_streams;
}

static enum KoalaCodecID avcodec2Koalacodec(enum AVCodecID codec_id){
	switch(codec_id){
		case AV_CODEC_ID_H264:
			return KOALA_CODEC_ID_H264;
		case AV_CODEC_ID_AAC:
		case AV_CODEC_ID_AAC_LATM:
			return KOALA_CODEC_ID_AAC;
		case AV_CODEC_ID_MP3:
			return KOALA_CODEC_ID_MP3;
		case AV_CODEC_ID_APE:
			return KOALA_CODEC_ID_APE;
		default:
			break;
	}
	return  KOALA_CODEC_ID_NONE;
}
static int get_aac_profile( AVCodecContext *codec){
	int object_type;
	object_type = codec->extradata[0] >>3 &0x1f;
	codec->profile = object_type;
	printf("object_type is %d\n",object_type);
	return 0;
}

int get_stream_meta_by_index(koala_handle *pHandle,int index,stream_meta* meta){
	
	AVStream *pStream;
	if (pHandle == NULL || meta == NULL)
		return -1;
	if (index > pHandle->ctx->nb_streams){
		printf("%s:%d no such stream\n",__FILE__,__LINE__);
		return -1;
	}
	pStream	= pHandle->ctx->streams[index];
	enum AVMediaType codec_type = pStream->codec->codec_type;
//	pStream->codec->codec_id
	printf("%s:%d pStream->codec->codec_id is %d\n",__FILE__,__LINE__,pStream->codec->codec_id);
	// TODO: duration
//	if (pStream->duration == 0)
		meta->duration = pHandle->ctx->duration/(AV_TIME_BASE/1000);
//	else{
//		meta->duration = pStream->duration*(pStream->time_base.num/pStream->time_base.den);
//	}
	meta->codec = avcodec2Koalacodec(pStream->codec->codec_id);
	meta->index = index;
	if (codec_type == AVMEDIA_TYPE_VIDEO){
		meta->type = STREAM_TYPE_VIDEO;
		meta->width = pStream->codec->width;
		meta->height = pStream->codec->height;
		
	}else if (codec_type == AVMEDIA_TYPE_AUDIO){
		meta->type = STREAM_TYPE_AUDIO;
		if (pStream->codec->codec_id == AV_CODEC_ID_AAC)
			get_aac_profile(pStream->codec);
		meta->channels = pStream->codec->channels;
		meta->samplerate = pStream->codec->sample_rate;
		meta->profile = pStream->codec->profile;
	}else{
		meta->type = STREAM_TYPE_UNKNOWN;
	}
	return 0;
}

static int stream_index2av_index(koala_handle *pHandle,enum AVMediaType type,int index){
	int i = -1;
	if (type == AVMEDIA_TYPE_VIDEO){
		for (i = 0; i < pHandle->nb_video_stream;i++)
			if (pHandle->video_stream_list[i]== index)
				break;
	}else if (type == AVMEDIA_TYPE_AUDIO){
		for (i = 0; i < pHandle->nb_audio_stream;i++)
			if (pHandle->audio_stream_list[i]== index)
				break;
	}
	return i;
}

int open_stream(koala_handle *pHandle,int index){
	int av_index;
	if (pHandle == NULL)
		return -1;
	enum AVMediaType codec_type = pHandle->ctx->streams[index]->codec->codec_type;

	if (index >pHandle->ctx->nb_streams){
		printf("No such stream");
		return -1;
	}

	av_index = stream_index2av_index(pHandle,codec_type,index);
	if (av_index < 0){
		printf("Not support\n");
		return -1;
	}

	switch (codec_type){
		case AVMEDIA_TYPE_VIDEO:
			return open_video(pHandle,av_index);
		case AVMEDIA_TYPE_AUDIO:
			return open_audio(pHandle,av_index);
		default:
			printf("Not support\n");
			return -1;
	}
	return -1;
}
int open_audio(koala_handle *pHandle,int index){ 
	int err;
	if (pHandle == NULL)
		return -1;

	printf("%s\n",__func__);
	if (index >= pHandle->nb_audio_stream || index < 0){
		printf("%s:%d No such audio\n",__FILE__,__LINE__);
		return -1;
	}
	pHandle->audio_stream = pHandle->audio_stream_list[index];
	if (pHandle->audio_input_buffer == NULL)
		pHandle->audio_input_buffer = av_malloc(AVP_BUFFSIZE);
	pHandle->ac = pHandle->ctx->streams[pHandle->audio_stream]->codec;
 
	// TODO: when switch audio please close it, if not aac or not need the filter
	if (pHandle->wrape_aac2adts
		&&pHandle->ac->codec_id == AV_CODEC_ID_AAC){
	    pHandle->oc = avformat_alloc_context();
	    if (!pHandle->oc) {
	        fprintf(stderr, "Memory error\n");
	        return -1;
	    }
		pHandle->oc->oformat = av_guess_format("adts", NULL, NULL);
		if (pHandle->oc->oformat == NULL){
			printf("%s:%d\n",__FILE__,__LINE__);
			return -1;
		}
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
int set_demuxer_mode(koala_handle *pHandle,demux_mode_e mode){
	if (pHandle == NULL)
		return -1;
	pHandle->demux_mode = mode;
	return mode;
}
int open_video(koala_handle *pHandle,int index){
//	int i ;
	printf("%s\n",__func__);
	if (pHandle == NULL)
		return -1;

	if (index >= pHandle->nb_video_stream || index < 0){
		printf("%s:%d No such video\n",__FILE__,__LINE__);
		return -1;
	}
	pHandle->video_stream = pHandle->video_stream_list[index];
	pHandle->vc = pHandle->ctx->streams[pHandle->video_stream]->codec;
	pHandle->v_time_base = pHandle->ctx->streams[pHandle->video_stream]->time_base.den/pHandle->ctx->streams[pHandle->video_stream]->time_base.num;
	// TODO: when switch video please close it, if not h264 or not need the filter
	if ((pHandle->vc->codec_id == AV_CODEC_ID_H264)
	&& pHandle->vc->extradata != NULL 
	&&(pHandle->vc->extradata[0] == 1)
	){
	    uint8_t *dummy_p;
	    int dummy_int;
		int i;
		pHandle->avcbsfc = av_bitstream_filter_init("h264_mp4toannexb");
		if (!pHandle->avcbsfc) {
			av_log(NULL, AV_LOG_ERROR, "Cannot open the h264_mp4toannexb BSF!\n");
			return -1;
		}
	//	for(i = 0; i< pHandle->vc->extradata_size;i++)
	//		printf("%02x ",pHandle->vc->extradata[i]);
	//	printf("\n");
	//	av_bitstream_filter_filter(bsfc, pHandle->vc, NULL, &dummy_p, &dummy_int, NULL, 0, 0);
	//	if (err < 0)
	//		printf("av_bitstream_filter_filter error\n");
	//	for(i = 0; i< pHandle->vc->extradata_size;i++)
	//		printf("%02x ",pHandle->vc->extradata[i]);
	//	printf("\n");

	//    av_bitstream_filter_close(bsfc);
		pHandle->write_h264_sps_pps = 1;
		pHandle->write_h264_startcode =1;
	}else 
	if (pHandle->vc->codec_id == AV_CODEC_ID_MPEG4){
		pHandle->mp4_vol = 1;
	}
//	if (err < 0)
//		return -1;
	return pHandle->video_stream;
}

// TODO: deal with malloc error

int demux_seek(koala_handle *pHandle,int64_t timems,int stream_id){
	int ret;
	int64_t timestamp;
	if (pHandle == NULL)
		return -1;

	if (pHandle->pkt_cache_buf){
			free(pHandle->pkt_cache_buf);
			pHandle->pkt_cache_buf = NULL;
			pHandle->pkt_cache_buf_ptr = 0;
			pHandle->pkt_cache_buf_size = 0;
	}
	if (stream_id == pHandle->audio_stream){
		printf("%s:%d timems is %lld pHandle->a_time_base is %d\n",__FILE__,__LINE__,timems,pHandle->a_time_base);
		timestamp = (timems/1000) * pHandle->a_time_base;
		
	}else if (stream_id == pHandle->video_stream){
		printf("%s:%d timems is %lld pHandle->v_time_base is %d\n",__FILE__,__LINE__,timems,pHandle->v_time_base);
		timestamp = (timems/1000) * pHandle->v_time_base;
	
	}
	else{
		stream_id = -1;
		timestamp = timestamp/1000;
	}
	printf("%s:%d timestamp is %lld stream_id is %d\n",__func__,__LINE__,timestamp,stream_id);
	//ret = avformat_seek_file(pHandle->ctx, stream_id, INT64_MIN, timestamp, timestamp, 0);
	ret = avformat_seek_file(pHandle->ctx, stream_id, timestamp, timestamp, INT64_MAX, 0);
	return ret;

}
int demux_read_packet(koala_handle *pHandle,uint8_t *pBuffer,int *pSize,int * pStream,int64_t *pPts,int *pFlag){
	int err;
	int keyframe;
	uint8_t startcode[4] = {0,0,0,1};
	int in_buf_ptr = 0;

	if (pHandle == NULL ||pBuffer == NULL||pSize == NULL){
		printf("%s:%d\n",__FILE__,__LINE__);
		return -1;
	}

	pHandle->pPkt_buf = pBuffer;
	pHandle->pPkt_buf_size  = pSize;
	// when *pSize is not big enough only copy  *pSize,copy the left next time
	// TODO: Maybe can use pkt,no need to mallloc and copy
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
		if (pPts)
			*pPts = -1;
		if (pStream)
			*pStream = pHandle->pkt_cache_buf_stream_index;
		return 0;
	}

	do{
		// TODO: How to deal with  interrupt ?
		if (interrupt_cb(pHandle)){
			printf("%s:%d interrupt\n",__FILE__,__LINE__);
			break;
		}
		// TODO: when be interrupt return what? AVERROR_EXIT? How to deal with it?
		err = av_read_frame(pHandle->ctx, pHandle->pkt);
		if (err < 0){
            char errbuf[50];
            av_strerror(err, errbuf, sizeof(errbuf));
			printf("%s:%d: %s\n",__FILE__,__LINE__,errbuf);
			if (err == AVERROR_EOF &&(pHandle->oc != NULL )){
				int ret;
				ret = av_write_frame(pHandle->oc,NULL);
				if (ret == 0){
					if (pPts)
						*pPts = -1;
					if (pStream)
						*pStream = pHandle->audio_stream;
					if (pFlag)
						*pFlag = 1;
					return 0;
				}
			}
			return -1;
		}
		if (pHandle->demux_mode == DEMUX_MODE_I_FRAME){
			if (pHandle->pkt->stream_index != pHandle->video_stream
				||(pHandle->pkt->flags &AV_PKT_FLAG_KEY) == 0
				)
				continue;
		}
		if (pStream){
			*pStream = pHandle->pkt->stream_index;
			pHandle->pkt_cache_buf_stream_index = *pStream;
		}
		if (pFlag)
			*pFlag = pHandle->pkt->flags &AV_PKT_FLAG_KEY;

		if (pHandle->pkt->stream_index == pHandle->video_stream){
			if (pHandle->pkt->flags &AV_PKT_FLAG_KEY)
				keyframe = 1;
			else
				keyframe = 0;
			pHandle->pkt->pts = pHandle->pkt->pts*1000 /pHandle->v_time_base;//ms
			if (pPts)
				*pPts = pHandle->pkt->pts;
			printf("video pHandle->pkt->size is %d\n",pHandle->pkt->size);
			if (*pSize < (pHandle->vc->extradata_size + pHandle->pkt->size)){

				// TODO:  size = pHandle->vc->extradata_size + pHandle->pkt->size -*pSize
				pHandle->pkt_cache_buf_size = pHandle->vc->extradata_size + pHandle->pkt->size;
				pHandle->pkt_cache_buf = malloc(pHandle->pkt_cache_buf_size);
				if (pHandle->pkt_cache_buf == NULL){
					printf("%s:%d\n",__FILE__,__LINE__);
					perror("malloc");
					return -1;
				}
				pHandle->pPkt_buf = pHandle->pkt_cache_buf;
			}
			if (pHandle->vc->codec_id == AV_CODEC_ID_MPEG4 && keyframe &&pHandle->mp4_vol){
				memcpy(pHandle->pPkt_buf+ in_buf_ptr ,pHandle->vc->extradata ,pHandle->vc->extradata_size);
				in_buf_ptr += pHandle->vc->extradata_size;
			}else

			if (pHandle->write_h264_sps_pps/* && keyframe*/){
				if (1){
			        AVPacket new_pkt = *pHandle->pkt;
				//	AVBitStreamFilterContext *bsfc= av_bitstream_filter_init("h264_mp4toannexb");
			        int a = av_bitstream_filter_filter(pHandle->avcbsfc, pHandle->vc, NULL,
			                                           &new_pkt.data, &new_pkt.size,
			                                           pHandle->pkt->data, pHandle->pkt->size,
			                                           pHandle->pkt->flags & AV_PKT_FLAG_KEY);
			        if (a > 0) {
			            av_free_packet(pHandle->pkt);
			            new_pkt.destruct = av_destruct_packet;
			        } else if (a < 0) {
			            av_log(NULL, AV_LOG_ERROR, "%s failed for stream %d, codec %s",
			                   pHandle->avcbsfc->filter->name, pHandle->pkt->stream_index,
			                   pHandle->vc->codec ? pHandle->vc->codec->name : "copy");
			          //  print_error("", a);
			        }
			        *pHandle->pkt = new_pkt;
					memcpy(pHandle->pPkt_buf + (in_buf_ptr),pHandle->pkt->data,pHandle->pkt->size);
					in_buf_ptr += pHandle->pkt->size;
					
			    }
				else
			    {
					memcpy(pHandle->pPkt_buf + (in_buf_ptr),pHandle->vc->extradata ,pHandle->vc->extradata_size);
					in_buf_ptr += pHandle->vc->extradata_size;
					memcpy(pHandle->pPkt_buf + (in_buf_ptr),startcode,4);
					in_buf_ptr +=4;
					memcpy(pHandle->pPkt_buf + (in_buf_ptr),pHandle->pkt->data+4,pHandle->pkt->size-4);
					in_buf_ptr += (pHandle->pkt->size-4);
			    }
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
		//	printf("audio pHandle->pkt->size is %d\n",pHandle->pkt->size);
			if (pHandle->ac->codec_id == AV_CODEC_ID_AAC 
				&&((AV_RB16(pHandle->pkt->data) & 0xfff0) != 0xfff0)
				&& pHandle->wrape_aac2adts){
				pHandle->pkt->stream_index = pHandle->ast->index;
				av_write_frame(pHandle->oc,pHandle->pkt);
				pHandle->pkt->stream_index = pHandle->audio_stream;
			}else{
			//	*pSize = 0;
				if (*pSize < pHandle->pkt->size){
					pHandle->pkt_cache_buf_size = pHandle->pkt->size - *pSize;
					pHandle->pkt_cache_buf = malloc(pHandle->pkt_cache_buf_size);
					if (pHandle->pkt_cache_buf == NULL){
						printf("%s:%d\n",__FILE__,__LINE__);
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
			if (pPts)
				*pPts = pHandle->pkt->pts*1000/pHandle->a_time_base;
			break;
		}else
			av_free_packet(pHandle->pkt);
	}while(1);
	av_free_packet(pHandle->pkt);
	return 0;
}

int  koala_set_aac_wrape_type(koala_handle *pHandle,int adts){
	if (pHandle == NULL)
		return -1;
	if (pHandle->audio_stream >= 0){
		printf("%s Please call me before open audio stream\n",__func__);
		return pHandle->wrape_aac2adts;
	}
	pHandle->wrape_aac2adts = adts;
	return adts;
}

static void init_handle(koala_handle *pHandle){
	memset(pHandle,0,sizeof(koala_handle));
	pHandle->audio_stream = -1;
	pHandle->video_stream = -1;
	pHandle->wrape_aac2adts = 1;
}
koala_handle * koala_get_demux_handle(){
	koala_handle *pHandle = av_malloc(sizeof(koala_handle));
	if (pHandle == NULL)
		return NULL;
	init_handle(pHandle);
	return pHandle;
}

// TODO: move to another file

int koala_sinff(const char *short_name,unsigned char *buf,int buf_size){
	AVInputFormat  * fmt;
	AVProbeData pd = {"", NULL, 0 };
	int score = 0;
	if (buf == NULL || buf_size <= 0)
		return score;
	pd.buf_size = buf_size;
	pd.buf = buf;
	av_register_all();
	fmt = av_find_input_format(short_name);
	if (fmt == NULL){
		printf("%s:%d\n",__func__,__LINE__);
		return score;
	}
	if (fmt->read_probe){
		score = fmt->read_probe(&pd);
		printf("%s:%d score is %d\n",__func__,__LINE__,score);
	}
	return score;
}


