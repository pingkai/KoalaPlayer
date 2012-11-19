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

typedef struct koala_decoder_handle_t koala_decoder_handle;

koala_decoder_handle* init_decoder_audio(void *codec_cont);

void close_decoder_audio(koala_decoder_handle* pHandle);


int reg_audio_decoder_cb(koala_decoder_handle *pHandle,decoder_buf_callback cb,void *CbpHandle);


int koala_ffmpeg_decode_audio_pkt(koala_decoder_handle* pHandle,uint8_t *data,int size,long long pts);

int get_audio_info(void *codec_cont,audio_info *info);


#ifdef __cplusplus
}
#endif

#endif



