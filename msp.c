#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "writeint.h"

struct mspaint
{
	unsigned char magic[4];
	unsigned short width;
	unsigned short height;
	unsigned short xarbitmap;
	unsigned short yarbitmap;
	unsigned short xarprinter;
	unsigned short yarprinter;
	unsigned short printerwidth;
	unsigned short printerheight;
	unsigned short unused1;
	unsigned short unused2;
	unsigned short checksum;
	unsigned short pad[3];
};

int linebreak = 0;

unsigned short to_short(unsigned char *bytes)
{
	return (bytes[0] | (bytes[1] << 8));
}

void read_bits(unsigned char bits, unsigned char *buf)
{
	int i, j;
	for(j = 0, i = 7; i >= 0; i--)
		buf[j++] = ((bits >> i) & 1) ? 0xff : 0x00;
}

int read_header(FILE *file, struct mspaint *head)
{
	unsigned char buf[32];

	if(fread(buf, 1, 32, file) != 32)
		return -1;

	memcpy(head->magic, buf, 4);
	head->width    = to_short(buf + 4);
	head->height   = to_short(buf + 6);
	head->checksum = to_short(buf + 24);

	return 0;
}

void write_pbm_p1_header(FILE *pbm, int width, int height)
{
	fprintf(pbm, "P1\n%d\n%d\n", width, height);
}

int write_pbm_p1_data(FILE *pbm, unsigned char *buf, int len)
{
	int i;
	linebreak += len;
	for(i = 0; i < len; i++)
		if(buf[i] == 0xff)
			buf[i] = '0';
		else
			buf[i] = '1';
	if(linebreak > 24)
	{
		linebreak = 0;
		fputs("\n", pbm);
	}
	return fwrite(buf, 1, len, pbm);
}

int fwrite_bmp_header(FILE *bmp, long filesize, int width, int height)
{
	fwrite("\x42\x4D", 2, 1, bmp);
	fwrite_uint32_le(filesize, bmp); /* size */
	fwrite("\x00\x00\x00\x00", 4, 1, bmp); /* reserved 1&2 */
	fwrite("\x3e\x00\x00\x00", 4, 1, bmp); /* data offset */
	fwrite("\x28\x00\x00\x00", 4, 1, bmp); /* header size */
	fwrite_uint32_le(width, bmp);
	fwrite_uint32_le(height, bmp);
	fwrite("\x01\x00", 2, 1, bmp);
	fwrite("\x01\x00", 2, 1, bmp); /* 1-bit BMP */
	fwrite("\x00\x00\x00\x00", 4, 1, bmp);
	fwrite_uint32_le(filesize - 62, bmp); /* image size */
	fwrite("\x00\x00\x00\x00", 4, 1, bmp);
	fwrite("\x00\x00\x00\x00", 4, 1, bmp);
	fwrite("\x02\x00\x00\x00", 4, 1, bmp);
	fwrite("\x00\x00\x00\x00", 4, 1, bmp);
	fwrite("\x00\x00\x00\x00", 4, 1, bmp); /* black */
	fwrite("\xFF\xFF\xFF\x00", 4, 1, bmp); /* white */

	return 0;
}

int swap_vertical(unsigned char *inbuf, unsigned char *outbuf, int offset, int width, int height, int bpp)
{
	int j, stride;

	inbuf += offset;
	stride = (((width * bpp) + 7) / 8 + 3) & ~3;
	outbuf += (height - 1) * stride;
	for(j = 0; j < height; j++)
	{
		memcpy(outbuf, inbuf, stride);
		inbuf += stride;
		outbuf -= stride;
	}

	return 0;
}

int check_file_ext(const char *filename, const char *ext, size_t len)
{
	char *dot = strrchr(filename, '.');

	if(!dot || dot == filename)
		return 1;

	if(strlen(dot + 1) != len)
		return 1;

	if(ext)
		if(memcmp(dot + 1, ext, len))
			return 1;

	return 0;
}

int main(int argc, char **argv)
{
	char *p;
	FILE *msp, *out;
	long filesize, l;
	int stride, mapsize;
	struct mspaint head;
	unsigned char *t, *inbuf, *outbuf;

	if(argc < 2)
	{
		fprintf(stderr, "usage: %s file.msp\n", argv[0]);
		return 2;
	}

	msp = fopen(argv[1], "rb");

	if(!msp)
	{
		fprintf(stderr, "failed to open '%s' (%s)\n", argv[1], strerror(errno));
		return 1;
	}

	fseek(msp, 0, SEEK_END);
	filesize = ftell(msp) - sizeof(struct mspaint);
	fseek(msp, 0, SEEK_SET);

	if(read_header(msp, &head) != 0)
	{
		fprintf(stderr, "failed to read header data from file '%s', truncated?\n", argv[1]);
		return 1;
	}

	if((memcmp(head.magic, "\x44\x61", 2)) || check_file_ext(argv[1], NULL, 3))
	{
		fprintf(stderr, "file '%s' is not a valid MSP image\n", argv[1]);
		return 1;
	}

	p = strrchr(argv[1], '.');
	memcpy(p + 1, "bmp", 3);

	out = fopen(argv[1], "wb");

	if(!out)
	{
		fprintf(stderr, "failed to open '%s' (%s)\n", argv[1], strerror(errno));
		return 1;
	}

	stride = ((head.width + 7) / 8 + 3) & ~3;
	mapsize = head.height * stride;
	inbuf = calloc(mapsize, 1);
	outbuf = calloc(mapsize, 1);

	t = inbuf;
	for(l = 0; l < filesize; l++)
	{
		fread(inbuf, 1, head.width / 8, msp);
		inbuf += stride;
	}
	inbuf = t;

	swap_vertical(inbuf, outbuf, 0, head.width, head.height, 1);

	fwrite_bmp_header(out, mapsize, head.width, head.height);
	fwrite(outbuf, 1, mapsize, out);

	fclose(msp);
	fclose(out);
	return 0;
}
