#define __USE_POSIX
#include <limits.h>
#include <inttypes.h>
#include <stdint.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <linux/fb.h>
#include <jpeglib.h>
#include "encode_jpeg.h"
#include "fill_bits.h"

struct bitop_procedure_atom{
	uint64_t mask;
	int8_t rshift;
	uint8_t depth;
};

struct bitop_procedure{
	struct bitop_procedure_atom red, green, blue, gray, alpha;
};

J_COLOR_SPACE jpeg_colortype;
int color_components;
int jpeg_effective_bytes_per_pixel_color;
size_t fb_pointer_size, jpeg_pointer_size;
uint32_t width, height;
struct bitop_procedure bp;
uint8_t *retbuf=NULL;
long unsigned int localimagesize;

static uint8_t *encode_jpeg_core(uint8_t **finalbuf, uint32_t *imagesize);

void encode_jpeg_init(struct fb_var_screeninfo sc, int fb_effective_bytes_per_pixel)
{
	width=sc.xres;
	height=sc.yres;
	jpeg_effective_bytes_per_pixel_color=1;

	memset(&bp, 0, sizeof(bp));

	if(sc.grayscale){
		jpeg_colortype=JCS_GRAYSCALE;
		color_components=1;
		if(sc.transp.length==0){
			switch(sc.bits_per_pixel){
				case 1:
				case 2:
				case 4:
				case 8:
				case 16:
					bp.gray.mask=fill_bits(sc.bits_per_pixel);
					bp.gray.depth=sc.bits_per_pixel;
					break;

				default:
					bp.gray.mask=fill_bits(sc.bits_per_pixel);
					if(sc.bits_per_pixel<4)
						bp.gray.depth=4;
					else if(sc.bits_per_pixel<8)
						bp.gray.depth=8;
					else
						bp.gray.depth=16;
					break;
			}
			bp.gray.rshift=sc.bits_per_pixel-jpeg_effective_bytes_per_pixel_color*8;
		}else{
			if((sc.transp.offset!=0)&&(sc.transp.offset+sc.transp.length-1!=sc.bits_per_pixel)){
				fprintf(stderr, "error: gray bit field which is detected by alpha bit field is fragmented\n");
				exit(EXIT_FAILURE);
			}

			fprintf(stderr, "warning: alpha channel of framebuffer is ignored in JPEG output file type (for now?)\n");

			for(bp.gray.depth=sc.bits_per_pixel-sc.transp.length; bp.gray.depth>jpeg_effective_bytes_per_pixel_color*8; bp.gray.depth--)
				;
			for(bp.alpha.depth=sc.transp.length; bp.alpha.depth>jpeg_effective_bytes_per_pixel_color*8; bp.alpha.depth--)
				;

			if(sc.transp.offset==0){
				bp.gray.mask=fill_bits(sc.bits_per_pixel-sc.transp.length)<<sc.transp.length;
				bp.gray.rshift=sc.transp.length+((sc.bits_per_pixel-sc.transp.length)-bp.gray.depth);
				bp.alpha.mask=fill_bits(sc.transp.length);
				bp.alpha.rshift=sc.transp.length-bp.alpha.depth;
			}else if(sc.transp.offset+sc.transp.length-1==sc.bits_per_pixel){
				bp.gray.mask=fill_bits(sc.bits_per_pixel-sc.transp.length);
				bp.gray.rshift=(sc.bits_per_pixel-sc.transp.length)-bp.gray.depth;
				bp.alpha.mask=fill_bits(sc.transp.length)<<(sc.transp.offset-1);
				bp.alpha.rshift=(sc.transp.offset-1)+(sc.transp.length-bp.alpha.depth);
			}
			bp.gray.rshift-=jpeg_effective_bytes_per_pixel_color*8-bp.gray.depth;
			bp.alpha.rshift-=jpeg_effective_bytes_per_pixel_color*8-bp.alpha.depth;
		}
	}else{
		if(sc.transp.length!=0)
			fprintf(stderr, "warning: alpha channel of framebuffer is ignored in JPEG output file type (for now?)\n");

		jpeg_colortype=JCS_RGB;
		color_components=3;

		for(bp.red.depth=sc.red.length; bp.red.depth>jpeg_effective_bytes_per_pixel_color*8; bp.red.depth--)
			;
		for(bp.green.depth=sc.green.length; bp.green.depth>jpeg_effective_bytes_per_pixel_color*8; bp.green.depth--)
			;
		for(bp.blue.depth=sc.blue.length; bp.blue.depth>jpeg_effective_bytes_per_pixel_color*8; bp.blue.depth--)
			;
		if(sc.transp.length!=0)
			for(bp.alpha.depth=sc.transp.length; bp.alpha.depth>jpeg_effective_bytes_per_pixel_color*8; bp.alpha.depth--)
				;

		bp.red.mask=fill_bits(sc.red.length)<<sc.red.offset;
		bp.red.rshift=sc.red.offset+(sc.red.length-bp.red.depth)-(jpeg_effective_bytes_per_pixel_color*8-bp.red.depth);
		bp.green.mask=fill_bits(sc.green.length)<<sc.green.offset;
		bp.green.rshift=sc.green.offset+(sc.green.length-bp.green.depth)-(jpeg_effective_bytes_per_pixel_color*8-bp.green.depth);
		bp.blue.mask=fill_bits(sc.blue.length)<<sc.blue.offset;
		bp.blue.rshift=sc.blue.offset+(sc.blue.length-bp.blue.depth)-(jpeg_effective_bytes_per_pixel_color*8-bp.blue.depth);
		if(sc.transp.length!=0){
			bp.alpha.mask=fill_bits(sc.transp.length)<<sc.transp.offset;
			bp.alpha.rshift=sc.transp.offset+(sc.transp.length-bp.alpha.depth)-(jpeg_effective_bytes_per_pixel_color*8-bp.alpha.depth);
		}
	}

	switch(fb_effective_bytes_per_pixel){
		case 1:
			fb_pointer_size=sizeof(uint8_t*);
			break;

		case 2:
			fb_pointer_size=sizeof(uint16_t*);
			break;

		case 4:
			fb_pointer_size=sizeof(uint32_t*);
			break;

		case 8:
			fb_pointer_size=sizeof(uint64_t*);
			break;

		default:
			fprintf(stderr, "error: unknown fb_effective_bytes_per_pixel (internal error)\n");
			exit(EXIT_FAILURE);
	}

	switch(jpeg_effective_bytes_per_pixel_color){
		case 1:
			jpeg_pointer_size=sizeof(uint8_t*);
			break;

		case 2:
			jpeg_pointer_size=sizeof(uint16_t*);
			break;

		default:
			fprintf(stderr, "error: unknown jpeg_effective_bytes_per_pixel_color (internal error)\n");
			exit(EXIT_FAILURE);
	}

	return;
}

