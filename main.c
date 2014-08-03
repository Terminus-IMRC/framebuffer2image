#include <inttypes.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define DEFAULT_FRAMEBUFFER_DEVICE "/dev/fb0"

void usage(char *progname, FILE *f);
void print_sc(struct fb_var_screeninfo sc);

int main(int argc, char *argv[])
{
	int d;
	int opt;
	char *dev=NULL;
	struct fb_var_screeninfo sc;

	while((opt=getopt(argc, argv, "d:h"))!=-1){
		switch(opt){
			case 'd':
				dev=optarg;
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

	if(dev==NULL)
		dev=DEFAULT_FRAMEBUFFER_DEVICE;

	if((d=open(dev, O_RDONLY))==-1){
		perror("open");
		exit(EXIT_FAILURE);
	}

	if(ioctl(d, FBIOGET_VSCREENINFO, &sc)==-1){
		perror("ioctl(..., FBIOGET_VSCREENINFO, ...)");
		exit(EXIT_FAILURE);
	}

	if(close(d)==-1){
		perror("close");
		exit(EXIT_FAILURE);
	}

	printf("Device: %s\n", dev);
	print_sc(sc);
	if(sc.nonstd){
		fprintf(stderr, "error: %s has non-standard pixel format\n", dev);
		exit(EXIT_FAILURE);
	}

	return 0;
}

void usage(char *progname, FILE *f)
{
	fprintf(f, "Usage: %s [-d framebuffer_device]\n", progname);
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
