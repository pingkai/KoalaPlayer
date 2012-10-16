CC := gcc
OBJS := koala_demuxer_test.o 
LDFLAGS = -lkoalaplayer -lavformat -lavcodec -lavutil  -lpthread  -lm -lz 
CFLAGS :=  -Wall -g -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
CFLAGS += -L../../libav/libavformat/ -L../../libav/libavcodec/ -L../../libav/libavutil/ -L../src -I../../libav -I../include
koala_demuxer_test : $(OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS)  -o $@ 

.PHNOY: 
clean :
	rm $(OBJS) koala_demuxer_test
