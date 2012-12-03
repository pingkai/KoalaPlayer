/*
 * Copyright (c) 2012 pingkai010@gmail.com
 *
 *
 */

#ifndef KOALA_DEMUXER_H
#define KOALA_DEMUXER_H
#ifdef __cplusplus
extern "C"
{
#endif
#include "koala_type.h"
typedef struct koala_handle_t koala_handle;
koala_handle * koala_get_demux_handle();

void regist_input_file_func(koala_handle *pHandle,void *opaque,int (*read_packet)(void *opaque, uint8_t *buf, int buf_size),
								int64_t (*seek)(void *opaque, int64_t offset, int whence));
void regist_log_call_back(koala_handle *pHandle,void (*callback)(void*, int, const char*, va_list));

int init_open(koala_handle *pHandle,const char *filename);

int get_stream_codec_extra_data(koala_handle *pHandle,int index,uint8_t *extradata);

int get_nb_stream(koala_handle *pHandle,int *pNbAudio, int *pNbVideo);
int  koala_set_aac_wrape_type(koala_handle *pHandle,int adts);

int open_audio(koala_handle *pHandle,int index);
int set_demuxer_mode(koala_handle *pHandle,demux_mode_e mode);
int open_stream(koala_handle *pHandle,int index);

int open_video(koala_handle *pHandle,int index);
int demux_seek(koala_handle *pHandle,int64_t timems,int stream_id,int flag);

int koala_demux_pre_read_packet(koala_handle *pHandle);


int koala_demux_read_packet_data(koala_handle *pHandle,uint8_t *pBuffer,int *pSize,int * pStream,int64_t *pPts,int *pFlag);


int demux_read_packet(koala_handle *pHandle,uint8_t *pBuffer,int *pSize,int * pStream,int64_t *pPts,int *pFlag);

void close_demux(koala_handle *pHandle);
void interrupt_demuxer(koala_handle *pHandle);
int get_stream_meta_by_index(koala_handle *pHandle,int index,stream_meta* meta);
int koala_sinff(const char * short_name,unsigned char *buf,int buf_size);
void *koala_get_codec_data(koala_handle *pHandle,int index);



#ifdef __cplusplus
}
#endif

#endif


