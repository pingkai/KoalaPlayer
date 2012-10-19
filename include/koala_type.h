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

	
	KOALA_CODEC_ID_AAC,
	KOALA_CODEC_ID_MP3,
};


typedef struct {
	stream_type type;
	int64_t duration;//ms
	enum KoalaCodecID codec;
	int index;


	// TODO:  use union
	//audio
	int channels;
	int samplerate;
	int profile;

	//video only
	int width;
	int height;
	
}stream_meta;
#endif
