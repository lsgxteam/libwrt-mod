/*
 * Copyright (C) 2009 Gabor Juhos <juhosg@openwrt.org>
 *
 * This tool was based on:
 *   TP-Link WR941 V2 firmware checksum fixing tool.
 *   Copyright (C) 2008,2009 Wang Jian <lark@linux.net.cn>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>     /* for unlink() */
#include <libgen.h>
#include <getopt.h>     /* for getopt() */
#include <stdarg.h>
#include <errno.h>
#include <sys/stat.h>

#include <arpa/inet.h>
#include <netinet/in.h>

#include "md5.h"

#define ALIGN(x,a) ({ typeof(a) __a = (a); (((x) + __a - 1) & ~(__a - 1)); })
#define ARRAY_SIZE(a) (sizeof((a)) / sizeof((a)[0]))

#define HEADER_VERSION_V1	0x01000000
#define HEADER_VERSION_V2	0x02000000

#define MD5SUM_LEN	16

struct file_info {
	char		*file_name;	/* name of the file */
	uint32_t	file_size;	/* length of the file */
};

struct fw_header {
	uint32_t	version;	/* header version */
	char		vendor_name[24];
	char		fw_version[36];
	uint32_t	hw_id;		/* hardware id */
	uint32_t	hw_rev;		/* hardware revision */
	uint32_t	region_code;	/* region code */
	uint8_t		md5sum1[MD5SUM_LEN];
	uint32_t	unk2;
	uint8_t		md5sum2[MD5SUM_LEN];
	uint32_t	unk3;
	uint32_t	kernel_la;	/* kernel load address */
	uint32_t	kernel_ep;	/* kernel entry point */
	uint32_t	fw_length;	/* total length of the firmware */
	uint32_t	kernel_ofs;	/* kernel data offset */
	uint32_t	kernel_len;	/* kernel data length */
	uint32_t	rootfs_ofs;	/* rootfs data offset */
	uint32_t	rootfs_len;	/* rootfs data length */
	uint32_t	boot_ofs;	/* bootloader data offset */
	uint32_t	boot_len;	/* bootloader data length */
	uint16_t	ver_hi;
	uint16_t	ver_mid;
	uint16_t	ver_lo;
	uint8_t		pad[130];
	char		region_str1[32];
	char		region_str2[32];
	uint8_t		pad2[160];
} __attribute__ ((packed));

struct flash_layout {
	char		*id;
	uint32_t	fw_max_len;
	uint32_t	kernel_la;
	uint32_t	kernel_ep;
	uint32_t	rootfs_ofs;
};

struct fw_region {
	char		name[4];
	uint32_t	code;
};


/*
 * Globals
 */
static char *ofname;
static char *progname;
static char *vendor = "TP-LINK Technologies";
static char *version = "ver. 1.0";
static char *fw_ver = "0.0.0";
static uint32_t hdr_ver = HEADER_VERSION_V1;

static char *layout_id;
static struct flash_layout *layout;
static char *opt_hw_id;
static uint32_t hw_id;
static char *opt_hw_rev;
static uint32_t hw_rev;
static uint32_t opt_hdr_ver = 1;
static char *country;
static const struct fw_region *region;
static int fw_ver_lo;
static int fw_ver_mid;
static int fw_ver_hi;
static struct file_info kernel_info;
static uint32_t kernel_la = 0;
static uint32_t kernel_ep = 0;
static uint32_t kernel_len = 0;
static struct file_info rootfs_info;
static uint32_t rootfs_ofs = 0;
static uint32_t rootfs_align;
static struct file_info boot_info;
static int combined;
static int strip_padding;
static int ignore_size;
static int add_jffs2_eof;
static unsigned char jffs2_eof_mark[4] = {0xde, 0xad, 0xc0, 0xde};
static uint32_t fw_max_len;
static uint32_t reserved_space;

static struct file_info inspect_info;
static int extract = 0;

static const char md5salt_normal[MD5SUM_LEN] = {
	0xdc, 0xd7, 0x3a, 0xa5, 0xc3, 0x95, 0x98, 0xfb,
	0xdd, 0xf9, 0xe7, 0xf4, 0x0e, 0xae, 0x47, 0x38,
};

static const char md5salt_boot[MD5SUM_LEN] = {
	0x8c, 0xef, 0x33, 0x5b, 0xd5, 0xc5, 0xce, 0xfa,
	0xa7, 0x9c, 0x28, 0xda, 0xb2, 0xe9, 0x0f, 0x42,
};

