/*
 * Copyright (c) 2012 pingkai010@gmail.com
 *
 *
 */
#ifndef KOALA_DECODER_VIDEO_H
#define KOALA_DECODER_VIDEO_H
#ifdef __cplusplus
extern "C"
{
#endif

typedef struct koala_decoder_handle_v_t koala_decoder_handle_v;

koala_decoder_handle_v* init_decoder_video(void *codec_cont);

int reg_video_decoder_cb(koala_decoder_handle_v *pHandle,decoder_buf_callback_video cb,void *CbpHandle);

int get_video_info(koala_decoder_handle_v* pHandle,video_info *info);


int koala_ffmpeg_decode_video_pkt(koala_decoder_handle_v* pHandle,uint8_t *data,int size,long long pts);

void close_decoder_video(koala_decoder_handle_v* pHandle);

#ifdef __cplusplus
}
#endif

#endif

