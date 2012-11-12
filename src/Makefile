CC := gcc
AR := ar
OBJS :=  koala_demuxer.o tty.o koala_decoder_audio.o 
LDFLAGS = -lavformat -lavcodec -lavutil -lz -lpthread -lbz2 -lm 
CFLAGS :=  -Wall -g -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
CFLAGS += -L../../libav/libavformat/ -L../../libav/libavcodec/ -L../../libav/libavutil/ -I../../libav -I../include
libkoalaplayer.a : $(OBJS)
	$(AR) -r $@ $^ 

.PHNOY: 
clean :
	rm $(OBJS) libkoalaplayer.a
