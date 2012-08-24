CC := gcc
OBJS := koala_demuxer_test.o koala_demuxer.o 
LDFLAGS = -lavformat -lavcodec -lavutil -lz -lpthread -lbz2 -lm 
CFLAGS :=  -Wall -g -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE
koala_demuxer_test : $(OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS)  -o $@ 

.PHNOY: 
clean :
	rm $(OBJS) koala_demuxer_test