static struct flash_layout layouts[] = {
	{
		.id		= "4M",
		.fw_max_len	= 0x3c0000,
		.kernel_la	= 0x80060000,
		.kernel_ep	= 0x80060000,
		.rootfs_ofs	= 0x140000,
	}, {
		.id		= "4Mlzma",
		.fw_max_len	= 0x3c0000,
		.kernel_la	= 0x80060000,
		.kernel_ep	= 0x80060000,
		.rootfs_ofs	= 0x100000,
	}, {
		.id		= "8M",
		.fw_max_len	= 0x7c0000,
		.kernel_la	= 0x80060000,
		.kernel_ep	= 0x80060000,
		.rootfs_ofs	= 0x140000,
	}, {
		.id		= "8Mlzma",
		.fw_max_len	= 0x7c0000,
		.kernel_la	= 0x80060000,
		.kernel_ep	= 0x80060000,
		.rootfs_ofs	= 0x100000,
	}, {
		.id		= "16M",
		.fw_max_len	= 0xf80000,
		.kernel_la	= 0x80060000,
		.kernel_ep	= 0x80060000,
		.rootfs_ofs	= 0x140000,
	}, {
		.id		= "16Mlzma",
		.fw_max_len	= 0xf80000,
		.kernel_la	= 0x80060000,
		.kernel_ep	= 0x80060000,
		.rootfs_ofs	= 0x100000,
	}, {
		.id		= "16Mppc",
		.fw_max_len	= 0xf80000,
		.kernel_la	= 0x00000000 ,
		.kernel_ep	= 0xc0000000,
		.rootfs_ofs	= 0x2a0000,
	}, {
		/* terminating entry */
	}
};

static const struct fw_region regions[] = {
	/* Default region (universal) uses code 0 as well */
	{"US", 1},
	{"EU", 0},
};

/*
 * Message macros
 */
#define ERR(fmt, ...) do { \
	fflush(0); \
	fprintf(stderr, "[%s] *** error: " fmt "\n", \
			progname, ## __VA_ARGS__ ); \
} while (0)

#define ERRS(fmt, ...) do { \
	int save = errno; \
	fflush(0); \
	fprintf(stderr, "[%s] *** error: " fmt ": %s\n", \
			progname, ## __VA_ARGS__, strerror(save)); \
} while (0)

#define DBG(fmt, ...) do { \
	fprintf(stderr, "[%s] " fmt "\n", progname, ## __VA_ARGS__ ); \
} while (0)

static struct flash_layout *find_layout(const char *id)
{
	struct flash_layout *ret;
	struct flash_layout *l;

	ret = NULL;
	for (l = layouts; l->id != NULL; l++){
		if (strcasecmp(id, l->id) == 0) {
			ret = l;
			break;
		}
	};

	return ret;
}

static const struct fw_region * find_region(const char *country) {
	size_t i;

	for (i = 0; i < ARRAY_SIZE(regions); i++) {
		if (strcasecmp(regions[i].name, country) == 0)
			return &regions[i];
	}

	return NULL;
}

static void usage(int status)
{
	fprintf(stderr, "Usage: %s [OPTIONS...]\n", progname);
	fprintf(stderr,
"\n"
"Options:\n"
"  -c              use combined kernel image\n"
"  -E <ep>         overwrite kernel entry point with <ep> (hexval prefixed with 0x)\n"
"  -L <la>         overwrite kernel load address with <la> (hexval prefixed with 0x)\n"
"  -H <hwid>       use hardware id specified with <hwid>\n"
"  -W <hwrev>      use hardware revision specified with <hwrev>\n"
"  -C <country>    set region code to <country>\n"
"  -F <id>         use flash layout specified with <id>\n"
"  -k <file>       read kernel image from the file <file>\n"
"  -r <file>       read rootfs image from the file <file>\n"
"  -a <align>      align the rootfs start on an <align> bytes boundary\n"
"  -R <offset>     overwrite rootfs offset with <offset> (hexval prefixed with 0x)\n"
"  -o <file>       write output to the file <file>\n"
"  -s              strip padding from the end of the image\n"
"  -S              ignore firmware size limit (only for combined images)\n"
"  -j              add jffs2 end-of-filesystem markers\n"
"  -N <vendor>     set image vendor to <vendor>\n"
"  -V <version>    set image version to <version>\n"
"  -v <version>    set firmware version to <version>\n"
"  -m <version>    set header version to <version>\n"
"  -i <file>       inspect given firmware file <file>\n"
"  -x              extract kernel and rootfs while inspecting (requires -i)\n"
"  -X <size>       reserve <size> bytes in the firmware image (hexval prefixed with 0x)\n"
"  -h              show this screen\n"
	);

	exit(status);
}

