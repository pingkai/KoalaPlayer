/*
 * Copyright (c) 2012 pingkai010@gmail.com
 *
 *
 */
#ifndef KOALA_TYPE_H
#define KOALA_TYPE_H
typedef enum {
	DEMUX_MODE_NORMOL,
	DEMUX_MODE_I_FRAME,
} demux_mode_e;

typedef enum {
	STREAM_TYPE_UNKNOWN,
	STREAM_TYPE_VIDEO,
	STREAM_TYPE_AUDIO,
} stream_type;

enum KoalaCodecID{
	KOALA_CODEC_ID_NONE,
	
	KOALA_CODEC_ID_H264,
	KOALA_CODEC_ID_MPEG4,
    KOALA_CODEC_ID_RV40,
	
	KOALA_CODEC_ID_AAC,
	KOALA_CODEC_ID_MP3,
	KOALA_CODEC_ID_APE,
};
typedef struct{
    int nChannles;
    int sample_rate;
    int sample_fmt;

}audio_info;

typedef struct{
    int pix_fmt;
    int width;
    int height;

}video_info;



typedef struct {
	stream_type type;
	int64_t duration;//ms
	enum KoalaCodecID codec;
	int index;
	int nb_index_entries;
    void * koala_codec_context;
    int koala_codec_context_size;


	// TODO:  use union
	//audio
	int channels;
	int samplerate;
	int profile;
    int bits_per_coded_sample;

	//video only
	int width;
	int height;
	
}stream_meta;

#define MAX_AUDIO_FRAME_SIZE 192000

typedef int  (*decoder_buf_callback) (unsigned char *buffer, int size,long long pts,void *CbpHandle);
typedef int  (*decoder_buf_callback_video)(unsigned char *buffer[], int linesize[],long long pts,void *CbpHandle);

#endif
