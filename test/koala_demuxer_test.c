/*
 * Copyright (c) 2012 pingkai010@gmail.com
 *
 *
 */

#include <limits.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdint.h>
#include <pthread.h>

#include "koala_demuxer.h"
#include "koala_type.h"
#include "tty.h"


#define  MAX_PKT_SIZE   1024*256

struct main_handle {
	koala_handle *pKoalaHandle;
	int open_with_koala;
	int quite;
	int eos;

};


static int read_data(void *opaque, uint8_t *buf, int buf_size){
	int ret;
	int fd = *(int *)opaque;
	ret = read(fd,buf,buf_size);
	return ret;
}
#define AVSEEK_SIZE 0x10000
static int64_t seek(void *opaque, int64_t offset, int whence){
	int ret;
	int fd = *(int *)opaque;
	if (whence == AVSEEK_SIZE)
		return -1;
	ret = lseek(fd,offset,whence);
	return ret;
}

static void * control_thread(struct main_handle *pMainContent){
	char ch = -1;
	tty_set_noblock();
	while(!pMainContent->eos){
		do {
			ch = getchar();
			if (ch == -1)
				usleep(100000);
		}while ((ch == -1 ||ch == '\n') && !pMainContent->eos);

		switch (ch){
			case 'q':
				if (pMainContent->open_with_koala)
					interrupt_demuxer(pMainContent->pKoalaHandle);
				pMainContent->quite = 1;
				printf("Quiting\n");
				break;
			default:
				//printf("unknown command %c\n",ch);
				break;
				
		}
		if (pMainContent->quite)
			break;
	}
	tty_reset();
	return (void* )0;

}
int main(int argc, char **argv)
{
	int err;
	int size;
	uint8_t *pkt_buf = malloc(MAX_PKT_SIZE);
	koala_handle *pHandle;
	int stream_index;
	int64_t pts;
	int flag,mode;
	int ifd = 0;
	int NbAudio,NbVideo;
	int v_index, a_index;
	int afd,vfd;
	pthread_t control_threah_id;
	struct main_handle main_content = {0,};
	if (argc <2){
		printf("%s filename\n",argv[0]);
		return 1;
	}

//	if (strncmp(argv[1],"http://",7) == 0 
//		||strncmp(argv[1],"rtsp://",7) == 0
//		|| strncmp(argv[1],"mms://",6) == 0
//		)
		main_content.open_with_koala = 1;
	unlink("audio.aac");
	unlink("video.h264");
	afd  = open("audio.aac", O_WRONLY | O_CREAT, 0644);
	vfd  = open("video.h264", O_WRONLY | O_CREAT, 0644);
	pHandle = koala_get_demux_handle();
	main_content.pKoalaHandle = pHandle;
	pthread_create(&control_threah_id,NULL,(void *)control_thread,&main_content);
	if (!main_content.open_with_koala){
		ifd = open(argv[1], O_RDONLY);
		if (ifd < 0){
			printf("%s:%d\n",__FILE__,__LINE__);
			goto cleanup;
		}
		regist_input_file_func(pHandle,&ifd,read_data,seek);
	}
	err = init_open(pHandle,main_content.open_with_koala ? argv[1]:NULL);
	if (err < 0){
		printf("%s:%d\n",__FILE__,__LINE__);
		goto cleanup;
	}
	get_nb_stream(pHandle,&NbAudio,&NbVideo);
	if (NbAudio > 0)
		a_index = open_audio(pHandle,0);
	if (NbVideo > 0){
	//	mode = set_demuxer_mode(pHandle,DEMUX_MODE_I_FRAME);
		v_index = open_video(pHandle,0);
	}
	while(!main_content.quite){

		size = MAX_PKT_SIZE;
		err = demux_read_packet(pHandle,pkt_buf,&size,&stream_index,&pts,&flag);
		if (err < 0){
			// TODO: flush ?
			main_content.eos = 1;
			break;
		}
		if (stream_index == v_index){
#if (__WORDSIZE == 64)
			printf("V size is %d,pts is %ld\n",size,pts);
#else
			printf("V size is %d,pts is %lld\n",size,pts);
#endif
			if (mode == DEMUX_MODE_I_FRAME)
				mode = set_demuxer_mode(pHandle,DEMUX_MODE_NORMOL);
			write(vfd,pkt_buf,size);
		}
		else if (stream_index == a_index){
#if (__WORDSIZE == 64)
//			printf("A size is %d,pts is %ld\n",size,pts);
#else
//			printf("A size is %d,pts is %lld\n",size,pts);
#endif
			write(afd,pkt_buf,size);

		}
		else{
			printf("Other\n");
		}
	}
cleanup:
	main_content.eos = 1;
	pthread_join(control_threah_id,NULL);
	close_demux(pHandle);
	if (ifd)
		close(ifd);
	close(afd);
	close(vfd);
	if (pkt_buf)
		free(pkt_buf);
	return 0;
}

