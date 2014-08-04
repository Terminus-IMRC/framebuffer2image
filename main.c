#define __USE_POSIX
#include <limits.h>
#include <inttypes.h>
#include <stdint.h>
#include <getopt.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <png.h>

#define DEFAULT_FRAMEBUFFER_DEVICE "/dev/fb0"

enum image_type{
	IT_PNG, IT_JPEG
};

void usage(char *progname, FILE *f);
void print_sc(struct fb_var_screeninfo sc);

int main(int argc, char *argv[])
{
	int d;
	int opt;
	char *dev;
	_Bool dev_set=0;
	enum image_type type;
	char *type_str;
	_Bool type_set=0;
	struct fb_var_screeninfo sc;
	uint32_t effective_bytes_per_pixel;
	uint64_t size;
	void *buf;
	ssize_t rc;
	int png_colortype;

	dev=(char*)malloc((_POSIX_PATH_MAX+1)*sizeof(char));
	if(dev==NULL){
		fprintf(stderr, "error: failed to malloc dev\n");
		exit(EXIT_FAILURE);
	}

	type_str=(char*)malloc((4+1)*sizeof(char));
	if(type_str==NULL){
		fprintf(stderr, "error: failed to malloc type_str\n");
		exit(EXIT_FAILURE);
	}

	while((opt=getopt(argc, argv, "d:t:h"))!=-1){
		switch(opt){
			case 'd':
				if(dev_set){
					fprintf(stderr, "error: framebuffer device option is specified more than once\n");
					exit(EXIT_FAILURE);
				}
				if(strlen(optarg)>_POSIX_PATH_MAX){
					fprintf(stderr, "error: framebuffer device path name is too long\n");
					exit(EXIT_FAILURE);
				}
				strncpy(dev, optarg, _POSIX_PATH_MAX+1);
				dev_set=!0;
				break;
			case 't':
				if(type_set){
					fprintf(stderr, "error: output image type option is specified more than once\n");
					exit(EXIT_FAILURE);
				}
				if(strlen(optarg)>4){
					fprintf(stderr, "error: output image type name is too long\n");
					exit(EXIT_FAILURE);
				}
				strncpy(type_str, optarg, 4+1);
				if(!strcasecmp("png", type_str))
					type=IT_PNG;
				else if(!strcasecmp("jpg", type_str))
					type=IT_JPEG;
				else if(!strcasecmp("jpeg", type_str))
					type=IT_JPEG;
				else{
					fprintf(stderr, "error: unrecognized output image type: %s\n", type_str);
					usage(argv[0], stderr);
					exit(EXIT_FAILURE);
				}
				free(type_str);
				type_set=!0;
				break;
			case 'h':
			default:
				usage(argv[0], stderr);
				exit(EXIT_FAILURE);
		}
	}

	if(optind!=argc){
		fprintf(stderr, "error: extra option specified\n");
		usage(argv[0], stderr);
		exit(EXIT_FAILURE);
	}

	if(!dev_set){
		strncpy(dev, DEFAULT_FRAMEBUFFER_DEVICE, _POSIX_PATH_MAX+1);
		dev_set=!0;
	}

	if(!type_set){
		fprintf(stderr, "error: specify output image type\n");
		exit(EXIT_FAILURE);
	}

	if((d=open(dev, O_RDONLY))==-1){
		perror("open");
		exit(EXIT_FAILURE);
	}

	if(ioctl(d, FBIOGET_VSCREENINFO, &sc)==-1){
		perror("ioctl(..., FBIOGET_VSCREENINFO, ...)");
		exit(EXIT_FAILURE);
	}

	printf("Device: %s\n", dev);
	print_sc(sc);
	if(sc.nonstd){
		fprintf(stderr, "error: %s has non-standard pixel format\n", dev);
		exit(EXIT_FAILURE);
	}

	if(sc.bits_per_pixel>64){
		fprintf(stderr, "error: color bits per pixel is too high\n");
		exit(EXIT_FAILURE);
	}

	effective_bytes_per_pixel=sc.bits_per_pixel%8==0?sc.bits_per_pixel/8:(uint32_t)(sc.bits_per_pixel/8)+1;

	if((uint64_t)sc.xres*sc.yres>(~((uint64_t)0))/effective_bytes_per_pixel){
		fprintf(stderr, "error: the framebuffer resoltion is too high\n");
		exit(EXIT_FAILURE);
	}

	size=effective_bytes_per_pixel*sc.xres*sc.yres;

	buf=(void*)malloc(size);
	if(buf==NULL){
		fprintf(stderr, "error: failed to malloc buf\n");
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

	switch(type){
		case IT_PNG:
			if(sc.grayscale){
				if(sc.transp.length==0)
					png_colortype=PNG_COLOR_TYPE_GRAY;
				else
					png_colortype=PNG_COLOR_TYPE_GRAY_ALPHA;
			}else{
				if(sc.transp.length==0)
					png_colortype=PNG_COLOR_TYPE_RGB;
				else
					png_colortype=PNG_COLOR_TYPE_RGB_ALPHA;
			}

			break;

		default:
			fprintf(stderr, "error: unknown color type (internal error)\n");
			exit(EXIT_FAILURE);
	}

	free(dev);

	if(close(d)==-1){
		perror("close");
		exit(EXIT_FAILURE);
	}

	free(buf);

	return 0;
}

void usage(char *progname, FILE *f)
{
	fprintf(f, "Usage: %s [-d framebuffer_device] -t [png|[jpg|jpeg]]\n", progname);
	fprintf(f, "framebuffer_device is set to %s by default\n", DEFAULT_FRAMEBUFFER_DEVICE);

	return;
}

void print_sc(struct fb_var_screeninfo sc)
{
	printf("xres: %" PRIu32 "\n", sc.xres);
	printf("yres: %" PRIu32 "\n", sc.yres);
	printf("bits_per_pixel: %" PRIu32 "\n", sc.bits_per_pixel);
	printf("grayscale: %" PRIu32 "\n", sc.grayscale);
	if(!sc.grayscale){
		printf("red.offset: %" PRIu32 "\n", sc.red.offset);
		printf("red.length: %" PRIu32 "\n", sc.red.length);
		printf("red.msb_right: %" PRIu32 "\n", sc.red.msb_right);
		printf("green.offset: %" PRIu32 "\n", sc.green.offset);
		printf("green.length: %" PRIu32 "\n", sc.green.length);
		printf("green.msb_right: %" PRIu32 "\n", sc.green.msb_right);
		printf("blue.offset: %" PRIu32 "\n", sc.blue.offset);
		printf("blue.length: %" PRIu32 "\n", sc.blue.length);
		printf("blue.msb_right: %" PRIu32 "\n", sc.blue.msb_right);
		printf("transp.offset: %" PRIu32 "\n", sc.transp.offset);
		printf("transp.length: %" PRIu32 "\n", sc.transp.length);
		printf("transp.msb_right: %" PRIu32 "\n", sc.transp.msb_right);
	}
	printf("nonstd: %" PRIu32 "\n", sc.nonstd);
	printf("rotate: %" PRIu32 "\n", sc.rotate);

	return;
}
