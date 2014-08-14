#ifndef __ENCODE_PNG_H_INCLUDED__
#define __ENCODE_PNG_H_INCLUDED__

#ifndef __USE_POSIX
#define __USE_POSIX
#endif /* __USE_POSIX */

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
#include <sys/wait.h>
#include <unistd.h>
#include <png.h>
#include "fill_bits.h"

	void encode_png_init(struct fb_var_screeninfo sc, uint8_t fb_effective_bytes_per_pixel_arg);
	uint8_t *encode_png(void *fb_buf_1dim, uint32_t *imagesize);

#endif /* __ENCODE_PNG_H_INCLUDED__ */
