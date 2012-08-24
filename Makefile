CC := gcc
OBJS := koala_demuxer.o
LDFLAGS = -lavformat -lavcodec -lavutil -lz -lpthread -lbz2 -lm 
CFLAGS :=  -Wall -g
koala_demuxer_test : $(OBJS)
	$(CC) $(CFLAGS) $^ $(LDFLAGS)  -o $@ 

.PHNOY: 
clean :
	rm $(OBJS) koala_demuxer_test