static void get_md5(const char *data, int size, uint8_t *md5)
{
	MD5_CTX ctx;

	MD5_Init(&ctx);
	MD5_Update(&ctx, data, size);
	MD5_Final(md5, &ctx);
}

static int get_file_stat(struct file_info *fdata)
{
	struct stat st;
	int res;

	if (fdata->file_name == NULL)
		return 0;

	res = stat(fdata->file_name, &st);
	if (res){
		ERRS("stat failed on %s", fdata->file_name);
		return res;
	}

	fdata->file_size = st.st_size;
	return 0;
}

static int read_to_buf(const struct file_info *fdata, char *buf)
{
	FILE *f;
	int ret = EXIT_FAILURE;

	f = fopen(fdata->file_name, "r");
	if (f == NULL) {
		ERRS("could not open \"%s\" for reading", fdata->file_name);
		goto out;
	}

	errno = 0;
	fread(buf, fdata->file_size, 1, f);
	if (errno != 0) {
		ERRS("unable to read from file \"%s\"", fdata->file_name);
		goto out_close;
	}

	ret = EXIT_SUCCESS;

 out_close:
	fclose(f);
 out:
	return ret;
}

static int check_options(void)
{
	int ret;
	int exceed_bytes;

	if (inspect_info.file_name) {
		ret = get_file_stat(&inspect_info);
		if (ret)
			return ret;

		return 0;
	} else if (extract) {
		ERR("no firmware for inspection specified");
		return -1;
	}

	if (opt_hw_id == NULL) {
		ERR("hardware id not specified");
		return -1;
	}
	hw_id = strtoul(opt_hw_id, NULL, 0);

	if (layout_id == NULL) {
		ERR("flash layout is not specified");
		return -1;
	}

	if (opt_hw_rev)
		hw_rev = strtoul(opt_hw_rev, NULL, 0);
	else
		hw_rev = 1;

	if (country) {
		region = find_region(country);
		if (!region) {
			ERR("unknown region code \"%s\"", country);
			return -1;
		}
	}

	layout = find_layout(layout_id);
	if (layout == NULL) {
		ERR("unknown flash layout \"%s\"", layout_id);
		return -1;
	}

	if (!kernel_la)
		kernel_la = layout->kernel_la;
	if (!kernel_ep)
		kernel_ep = layout->kernel_ep;
	if (!rootfs_ofs)
		rootfs_ofs = layout->rootfs_ofs;

	if (reserved_space > layout->fw_max_len) {
		ERR("reserved space is not valid");
		return -1;
	}

	fw_max_len = layout->fw_max_len - reserved_space;

	if (kernel_info.file_name == NULL) {
		ERR("no kernel image specified");
		return -1;
	}

	ret = get_file_stat(&kernel_info);
	if (ret)
		return ret;

	kernel_len = kernel_info.file_size;

	if (combined) {
		exceed_bytes = kernel_info.file_size - (fw_max_len - sizeof(struct fw_header));
		if (exceed_bytes > 0) {
			if (!ignore_size) {
				ERR("kernel image is too big by %i bytes", exceed_bytes);
				return -1;
			}
			layout->fw_max_len = sizeof(struct fw_header) +
					     kernel_info.file_size +
					     reserved_space;
		}
	} else {
		if (rootfs_info.file_name == NULL) {
			ERR("no rootfs image specified");
			return -1;
		}

		ret = get_file_stat(&rootfs_info);
		if (ret)
			return ret;

		if (rootfs_align) {
			kernel_len += sizeof(struct fw_header);
			kernel_len = ALIGN(kernel_len, rootfs_align);
			kernel_len -= sizeof(struct fw_header);

			DBG("kernel length aligned to %u", kernel_len);

			exceed_bytes = kernel_len + rootfs_info.file_size - (fw_max_len - sizeof(struct fw_header));
			if (exceed_bytes > 0) {
				ERR("images are too big by %i bytes", exceed_bytes);
				return -1;
			}
		} else {
			exceed_bytes = kernel_info.file_size - (rootfs_ofs - sizeof(struct fw_header));
			if (exceed_bytes > 0) {
				ERR("kernel image is too big by %i bytes", exceed_bytes);
				return -1;
			}

			exceed_bytes = rootfs_info.file_size - (fw_max_len - rootfs_ofs);
			if (exceed_bytes > 0) {
				ERR("rootfs image is too big by %i bytes", exceed_bytes);
				return -1;
			}
		}
	}

	if (ofname == NULL) {
		ERR("no output file specified");
		return -1;
	}

	ret = sscanf(fw_ver, "%d.%d.%d", &fw_ver_hi, &fw_ver_mid, &fw_ver_lo);
	if (ret != 3) {
		ERR("invalid firmware version '%s'", fw_ver);
		return -1;
	}

	if (opt_hdr_ver == 1) {
		hdr_ver = HEADER_VERSION_V1;
	} else if (opt_hdr_ver == 2) {
		hdr_ver = HEADER_VERSION_V2;
	} else {
		ERR("invalid header version '%u'", opt_hdr_ver);
		return -1;
	}

	return 0;
}

