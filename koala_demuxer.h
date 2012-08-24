

#ifndef KOALA_DEMUXER_H
#define KOALA_DEMUXER_H
typedef struct koala_handle_ koala_handle;
koala_handle * koala_get_demux_handle();

void regist_input_file_func(koala_handle *pHandle,void *opaque,int (*read_packet)(void *opaque, uint8_t *buf, int buf_size),
								int64_t (*seek)(void *opaque, int64_t offset, int whence));
int init_open(koala_handle *pHandle);
int get_nb_stream(koala_handle *pHandle,int *pNbAudio, int *pNbVideo);

int open_audio(koala_handle *pHandle,int index);

int open_video(koala_handle *pHandle,int index);
int demux_read_packet(koala_handle *pHandle,uint8_t *pBuffer,int *pSize,int * pStream,int64_t *pPts);

void close_demux(koala_handle *pHandle);

#endif


