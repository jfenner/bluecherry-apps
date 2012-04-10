/*
 * Copyright (C) 2010 Bluecherry, LLC
 *
 * Confidential, all rights reserved. No distribution is permitted.
 */

#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ether.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <libbluecherry.h>

#define BC_KEY_LEN		10
#define BC_KEY_MAGIC		0xBC

struct bc_key_ctx {
	int idx;
	unsigned char bytes[BC_KEY_LEN];
};

/* Our super secret passcode, used for license key codec */
static unsigned char segment_end[BC_KEY_LEN] =
	{ 0x32, 0x14, 0xfe, 0xed, 0xf0, 0x0c, 0x43, 0x25, 0xf4, 0x27 };


static unsigned short const crc16_table[256] = {
	0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
	0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
	0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
	0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
	0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
	0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
	0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
	0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
	0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
	0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
	0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
	0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
	0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
	0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
	0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
	0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
	0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
	0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
	0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
	0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
	0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
	0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
	0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
	0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
	0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
	0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
	0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
	0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
	0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
	0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
	0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
	0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040
};

static unsigned short crc16_byte(unsigned short crc, const unsigned char data)
{
	return (crc >> 8) ^ crc16_table[(crc ^ data) & 0xff];
}


static unsigned short crc16(unsigned char const *buf, size_t len)
{
	unsigned short crc = 0;

	while (len--)
		crc = crc16_byte(crc, *buf++);

	return crc;
}

static unsigned int bc_key_pullbits(struct bc_key_ctx *ctx, int len);

static int bc_key_start(struct bc_key_ctx *ctx, unsigned char *key)
{
	int i;
	unsigned short crc;

	ctx->idx = 0;

	/* Copy the current key, and transform it with passcode */
	for (i = BC_KEY_LEN - 1; i >= 0; i--) {
		unsigned short j = key[i];

		/* If this byte is too small, carry one from next byte */
		if (j < segment_end[i]) {
			j += 0x100;
			if (i < BC_KEY_LEN - 1)
				ctx->bytes[i + 1]--;
		}

		ctx->bytes[i] = j - segment_end[i];
	}

	/* Now check the crc */
	crc = bc_key_pullbits(ctx, 16);
	if (crc != crc16(ctx->bytes, BC_KEY_LEN))
		return EINVAL;

	return 0;
}

static unsigned int bc_key_pullbits(struct bc_key_ctx *ctx, int len)
{
	unsigned int bits = 0;
	int i;

	for (i = 0; i < len; i++) {
		bits <<= 1;
		bits |= ctx->bytes[ctx->idx] & 0x1;
		ctx->bytes[ctx->idx] >>= 1;
		ctx->idx = (ctx->idx + 1) % BC_KEY_LEN;
	}

	return bits;
}

/* Returns errno or 0 for success */
int bc_key_process(struct bc_key_data *res, char *str)
{
	unsigned char key[BC_KEY_LEN];
	struct bc_key_ctx c;
	int i;
	int len;

	memset(key, 0, sizeof(key));

	for (i = len = 0; str[i] != '\0'; i++) {
		unsigned char t;

		/* Means incoming string is too long */
		if (len >= BC_KEY_LEN * 2)
			return 0;

		/* If it's valid hex, process it */
		if (str[i] >= 'a' && str[i] <= 'f')
			t = (str[i] - 'a') + 0x0a;
		else if (str[i] >= 'A' && str[i] <= 'F')
			t = (str[i] - 'A') + 0x0a;
		else if (str[i] >= '0' && str[i] <= '9')
			t = str[i] - '0';
		else if (str[i] == '-')
			continue;
		else
			return EINVAL;

		/* Valid character and value now */
		if (!(len & 0x1))
			t <<= 4;

		key[len / 2] |= t;
		len++;
	}

	/* Too few characters */
	if (len != BC_KEY_LEN * 2)
		return EINVAL;

	if (bc_key_start(&c, key))
		return EINVAL;

	/* ORDER MATTERS HERE! */
	if (bc_key_pullbits(&c, 8) != BC_KEY_MAGIC)
		return EINVAL;

	res->major = bc_key_pullbits(&c, 4);
	res->minor = bc_key_pullbits(&c, 4);

	/* We don't know this license format yet */
	if (res->major != 2 || res->minor != 1)
		return EINVAL;

	res->type = (bc_key_type) bc_key_pullbits(&c, 4);
	if (res->type == BC_KEY_TYPE_CAMERA) {
		bc_key_pullbits(&c, 7);
		res->eval_period = 0;
	} else if (res->type == BC_KEY_TYPE_CAMERA_EVAL) {
		res->eval_period = bc_key_pullbits(&c, 7);
	} else {
		return EINVAL;
	}

	res->count = bc_key_pullbits(&c, 5);
	res->id = bc_key_pullbits(&c, 32);

	return 0;
}

int bc_license_machine_id(char *out, int out_sz)
{
	char buf[1024];
	char id_buf[6];
	struct ifconf ifc;
	struct ifreq *ifr;
	int fd;
	int re = -1;
	int if_count;
	int i;

	if (out_sz < 13)
		return -ENOBUFS;

	memset(id_buf, 0, sizeof(id_buf));

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0)
		return -errno;

	memset(&ifc, 0, sizeof(ifc));
	ifc.ifc_len = sizeof(buf);
	ifc.ifc_buf = buf;

	if (ioctl(fd, SIOCGIFCONF, &ifc) < 0) {
		re = -errno;
		goto end;
	}

	ifr = ifc.ifc_req;
	if_count = ifc.ifc_len / sizeof(struct ifreq);

	for (i = 0; i < if_count; ++i) {
		if (ioctl(fd, SIOCGIFHWADDR, &ifr[i]) < 0) {
			re = -errno;
			continue;
		}

		if (i && strncmp(ifr[i].ifr_name, "eth", 3) && strncmp(ifr[i].ifr_name, "wlan", 4))
			continue;

		memcpy(id_buf, ifr[i].ifr_hwaddr.sa_data, 6);
		if (i)
			break;
	}

	hex_encode(out, 13, id_buf, 6);
	re = 12;

end:
	close(fd);
	return re;
}