static void fill_header(char *buf, int len)
{
	struct fw_header *hdr = (struct fw_header *)buf;

	memset(hdr, 0, sizeof(struct fw_header));

	hdr->version = htonl(hdr_ver);
	strncpy(hdr->vendor_name, vendor, sizeof(hdr->vendor_name));
	strncpy(hdr->fw_version, version, sizeof(hdr->fw_version));
	hdr->hw_id = htonl(hw_id);
	hdr->hw_rev = htonl(hw_rev);

	if (boot_info.file_size == 0)
		memcpy(hdr->md5sum1, md5salt_normal, sizeof(hdr->md5sum1));
	else
		memcpy(hdr->md5sum1, md5salt_boot, sizeof(hdr->md5sum1));

	hdr->kernel_la = htonl(kernel_la);
	hdr->kernel_ep = htonl(kernel_ep);
	hdr->fw_length = htonl(layout->fw_max_len);
	hdr->kernel_ofs = htonl(sizeof(struct fw_header));
	hdr->kernel_len = htonl(kernel_len);
	if (!combined) {
		hdr->rootfs_ofs = htonl(rootfs_ofs);
		hdr->rootfs_len = htonl(rootfs_info.file_size);
	}

	hdr->ver_hi = htons(fw_ver_hi);
	hdr->ver_mid = htons(fw_ver_mid);
	hdr->ver_lo = htons(fw_ver_lo);

	if (region) {
		hdr->region_code = htonl(region->code);
		snprintf(
			hdr->region_str1, sizeof(hdr->region_str1), "00000000;%02X%02X%02X%02X;",
			region->name[0], region->name[1], region->name[2], region->name[3]
		);
		snprintf(
			hdr->region_str2, sizeof(hdr->region_str2), "%02X%02X%02X%02X",
			region->name[0], region->name[1], region->name[2], region->name[3]
		);
	}

	get_md5(buf, len, hdr->md5sum1);
}

static int pad_jffs2(char *buf, int currlen)
{
	int len;
	uint32_t pad_mask;

	len = currlen;
	pad_mask = (64 * 1024);
	while ((len < layout->fw_max_len) && (pad_mask != 0)) {
		uint32_t mask;
		int i;

		for (i = 10; i < 32; i++) {
			mask = 1 << i;
			if (pad_mask & mask)
				break;
		}

		len = ALIGN(len, mask);

		for (i = 10; i < 32; i++) {
			mask = 1 << i;
			if ((len & (mask - 1)) == 0)
				pad_mask &= ~mask;
		}

		for (i = 0; i < sizeof(jffs2_eof_mark); i++)
			buf[len + i] = jffs2_eof_mark[i];

		len += sizeof(jffs2_eof_mark);
	}

	return len;
}

static int write_fw(const char *data, int len)
{
	FILE *f;
	int ret = EXIT_FAILURE;

	f = fopen(ofname, "w");
	if (f == NULL) {
		ERRS("could not open \"%s\" for writing", ofname);
		goto out;
	}

	errno = 0;
	fwrite(data, len, 1, f);
	if (errno) {
		ERRS("unable to write output file");
		goto out_flush;
	}

	DBG("firmware file \"%s\" completed", ofname);

	ret = EXIT_SUCCESS;

 out_flush:
	fflush(f);
	fclose(f);
	if (ret != EXIT_SUCCESS) {
		unlink(ofname);
	}
 out:
	return ret;
}

