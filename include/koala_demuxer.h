
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
int get_nb_stream(koala_handle *pHandle,int *pNbAudio, int *pNbVideo);

int open_audio(koala_handle *pHandle,int index);
int set_demuxer_mode(koala_handle *pHandle,demux_mode_e mode);
int open_stream(koala_handle *pHandle,int index);

int open_video(koala_handle *pHandle,int index);
int demux_read_packet(koala_handle *pHandle,uint8_t *pBuffer,int *pSize,int * pStream,int64_t *pPts,int *pFlag);

void close_demux(koala_handle *pHandle);
void interrupt_demuxer(koala_handle *pHandle);
int get_stream_meta_by_index(koala_handle *pHandle,int index,stream_meta* meta);

#ifdef __cplusplus
}
#endif

#endif


