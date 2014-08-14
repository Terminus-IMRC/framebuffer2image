#ifndef __READ_FB_H_INCLUDED__
#define __READ_FB_H_INCLUDED__

#ifndef __USE_POSIX
#define __USE_POSIX
#endif /* __USE_POSIX */

#include <stdint.h>
#include <sys/types.h>
#include <linux/fb.h>

	void read_fb_init(char *devname, struct fb_var_screeninfo *sc, uint8_t *effective_bytes_per_pixel, uint64_t *size);
	void read_fb(uint8_t *buf, uint32_t size);
	void read_fb_finalize();

#endif /* __READ_FB_H_INCLUDED__ */
