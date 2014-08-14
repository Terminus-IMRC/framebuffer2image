#define __USE_POSIX
#include <limits.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

int d;

void read_fb_init(char *devname, struct fb_var_screeninfo *sc, uint32_t *effective_bytes_per_pixel, uint64_t *size)
{
	if((d=open(devname, O_RDONLY))==-1){
		perror("open");
		exit(EXIT_FAILURE);
	}

	if(ioctl(d, FBIOGET_VSCREENINFO, sc)==-1){
		perror("ioctl(..., FBIOGET_VSCREENINFO, ...)");
		exit(EXIT_FAILURE);
	}

	*effective_bytes_per_pixel=sc->bits_per_pixel%8==0?sc->bits_per_pixel/8:(uint32_t)(sc->bits_per_pixel/8)+1;
	*size=*effective_bytes_per_pixel*sc->xres*sc->yres;

	return;
}

void read_fb(char *buf, uint32_t size)
{
	ssize_t rc;
	off_t off;

	off=lseek(d, 0, SEEK_SET);
	if(off==-1){
		perror("lseek");
		exit(EXIT_FAILURE);
	}else if(off!=0){
		fprintf(stderr, "error: lseek returned unexpected offset\n");
		exit(EXIT_FAILURE);
	}

	if(SSIZE_MAX<size){
		uint64_t read_bytes;

		for(read_bytes=0; read_bytes<size; read_bytes+=SSIZE_MAX){
			rc=read(d, (uint8_t*)buf+read_bytes, SSIZE_MAX);
			if(rc==-1){
				perror("read");
				exit(EXIT_FAILURE);
			}else if(rc!=SSIZE_MAX){
				fprintf(stderr, "error: read returned unexpected read count\n");
				exit(EXIT_FAILURE);
			}
		}
		rc=read(d, (uint8_t*)buf+(read_bytes-SSIZE_MAX), size-(read_bytes-SSIZE_MAX));
		if(rc==-1){
			perror("read");
			exit(EXIT_FAILURE);
		}else if((uint64_t)rc!=size-(read_bytes-SSIZE_MAX)){
			fprintf(stderr, "error: read returned unexpected read count\n");
			exit(EXIT_FAILURE);
		}
	}else{
		rc=read(d, buf, size);
		if(rc==-1){
			perror("read");
			exit(EXIT_FAILURE);
		}else if((uint64_t)rc!=size){
			fprintf(stderr, "error: read returned unexpected read count\n");
			exit(EXIT_FAILURE);
		}
	}

	return;
}

void read_fb_finalize()
{
	if(close(d)==-1){
		perror("close");
		exit(EXIT_FAILURE);
	}

	return;
}