static int build_fw(void)
{
	int buflen;
	char *buf;
	char *p;
	int ret = EXIT_FAILURE;
	int writelen = 0;

	buflen = layout->fw_max_len;

	buf = malloc(buflen);
	if (!buf) {
		ERR("no memory for buffer\n");
		goto out;
	}

	memset(buf, 0xff, buflen);
	p = buf + sizeof(struct fw_header);
	ret = read_to_buf(&kernel_info, p);
	if (ret)
		goto out_free_buf;

	writelen = sizeof(struct fw_header) + kernel_len;

	if (!combined) {
		if (rootfs_align)
			p = buf + writelen;
		else
			p = buf + rootfs_ofs;

		ret = read_to_buf(&rootfs_info, p);
		if (ret)
			goto out_free_buf;

		if (rootfs_align)
			writelen += rootfs_info.file_size;
		else
			writelen = rootfs_ofs + rootfs_info.file_size;

		if (add_jffs2_eof)
			writelen = pad_jffs2(buf, writelen);
	}

	if (!strip_padding)
		writelen = buflen;

	fill_header(buf, writelen);
	ret = write_fw(buf, writelen);
	if (ret)
		goto out_free_buf;

	ret = EXIT_SUCCESS;

 out_free_buf:
	free(buf);
 out:
	return ret;
}

/* Helper functions to inspect_fw() representing different output formats */
static inline void inspect_fw_pstr(const char *label, const char *str)
{
	printf("%-23s: %s\n", label, str);
}

static inline void inspect_fw_phex(const char *label, uint32_t val)
{
	printf("%-23s: 0x%08x\n", label, val);
}

static inline void inspect_fw_phexdec(const char *label, uint32_t val)
{
	printf("%-23s: 0x%08x / %8u bytes\n", label, val, val);
}

static inline void inspect_fw_pmd5sum(const char *label, const uint8_t *val, const char *text)
{
	int i;

	printf("%-23s:", label);
	for (i=0; i<MD5SUM_LEN; i++)
		printf(" %02x", val[i]);
	printf(" %s\n", text);
}

static int inspect_fw(void)
{
	char *buf;
	struct fw_header *hdr;
	uint8_t md5sum[MD5SUM_LEN];
	int ret = EXIT_FAILURE;

	buf = malloc(inspect_info.file_size);
	if (!buf) {
		ERR("no memory for buffer!\n");
		goto out;
	}

	ret = read_to_buf(&inspect_info, buf);
	if (ret)
		goto out_free_buf;
	hdr = (struct fw_header *)buf;

	inspect_fw_pstr("File name", inspect_info.file_name);
	inspect_fw_phexdec("File size", inspect_info.file_size);

	if ((ntohl(hdr->version) != HEADER_VERSION_V1) &&
	    (ntohl(hdr->version) != HEADER_VERSION_V2)) {
		ERR("file does not seem to have V1/V2 header!\n");
		goto out_free_buf;
	}

	inspect_fw_phexdec("Version 1 Header size", sizeof(struct fw_header));

	memcpy(md5sum, hdr->md5sum1, sizeof(md5sum));
	if (ntohl(hdr->boot_len) == 0)
		memcpy(hdr->md5sum1, md5salt_normal, sizeof(md5sum));
	else
		memcpy(hdr->md5sum1, md5salt_boot, sizeof(md5sum));
	get_md5(buf, inspect_info.file_size, hdr->md5sum1);

	if (memcmp(md5sum, hdr->md5sum1, sizeof(md5sum))) {
		inspect_fw_pmd5sum("Header MD5Sum1", md5sum, "(*ERROR*)");
		inspect_fw_pmd5sum("          --> expected", hdr->md5sum1, "");
	} else {
		inspect_fw_pmd5sum("Header MD5Sum1", md5sum, "(ok)");
	}
	if (ntohl(hdr->unk2) != 0)
		inspect_fw_phexdec("Unknown value 2", hdr->unk2);
	inspect_fw_pmd5sum("Header MD5Sum2", hdr->md5sum2,
	                   "(purpose yet unknown, unchecked here)");
	if (ntohl(hdr->unk3) != 0)
		inspect_fw_phexdec("Unknown value 3", hdr->unk3);

	printf("\n");

	inspect_fw_pstr("Vendor name", hdr->vendor_name);
	inspect_fw_pstr("Firmware version", hdr->fw_version);
	inspect_fw_phex("Hardware ID", ntohl(hdr->hw_id));
	inspect_fw_phex("Hardware Revision", ntohl(hdr->hw_rev));
	inspect_fw_phex("Region code", ntohl(hdr->region_code));

	printf("\n");

	inspect_fw_phexdec("Kernel data offset",
	                   ntohl(hdr->kernel_ofs));
	inspect_fw_phexdec("Kernel data length",
	                   ntohl(hdr->kernel_len));
	inspect_fw_phex("Kernel load address",
	                ntohl(hdr->kernel_la));
	inspect_fw_phex("Kernel entry point",
	                ntohl(hdr->kernel_ep));
	inspect_fw_phexdec("Rootfs data offset",
	                   ntohl(hdr->rootfs_ofs));
	inspect_fw_phexdec("Rootfs data length",
	                   ntohl(hdr->rootfs_len));
	inspect_fw_phexdec("Boot loader data offset",
	                   ntohl(hdr->boot_ofs));
	inspect_fw_phexdec("Boot loader data length",
	                   ntohl(hdr->boot_len));
	inspect_fw_phexdec("Total firmware length",
	                   ntohl(hdr->fw_length));

	if (extract) {
		FILE *fp;
		char *filename;

		printf("\n");

		filename = malloc(strlen(inspect_info.file_name) + 8);
		sprintf(filename, "%s-kernel", inspect_info.file_name);
		printf("Extracting kernel to \"%s\"...\n", filename);
		fp = fopen(filename, "w");
		if (fp)	{
			if (!fwrite(buf + ntohl(hdr->kernel_ofs),
			            ntohl(hdr->kernel_len), 1, fp)) {
				ERR("error in fwrite(): %s", strerror(errno));
			}
			fclose(fp);
		} else {
			ERR("error in fopen(): %s", strerror(errno));
		}
		free(filename);

		filename = malloc(strlen(inspect_info.file_name) + 8);
		sprintf(filename, "%s-rootfs", inspect_info.file_name);
		printf("Extracting rootfs to \"%s\"...\n", filename);
		fp = fopen(filename, "w");
		if (fp)	{
			if (!fwrite(buf + ntohl(hdr->rootfs_ofs),
			            ntohl(hdr->rootfs_len), 1, fp)) {
				ERR("error in fwrite(): %s", strerror(errno));
			}
			fclose(fp);
		} else {
			ERR("error in fopen(): %s", strerror(errno));
		}
		free(filename);
	}

 out_free_buf:
	free(buf);
 out:
	return ret;
}