uint8_t *encode_jpeg(uint8_t fb_effective_bytes_per_pixel, void *fbbuf_1dim, uint32_t *imagesize)
{
	uint32_t i, j;
	uint8_t *toret;
	uint8_t **fbbuf_orig;
	uint8_t *finalbuf_1dim, **finalbuf;
	uint8_t **buf_8;
	uint16_t **buf_16;
	uint32_t **buf_32;
	uint64_t **buf_64;

	fbbuf_orig=(uint8_t**)malloc(height*fb_pointer_size);
	if(fbbuf_orig==NULL){
		fprintf(stderr, "error: failed to malloc fbbuf_orig\n");
		exit(EXIT_FAILURE);
	}

	finalbuf_1dim=(uint8_t*)malloc(width*height*color_components*jpeg_effective_bytes_per_pixel_color);
	if(finalbuf_1dim==NULL){
		fprintf(stderr, "error: failed to malloc finalbuf_1dim\n");
		exit(EXIT_FAILURE);
	}

	finalbuf=(uint8_t**)malloc(height*jpeg_pointer_size);
	if(finalbuf==NULL){
		fprintf(stderr, "error: failed to malloc finalbuf\n");
		exit(EXIT_FAILURE);
	}

	for(i=0; i<height; i++){
		fbbuf_orig[i]=(uint8_t*)fbbuf_1dim+(width*fb_effective_bytes_per_pixel)*i;
		finalbuf[i]=(uint8_t*)finalbuf_1dim+(width*color_components*jpeg_effective_bytes_per_pixel_color)*i;
	}

	switch(fb_effective_bytes_per_pixel){
		case 1:
			buf_8=(uint8_t**)fbbuf_orig;
			switch(jpeg_colortype){
				case JCS_GRAYSCALE:
					for(i=0; i<height; i++){
						for(j=0; j<width; j++){
							if(bp.gray.rshift>=0)
								finalbuf[i][j]=(buf_8[i][j]&bp.gray.mask)>>bp.gray.rshift;
							else
								finalbuf[i][j]=(buf_8[i][j]&bp.gray.mask)<<-bp.gray.rshift;
						}
					}
					break;

				case JCS_RGB:
					for(i=0; i<height; i++){
						for(j=0; j<width; j++){
							if(bp.red.rshift>=0)
								finalbuf[i][j*3+0]=(buf_8[i][j]&bp.red.mask)>>bp.red.rshift;
							else
								finalbuf[i][j*3+0]=(buf_8[i][j]&bp.red.mask)<<-bp.red.rshift;
							if(bp.green.rshift>=0)
								finalbuf[i][j*3+1]=(buf_8[i][j]&bp.green.mask)>>bp.green.rshift;
							else
								finalbuf[i][j*3+1]=(buf_8[i][j]&bp.green.mask)<<-bp.green.rshift;
							if(bp.blue.rshift>=0)
								finalbuf[i][j*3+2]=(buf_8[i][j]&bp.blue.mask)>>bp.blue.rshift;
							else
								finalbuf[i][j*3+2]=(buf_8[i][j]&bp.blue.mask)<<-bp.blue.rshift;
						}
					}
					break;

				default:
					fprintf(stderr, "error: unknown jpeg_colortype (internal error)\n");
					exit(EXIT_FAILURE);
			}
			break;

		case 2:
			buf_16=(uint16_t**)fbbuf_orig;
			switch(jpeg_colortype){
				case JCS_GRAYSCALE:
					for(i=0; i<height; i++){
						for(j=0; j<width; j++){
							if(bp.gray.rshift>=0)
								finalbuf[i][j]=(buf_16[i][j]&bp.gray.mask)>>bp.gray.rshift;
							else
								finalbuf[i][j]=(buf_16[i][j]&bp.gray.mask)<<-bp.gray.rshift;
						}
					}
					break;

				case JCS_RGB:
					for(i=0; i<height; i++){
						for(j=0; j<width; j++){
							if(bp.red.rshift>=0)
								finalbuf[i][j*3+0]=(buf_16[i][j]&bp.red.mask)>>bp.red.rshift;
							else
								finalbuf[i][j*3+0]=(buf_16[i][j]&bp.red.mask)<<-bp.red.rshift;
							if(bp.green.rshift>=0)
								finalbuf[i][j*3+1]=(buf_16[i][j]&bp.green.mask)>>bp.green.rshift;
							else
								finalbuf[i][j*3+1]=(buf_16[i][j]&bp.green.mask)<<-bp.green.rshift;
							if(bp.blue.rshift>=0)
								finalbuf[i][j*3+2]=(buf_16[i][j]&bp.blue.mask)>>bp.blue.rshift;
							else
								finalbuf[i][j*3+2]=(buf_16[i][j]&bp.blue.mask)<<-bp.blue.rshift;
						}
					}
					break;

				default:
					fprintf(stderr, "error: unknown jpeg_colortype (internal error)\n");
					exit(EXIT_FAILURE);
			}
			break;

		case 4:
			buf_32=(uint32_t**)fbbuf_orig;
			switch(jpeg_colortype){
				case JCS_GRAYSCALE:
					for(i=0; i<height; i++){
						for(j=0; j<width; j++){
							if(bp.gray.rshift>=0)
								finalbuf[i][j]=(buf_32[i][j]&bp.gray.mask)>>bp.gray.rshift;
							else
								finalbuf[i][j]=(buf_32[i][j]&bp.gray.mask)<<-bp.gray.rshift;
						}
					}
					break;

				case JCS_RGB:
					for(i=0; i<height; i++){
						for(j=0; j<width; j++){
							if(bp.red.rshift>=0)
								finalbuf[i][j*3+0]=(buf_32[i][j]&bp.red.mask)>>bp.red.rshift;
							else
								finalbuf[i][j*3+0]=(buf_32[i][j]&bp.red.mask)<<-bp.red.rshift;
							if(bp.green.rshift>=0)
								finalbuf[i][j*3+1]=(buf_32[i][j]&bp.green.mask)>>bp.green.rshift;
							else
								finalbuf[i][j*3+1]=(buf_32[i][j]&bp.green.mask)<<-bp.green.rshift;
							if(bp.blue.rshift>=0)
								finalbuf[i][j*3+2]=(buf_32[i][j]&bp.blue.mask)>>bp.blue.rshift;
							else
								finalbuf[i][j*3+2]=(buf_32[i][j]&bp.blue.mask)<<-bp.blue.rshift;
						}
					}
					break;

				default:
					fprintf(stderr, "error: unknown jpeg_colortype (internal error)\n");
					exit(EXIT_FAILURE);
			}
			break;

		case 8:
			buf_64=(uint64_t**)fbbuf_orig;
			switch(jpeg_colortype){
				case JCS_GRAYSCALE:
					for(i=0; i<height; i++){
						for(j=0; j<width; j++){
							if(bp.gray.rshift>=0)
								finalbuf[i][j]=(buf_64[i][j]&bp.gray.mask)>>bp.gray.rshift;
							else
								finalbuf[i][j]=(buf_64[i][j]&bp.gray.mask)<<-bp.gray.rshift;
						}
					}
					break;

				case JCS_RGB:
					for(i=0; i<height; i++){
						for(j=0; j<width; j++){
							if(bp.red.rshift>=0)
								finalbuf[i][j*3+0]=(buf_64[i][j]&bp.red.mask)>>bp.red.rshift;
							else
								finalbuf[i][j*3+0]=(buf_64[i][j]&bp.red.mask)<<-bp.red.rshift;
							if(bp.green.rshift>=0)
								finalbuf[i][j*3+1]=(buf_64[i][j]&bp.green.mask)>>bp.green.rshift;
							else
								finalbuf[i][j*3+1]=(buf_64[i][j]&bp.green.mask)<<-bp.green.rshift;
							if(bp.blue.rshift>=0)
								finalbuf[i][j*3+2]=(buf_64[i][j]&bp.blue.mask)>>bp.blue.rshift;
							else
								finalbuf[i][j*3+2]=(buf_64[i][j]&bp.blue.mask)<<-bp.blue.rshift;
						}
					}
					break;

				default:
					fprintf(stderr, "error: unknown jpeg_colortype (internal error)\n");
					exit(EXIT_FAILURE);
			}
			break;

		default:
			fprintf(stderr, "error: unknown fb_effective_bytes_per_pixel (internal error)\n");
			exit(EXIT_FAILURE);
	}

	free(fbbuf_orig);

	toret=encode_jpeg_core(finalbuf, imagesize);

	free(finalbuf);
	free(finalbuf_1dim);

	return toret;
}

void encode_jpeg_finalize()
{
	free(retbuf);

	return;
}

uint8_t *encode_jpeg_core(uint8_t **finalbuf, uint32_t *imagesize)
{
	struct jpeg_compress_struct cinfo;
	struct jpeg_error_mgr jerr;

	cinfo.err=jpeg_std_error(&jerr);
	jpeg_create_compress(&cinfo);
	jpeg_mem_dest(&cinfo, &retbuf, &localimagesize);

	cinfo.image_width=width;
	cinfo.image_height=height;
	cinfo.input_components=color_components;
	cinfo.in_color_space=jpeg_colortype;
	jpeg_set_defaults(&cinfo);
	jpeg_set_quality(&cinfo, 75, !0);

	jpeg_start_compress(&cinfo, !0);
	jpeg_write_scanlines(&cinfo, finalbuf, height);
	jpeg_finish_compress(&cinfo);

	jpeg_destroy_compress(&cinfo);

	*imagesize=localimagesize;

	return retbuf;
}
