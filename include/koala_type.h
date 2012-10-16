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
};


typedef struct {
        stream_type type;
        int64_t duration;//ms
        enum KoalaCodecID codec;


        // TODO:  use union
        //audio
        int channels;
        int samplerate;
        int bitsample;

        //video only
        int width;
        int height;

}stream_meta;
#endif