int main(int argc, char *argv[])
{
	int ret = EXIT_FAILURE;

	progname = basename(argv[0]);

	while ( 1 ) {
		int c;

		c = getopt(argc, argv, "a:H:E:F:L:m:V:N:W:C:ci:k:r:R:o:xX:hsSjv:");
		if (c == -1)
			break;

		switch (c) {
		case 'a':
			sscanf(optarg, "0x%x", &rootfs_align);
			break;
		case 'H':
			opt_hw_id = optarg;
			break;
		case 'E':
			sscanf(optarg, "0x%x", &kernel_ep);
			break;
		case 'F':
			layout_id = optarg;
			break;
		case 'W':
			opt_hw_rev = optarg;
			break;
		case 'C':
			country = optarg;
			break;
		case 'L':
			sscanf(optarg, "0x%x", &kernel_la);
			break;
		case 'm':
			sscanf(optarg, "%u", &opt_hdr_ver);
			break;
		case 'V':
			version = optarg;
			break;
		case 'v':
			fw_ver = optarg;
			break;
		case 'N':
			vendor = optarg;
			break;
		case 'c':
			combined++;
			break;
		case 'k':
			kernel_info.file_name = optarg;
			break;
		case 'r':
			rootfs_info.file_name = optarg;
			break;
		case 'R':
			sscanf(optarg, "0x%x", &rootfs_ofs);
			break;
		case 'o':
			ofname = optarg;
			break;
		case 's':
			strip_padding = 1;
			break;
		case 'S':
			ignore_size = 1;
			break;
		case 'i':
			inspect_info.file_name = optarg;
			break;
		case 'j':
			add_jffs2_eof = 1;
			break;
		case 'x':
			extract = 1;
			break;
		case 'h':
			usage(EXIT_SUCCESS);
			break;
		case 'X':
			sscanf(optarg, "0x%x", &reserved_space);
			break;
		default:
			usage(EXIT_FAILURE);
			break;
		}
	}

	ret = check_options();
	if (ret)
		goto out;

	if (!inspect_info.file_name)
		ret = build_fw();
	else
		ret = inspect_fw();

 out:
	return ret;
}
