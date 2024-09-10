#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "writeint.h"

struct mspaint
{
	unsigned short magic1;
	unsigned short magic2;
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

	head->magic1   = to_short(buf);
	head->magic2   = to_short(buf + 2);
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

int write_bmp_header(FILE *bmp, long filesize, int width, int height)
{
	fwrite("\x42\x4D", 2, 1, bmp);
	fwrite_uint32_le(filesize, bmp); /* size */
	fwrite("\x00\x00\x00\x00", 4, 1, bmp); /* reserved 1&2 */
	fwrite("\x3e\x00\x00\x00", 4, 1, bmp); /* data offset */
	fwrite("\x28\x00\x00\x00", 4, 1, bmp); /* header size */
	fwrite_uint32_le(width, bmp);
	fwrite_int32_le(-height, bmp); /* yes, yes, really. */
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

int main(int argc, char **argv)
{
	int i;
	char *p;
	FILE *msp, *out;
	long filesize, l;
	struct mspaint *head;
	int written, pads_written, pad, line;

	if(argc < 2)
	{
		fprintf(stderr, "usage: %s file.msp", argv[0]);
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

	head = malloc(sizeof(struct mspaint));

	if(!head)
	{
		fprintf(stderr, "out of memory\n");
		return 1;
	}

	read_header(msp, head);

	p = strrchr(argv[1], '.');
	memcpy(p + 1, "bmp", 3);

	out = fopen(argv[1], "wb");

	if(!out)
	{
		fprintf(stderr, "failed to open '%s' (%s)\n", argv[1], strerror(errno));
		return 1;
	}

	line = head->width / 8;
	pad = line % 4;

	fseek(out, 62, SEEK_SET);

	written = 0;
	pads_written = 0;
	for(l = 0; l < filesize; l++)
	{
		if(written == line && pad != 0)
		{
			written = 0;
			pads_written += pad;
			for(i = 0; i < pad; i++)
				fputc('\x00', out);
		}
		fputc(fgetc(msp), out);
		written++;
	}

	pad = (filesize + pads_written) % 4;

	if(pad)
		for(i = 0; i < pad; i++)
			fputc('\x00', out);

	/* write BMP header */
	filesize = ftell(out);
	fseek(out, 0, SEEK_SET);

	write_bmp_header(out, filesize, head->width, head->height);

	/*len = head->width * head->height / 8;

	out = fopen("TEST1.PBM", "wb");

	write_pbm_p1_header(out, head->width, head->height);
	while(len--)
	{
		unsigned char buf[8];
		unsigned char *pbuf = buf;

		read_bits(fgetc(msp), pbuf);
		write_pbm_p1_data(out, pbuf, 8);
	}*/

	if(head)
		free(head);

	fclose(msp);
	fclose(out);
	return 0;
}
