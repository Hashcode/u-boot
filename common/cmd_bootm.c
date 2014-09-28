/*
 * (C) Copyright 2000-2009
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

/*
 * Boot support
 */
#include <common.h>
#include <bootm.h>
#include <command.h>
#include <environment.h>
#include <image.h>
#include <lmb.h>
#include <malloc.h>
#include <nand.h>
#include <asm/byteorder.h>
#include <linux/compiler.h>
#include <linux/ctype.h>
#include <linux/err.h>
#include <u-boot/zlib.h>
#ifdef CONFIG_CMD_BOOTAI
#include <android_image.h>
#include <part.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

#if defined(CONFIG_CMD_IMI)
static int image_info(unsigned long addr);
#endif

#if defined(CONFIG_CMD_IMLS)
#include <flash.h>
#include <mtd/cfi_flash.h>
extern flash_info_t flash_info[]; /* info for FLASH chips */
#endif

#if defined(CONFIG_CMD_IMLS) || defined(CONFIG_CMD_IMLS_NAND)
static int do_imls(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);
#endif

bootm_headers_t images;		/* pointers to os/initrd/fdt images */

/* we overload the cmd field with our state machine info instead of a
 * function pointer */
static cmd_tbl_t cmd_bootm_sub[] = {
	U_BOOT_CMD_MKENT(start, 0, 1, (void *)BOOTM_STATE_START, "", ""),
	U_BOOT_CMD_MKENT(loados, 0, 1, (void *)BOOTM_STATE_LOADOS, "", ""),
#ifdef CONFIG_SYS_BOOT_RAMDISK_HIGH
	U_BOOT_CMD_MKENT(ramdisk, 0, 1, (void *)BOOTM_STATE_RAMDISK, "", ""),
#endif
#ifdef CONFIG_OF_LIBFDT
	U_BOOT_CMD_MKENT(fdt, 0, 1, (void *)BOOTM_STATE_FDT, "", ""),
#endif
	U_BOOT_CMD_MKENT(cmdline, 0, 1, (void *)BOOTM_STATE_OS_CMDLINE, "", ""),
	U_BOOT_CMD_MKENT(bdt, 0, 1, (void *)BOOTM_STATE_OS_BD_T, "", ""),
	U_BOOT_CMD_MKENT(prep, 0, 1, (void *)BOOTM_STATE_OS_PREP, "", ""),
	U_BOOT_CMD_MKENT(fake, 0, 1, (void *)BOOTM_STATE_OS_FAKE_GO, "", ""),
	U_BOOT_CMD_MKENT(go, 0, 1, (void *)BOOTM_STATE_OS_GO, "", ""),
};

static int do_bootm_subcommand(cmd_tbl_t *cmdtp, int flag, int argc,
			char * const argv[])
{
	int ret = 0;
	long state;
	cmd_tbl_t *c;

	c = find_cmd_tbl(argv[0], &cmd_bootm_sub[0], ARRAY_SIZE(cmd_bootm_sub));
	argc--; argv++;

	if (c) {
		state = (long)c->cmd;
		if (state == BOOTM_STATE_START)
			state |= BOOTM_STATE_FINDOS | BOOTM_STATE_FINDOTHER;
	} else {
		/* Unrecognized command */
		return CMD_RET_USAGE;
	}

	if (state != BOOTM_STATE_START && images.state >= state) {
		printf("Trying to execute a command out of order\n");
		return CMD_RET_USAGE;
	}

	ret = do_bootm_states(cmdtp, flag, argc, argv, state, &images, 0);

	return ret;
}

/*******************************************************************/
/* bootm - boot application image from image in memory */
/*******************************************************************/

int do_bootm(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
#ifdef CONFIG_NEEDS_MANUAL_RELOC
	static int relocated = 0;

	if (!relocated) {
		int i;

		/* relocate names of sub-command table */
		for (i = 0; i < ARRAY_SIZE(cmd_bootm_sub); i++)
			cmd_bootm_sub[i].name += gd->reloc_off;

		relocated = 1;
	}
#endif

	/* determine if we have a sub command */
	argc--; argv++;
	if (argc > 0) {
		char *endp;

		simple_strtoul(argv[0], &endp, 16);
		/* endp pointing to NULL means that argv[0] was just a
		 * valid number, pass it along to the normal bootm processing
		 *
		 * If endp is ':' or '#' assume a FIT identifier so pass
		 * along for normal processing.
		 *
		 * Right now we assume the first arg should never be '-'
		 */
		if ((*endp != 0) && (*endp != ':') && (*endp != '#'))
			return do_bootm_subcommand(cmdtp, flag, argc, argv);
	}

	return do_bootm_states(cmdtp, flag, argc, argv, BOOTM_STATE_START |
		BOOTM_STATE_FINDOS | BOOTM_STATE_FINDOTHER |
		BOOTM_STATE_LOADOS |
#if defined(CONFIG_PPC) || defined(CONFIG_MIPS)
		BOOTM_STATE_OS_CMDLINE |
#endif
		BOOTM_STATE_OS_PREP | BOOTM_STATE_OS_FAKE_GO |
		BOOTM_STATE_OS_GO, &images, 1);
}

int bootm_maybe_autostart(cmd_tbl_t *cmdtp, const char *cmd)
{
	const char *ep = getenv("autostart");

	if (ep && !strcmp(ep, "yes")) {
		char *local_args[2];
		local_args[0] = (char *)cmd;
		local_args[1] = NULL;
		printf("Automatic boot of image at addr 0x%08lX ...\n", load_addr);
		return do_bootm(cmdtp, 0, 1, local_args);
	}

	return 0;
}

#ifdef CONFIG_SYS_LONGHELP
static char bootm_help_text[] =
	"[addr [arg ...]]\n    - boot application image stored in memory\n"
	"\tpassing arguments 'arg ...'; when booting a Linux kernel,\n"
	"\t'arg' can be the address of an initrd image\n"
#if defined(CONFIG_OF_LIBFDT)
	"\tWhen booting a Linux kernel which requires a flat device-tree\n"
	"\ta third argument is required which is the address of the\n"
	"\tdevice-tree blob. To boot that kernel without an initrd image,\n"
	"\tuse a '-' for the second argument. If you do not pass a third\n"
	"\ta bd_info struct will be passed instead\n"
#endif
#if defined(CONFIG_FIT)
	"\t\nFor the new multi component uImage format (FIT) addresses\n"
	"\tmust be extened to include component or configuration unit name:\n"
	"\taddr:<subimg_uname> - direct component image specification\n"
	"\taddr#<conf_uname>   - configuration specification\n"
	"\tUse iminfo command to get the list of existing component\n"
	"\timages and configurations.\n"
#endif
	"\nSub-commands to do part of the bootm sequence.  The sub-commands "
	"must be\n"
	"issued in the order below (it's ok to not issue all sub-commands):\n"
	"\tstart [addr [arg ...]]\n"
	"\tloados  - load OS image\n"
#if defined(CONFIG_SYS_BOOT_RAMDISK_HIGH)
	"\tramdisk - relocate initrd, set env initrd_start/initrd_end\n"
#endif
#if defined(CONFIG_OF_LIBFDT)
	"\tfdt     - relocate flat device tree\n"
#endif
	"\tcmdline - OS specific command line processing/setup\n"
	"\tbdt     - OS specific bd_t processing\n"
	"\tprep    - OS specific prep before relocation or go\n"
	"\tgo      - start OS";
#endif

U_BOOT_CMD(
	bootm,	CONFIG_SYS_MAXARGS,	1,	do_bootm,
	"boot application image from memory", bootm_help_text
);

/*******************************************************************/
/* bootd - boot default image */
/*******************************************************************/
#if defined(CONFIG_CMD_BOOTD)
int do_bootd(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	return run_command(getenv("bootcmd"), flag);
}

U_BOOT_CMD(
	boot,	1,	1,	do_bootd,
	"boot default, i.e., run 'bootcmd'",
	""
);

/* keep old command name "bootd" for backward compatibility */
U_BOOT_CMD(
	bootd, 1,	1,	do_bootd,
	"boot default, i.e., run 'bootcmd'",
	""
);

#endif


/*******************************************************************/
/* iminfo - print header info for a requested image */
/*******************************************************************/
#if defined(CONFIG_CMD_IMI)
static int do_iminfo(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int	arg;
	ulong	addr;
	int	rcode = 0;

	if (argc < 2) {
		return image_info(load_addr);
	}

	for (arg = 1; arg < argc; ++arg) {
		addr = simple_strtoul(argv[arg], NULL, 16);
		if (image_info(addr) != 0)
			rcode = 1;
	}
	return rcode;
}

static int image_info(ulong addr)
{
	void *hdr = (void *)addr;

	printf("\n## Checking Image at %08lx ...\n", addr);

	switch (genimg_get_format(hdr)) {
#if defined(CONFIG_IMAGE_FORMAT_LEGACY)
	case IMAGE_FORMAT_LEGACY:
		puts("   Legacy image found\n");
		if (!image_check_magic(hdr)) {
			puts("   Bad Magic Number\n");
			return 1;
		}

		if (!image_check_hcrc(hdr)) {
			puts("   Bad Header Checksum\n");
			return 1;
		}

		image_print_contents(hdr);

		puts("   Verifying Checksum ... ");
		if (!image_check_dcrc(hdr)) {
			puts("   Bad Data CRC\n");
			return 1;
		}
		puts("OK\n");
		return 0;
#endif
#if defined(CONFIG_FIT)
	case IMAGE_FORMAT_FIT:
		puts("   FIT image found\n");

		if (!fit_check_format(hdr)) {
			puts("Bad FIT image format!\n");
			return 1;
		}

		fit_print_contents(hdr);

		if (!fit_all_image_verify(hdr)) {
			puts("Bad hash in FIT image!\n");
			return 1;
		}

		return 0;
#endif
	default:
		puts("Unknown image format!\n");
		break;
	}

	return 1;
}

U_BOOT_CMD(
	iminfo,	CONFIG_SYS_MAXARGS,	1,	do_iminfo,
	"print header information for application image",
	"addr [addr ...]\n"
	"    - print header information for application image starting at\n"
	"      address 'addr' in memory; this includes verification of the\n"
	"      image contents (magic number, header and payload checksums)"
);
#endif


/*******************************************************************/
/* imls - list all images found in flash */
/*******************************************************************/
#if defined(CONFIG_CMD_IMLS)
static int do_imls_nor(void)
{
	flash_info_t *info;
	int i, j;
	void *hdr;

	for (i = 0, info = &flash_info[0];
		i < CONFIG_SYS_MAX_FLASH_BANKS; ++i, ++info) {

		if (info->flash_id == FLASH_UNKNOWN)
			goto next_bank;
		for (j = 0; j < info->sector_count; ++j) {

			hdr = (void *)info->start[j];
			if (!hdr)
				goto next_sector;

			switch (genimg_get_format(hdr)) {
#if defined(CONFIG_IMAGE_FORMAT_LEGACY)
			case IMAGE_FORMAT_LEGACY:
				if (!image_check_hcrc(hdr))
					goto next_sector;

				printf("Legacy Image at %08lX:\n", (ulong)hdr);
				image_print_contents(hdr);

				puts("   Verifying Checksum ... ");
				if (!image_check_dcrc(hdr)) {
					puts("Bad Data CRC\n");
				} else {
					puts("OK\n");
				}
				break;
#endif
#if defined(CONFIG_FIT)
			case IMAGE_FORMAT_FIT:
				if (!fit_check_format(hdr))
					goto next_sector;

				printf("FIT Image at %08lX:\n", (ulong)hdr);
				fit_print_contents(hdr);
				break;
#endif
			default:
				goto next_sector;
			}

next_sector:		;
		}
next_bank:	;
	}
	return 0;
}
#endif

#if defined(CONFIG_CMD_IMLS_NAND)
static int nand_imls_legacyimage(nand_info_t *nand, int nand_dev, loff_t off,
		size_t len)
{
	void *imgdata;
	int ret;

	imgdata = malloc(len);
	if (!imgdata) {
		printf("May be a Legacy Image at NAND device %d offset %08llX:\n",
				nand_dev, off);
		printf("   Low memory(cannot allocate memory for image)\n");
		return -ENOMEM;
	}

	ret = nand_read_skip_bad(nand, off, &len,
			imgdata);
	if (ret < 0 && ret != -EUCLEAN) {
		free(imgdata);
		return ret;
	}

	if (!image_check_hcrc(imgdata)) {
		free(imgdata);
		return 0;
	}

	printf("Legacy Image at NAND device %d offset %08llX:\n",
			nand_dev, off);
	image_print_contents(imgdata);

	puts("   Verifying Checksum ... ");
	if (!image_check_dcrc(imgdata))
		puts("Bad Data CRC\n");
	else
		puts("OK\n");

	free(imgdata);

	return 0;
}

static int nand_imls_fitimage(nand_info_t *nand, int nand_dev, loff_t off,
		size_t len)
{
	void *imgdata;
	int ret;

	imgdata = malloc(len);
	if (!imgdata) {
		printf("May be a FIT Image at NAND device %d offset %08llX:\n",
				nand_dev, off);
		printf("   Low memory(cannot allocate memory for image)\n");
		return -ENOMEM;
	}

	ret = nand_read_skip_bad(nand, off, &len,
			imgdata);
	if (ret < 0 && ret != -EUCLEAN) {
		free(imgdata);
		return ret;
	}

	if (!fit_check_format(imgdata)) {
		free(imgdata);
		return 0;
	}

	printf("FIT Image at NAND device %d offset %08llX:\n", nand_dev, off);

	fit_print_contents(imgdata);
	free(imgdata);

	return 0;
}

static int do_imls_nand(void)
{
	nand_info_t *nand;
	int nand_dev = nand_curr_device;
	size_t len;
	loff_t off;
	u32 buffer[16];

	if (nand_dev < 0 || nand_dev >= CONFIG_SYS_MAX_NAND_DEVICE) {
		puts("\nNo NAND devices available\n");
		return -ENODEV;
	}

	printf("\n");

	for (nand_dev = 0; nand_dev < CONFIG_SYS_MAX_NAND_DEVICE; nand_dev++) {
		nand = &nand_info[nand_dev];
		if (!nand->name || !nand->size)
			continue;

		for (off = 0; off < nand->size; off += nand->erasesize) {
			const image_header_t *header;
			int ret;

			if (nand_block_isbad(nand, off))
				continue;

			len = sizeof(buffer);

			ret = nand_read(nand, off, &len, (u8 *)buffer);
			if (ret < 0 && ret != -EUCLEAN) {
				printf("NAND read error %d at offset %08llX\n",
						ret, off);
				continue;
			}

			switch (genimg_get_format(buffer)) {
#if defined(CONFIG_IMAGE_FORMAT_LEGACY)
			case IMAGE_FORMAT_LEGACY:
				header = (const image_header_t *)buffer;

				len = image_get_image_size(header);
				nand_imls_legacyimage(nand, nand_dev, off, len);
				break;
#endif
#if defined(CONFIG_FIT)
			case IMAGE_FORMAT_FIT:
				len = fit_get_size(buffer);
				nand_imls_fitimage(nand, nand_dev, off, len);
				break;
#endif
			}
		}
	}

	return 0;
}
#endif

#if defined(CONFIG_CMD_IMLS) || defined(CONFIG_CMD_IMLS_NAND)
static int do_imls(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int ret_nor = 0, ret_nand = 0;

#if defined(CONFIG_CMD_IMLS)
	ret_nor = do_imls_nor();
#endif

#if defined(CONFIG_CMD_IMLS_NAND)
	ret_nand = do_imls_nand();
#endif

	if (ret_nor)
		return ret_nor;

	if (ret_nand)
		return ret_nand;

	return (0);
}

U_BOOT_CMD(
	imls,	1,		1,	do_imls,
	"list all images found in flash",
	"\n"
	"    - Prints information about all images found at sector/block\n"
	"      boundaries in nor/nand flash."
);
#endif

#ifdef CONFIG_CMD_BOOTZ

int __weak bootz_setup(ulong image, ulong *start, ulong *end)
{
	/* Please define bootz_setup() for your platform */

	puts("Your platform's zImage format isn't supported yet!\n");
	return -1;
}

/*
 * zImage booting support
 */
static int bootz_start(cmd_tbl_t *cmdtp, int flag, int argc,
			char * const argv[], bootm_headers_t *images)
{
	int ret;
	ulong zi_start, zi_end;

	ret = do_bootm_states(cmdtp, flag, argc, argv, BOOTM_STATE_START,
			      images, 1);

	/* Setup Linux kernel zImage entry point */
	if (!argc) {
		images->ep = load_addr;
		debug("*  kernel: default image load address = 0x%08lx\n",
				load_addr);
	} else {
		images->ep = simple_strtoul(argv[0], NULL, 16);
		debug("*  kernel: cmdline image address = 0x%08lx\n",
			images->ep);
	}

	ret = bootz_setup(images->ep, &zi_start, &zi_end);
	if (ret != 0)
		return 1;

	lmb_reserve(&images->lmb, images->ep, zi_end - zi_start);

	/*
	 * Handle the BOOTM_STATE_FINDOTHER state ourselves as we do not
	 * have a header that provide this informaiton.
	 */
	if (bootm_find_ramdisk_fdt(flag, argc, argv))
		return 1;

	return 0;
}

int do_bootz(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int ret;

	/* Consume 'bootz' */
	argc--; argv++;

	if (bootz_start(cmdtp, flag, argc, argv, &images))
		return 1;

	/*
	 * We are doing the BOOTM_STATE_LOADOS state ourselves, so must
	 * disable interrupts ourselves
	 */
	bootm_disable_interrupts();

	images.os.os = IH_OS_LINUX;
	ret = do_bootm_states(cmdtp, flag, argc, argv,
			      BOOTM_STATE_OS_PREP | BOOTM_STATE_OS_FAKE_GO |
			      BOOTM_STATE_OS_GO,
			      &images, 1);

	return ret;
}

#ifdef CONFIG_SYS_LONGHELP
static char bootz_help_text[] =
	"[addr [initrd[:size]] [fdt]]\n"
	"    - boot Linux zImage stored in memory\n"
	"\tThe argument 'initrd' is optional and specifies the address\n"
	"\tof the initrd in memory. The optional argument ':size' allows\n"
	"\tspecifying the size of RAW initrd.\n"
#if defined(CONFIG_OF_LIBFDT)
	"\tWhen booting a Linux kernel which requires a flat device-tree\n"
	"\ta third argument is required which is the address of the\n"
	"\tdevice-tree blob. To boot that kernel without an initrd image,\n"
	"\tuse a '-' for the second argument. If you do not pass a third\n"
	"\ta bd_info struct will be passed instead\n"
#endif
	"";
#endif

U_BOOT_CMD(
	bootz,	CONFIG_SYS_MAXARGS,	1,	do_bootz,
	"boot Linux zImage image from memory", bootz_help_text
);
#endif	/* CONFIG_CMD_BOOTZ */

#ifdef CONFIG_CMD_BOOTAI

#ifndef CONFIG_CMD_BOOTAI_BOOT_PART
#define CONFIG_CMD_BOOTAI_BOOT_PART "boot"
#endif

#ifndef CONFIG_CMD_BOOTAI_RECOVERY_PART
#define CONFIG_CMD_BOOTAI_RECOVERY_PART "recovery"
#endif

void bootimg_print_image_hdr(struct andr_img_hdr *hdr)
{
	int i;
	printf ("   Image magic:   %s\n", hdr->magic);

	printf ("   kernel_size:   0x%x\n", hdr->kernel_size);
	printf ("   kernel_addr:   0x%x\n", hdr->kernel_addr);

	printf ("   rdisk_size:   0x%x\n", hdr->ramdisk_size);
	printf ("   rdisk_addr:   0x%x\n", hdr->ramdisk_addr);

	printf ("   second_size:   0x%x\n", hdr->second_size);
	printf ("   second_addr:   0x%x\n", hdr->second_addr);

	printf ("   tags_addr:   0x%x\n", hdr->tags_addr);
	printf ("   page_size:   0x%x\n", hdr->page_size);

	printf ("   dt_size:   0x%x\n", hdr->dt_size);

	printf ("   name:      %s\n", hdr->name);
	printf ("   cmdline:   %s\n", hdr->cmdline);

	for (i=0;i<8;i++)
		printf ("   id[%d]:   0x%x\n", i, hdr->id[i]);
}

#define _ALIGN(n,pagesz) ((n + (pagesz - 1)) & (~(pagesz - 1)))

int do_fdt(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[]);

int do_bootai(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	u32 addr;
	u32 fdt_addr = 0;
	u32 kernel_addr = 0x81000000;
	u32 ramdisk_addr = 0x82100000;
	char ptn[16];
	int boot_from_mmc = 0;
	struct andr_img_hdr *hdr;
	u64 num_sectors;
	int status;
	u32 mmc_instance = 0;

	if (argc < 2) {
		printf("%s: ERROR not enough arguments!!\n", __func__);
		return -1;
	}

	if (!(strcmp(argv[1], "ram"))) {
		boot_from_mmc = 0;
		if (argc < 3) {
			printf("%s: ERROR not enough arguments for 'ram' call!!\n", __func__);
			return -1;
		}
		addr = simple_strtoul(argv[2], NULL, 16);
	}
	else {
		boot_from_mmc = 1;
		if (argc > 1)
			mmc_instance = simple_strtoul(argv[1], NULL, 10);
		addr = CONFIG_USB_FASTBOOT_BUF_ADDR;
	}
	if (getenv_yesno("recovery") == 1)
		strcpy(ptn, CONFIG_CMD_BOOTAI_RECOVERY_PART);
	else
		strcpy(ptn, CONFIG_CMD_BOOTAI_BOOT_PART);

	printf("%s: checking fdt_addr = %s\n", __func__, getenv("fdt_addr_r"));
	fdt_addr = getenv_ulong("fdt_addr_r", 16, fdt_addr);
	if (!fdt_addr) {
		printf("%s: invalid fdtaddr in env\n", __func__);
		goto fail;
	}

	hdr = (struct andr_img_hdr *) addr;

	if (boot_from_mmc) {
		int ret;
		block_dev_desc_t *dev_desc;
		disk_partition_t info;
		unsigned sector;

		memset(hdr, 0, sizeof(struct andr_img_hdr));

		dev_desc = get_dev("mmc", mmc_instance);
		if (!dev_desc || dev_desc->type == DEV_TYPE_UNKNOWN) {
			printf("%s: invalid mmc device\n", __func__);
			goto fail;
		}

		ret = get_partition_info_efi_by_name(dev_desc, ptn, &info);
		if (ret) {
			printf("%s: cannot find '%s' partition\n", __func__, ptn);
			goto fail;
		}
		num_sectors =  ((sizeof(struct andr_img_hdr) / info.blksz) + 1);
		printf("%s: MMC read %llu sectors * blksz(%lu) @ %lu (size= %lu)\n", __func__, num_sectors, info.blksz, info.start, info.size);
		dev_desc->block_read(dev_desc->dev, info.start, num_sectors, (void*)hdr);
		if (memcmp(hdr->magic, ANDR_BOOT_MAGIC, 8)) {
			printf("%s: bad boot image magic\n", __func__);
			goto fail;
		}

		/* HACK: check if ramdisk_addr is an offset rather than full addr */
		/* HACK: shouldn't need every device to define MEMORY_BASE */
		if (hdr->ramdisk_addr < MEMORY_BASE)
			hdr->ramdisk_addr += MEMORY_BASE;

		/* print kernel info */
		bootimg_print_image_hdr(hdr);
		printf("\n\nramdisk sector count:%d\n", (int)(hdr->ramdisk_size / info.blksz) + 1);

		/* read kernel */
		sector = info.start + (hdr->page_size / info.blksz);
		num_sectors = ((hdr->kernel_size / info.blksz) + 1);
#ifndef CONFIG_CMD_BOOTAI_IGNORE_HDR_ADDR
		kernel_addr = hdr->kernel_addr;
#endif
		status = dev_desc->block_read(dev_desc->dev, sector, num_sectors, (void*)kernel_addr);
		if (status < 0) {
			printf("%s: Could not read kernel image\n", __func__);
			goto fail;
		}

		/* read ramdisk */
		sector += _ALIGN(hdr->kernel_size, hdr->page_size) / info.blksz;
		num_sectors = ((hdr->ramdisk_size / info.blksz) + 1);
#ifndef CONFIG_CMD_BOOTAI_IGNORE_HDR_ADDR
		ramdisk_addr = hdr->ramdisk_addr;
#endif
		status = dev_desc->block_read(dev_desc->dev, sector, num_sectors, (void*)ramdisk_addr);
		if(status < 0) {
			printf("%s: Could not read ramdisk\n", __func__);
			goto fail;
		}

		if ((hdr->second_size) && (!hdr->dt_size)) {
			/* read devtree */
			sector += _ALIGN(hdr->ramdisk_size, hdr->page_size) / info.blksz;
			debug("*** %s::devtree sector (second_size) == %u\n", __func__, sector);
			num_sectors = ((hdr->second_size / info.blksz) + 1);
			status = dev_desc->block_read(dev_desc->dev, sector, num_sectors, (void*)fdt_addr);
			if(status < 0) {
				printf("booti: Could not read devtree (second_size)\n");
				goto fail;
			}
		}
		else if (hdr->dt_size) {
			/* read devtree (preferring dt_size value */
			sector += _ALIGN(hdr->ramdisk_size, hdr->page_size) / info.blksz;
			debug("*** %s::devtree sector (dt_size) == %u\n", __func__, sector);
			num_sectors = ((hdr->dt_size / info.blksz) + 1);
			status = dev_desc->block_read(dev_desc->dev, sector, num_sectors, (void*)fdt_addr);
			if(status < 0) {
				printf("%s: Could not read devtree (dt_size)\n", __func__);
				goto fail;
			}
		}
		else {
			/* load 1mb of DTB partition */
		}
	}else {
		u32 kaddr, raddr;

		printf("Boot image downloaded using fastboot\n");

		status = memcmp(hdr->magic, ANDR_BOOT_MAGIC, 8);
		if (status != 0) {
			printf("%s: bad boot image magic\n", __func__);
			goto fail;
		}

		/* print kernel info */
		bootimg_print_image_hdr(hdr);

		kaddr = addr + hdr->page_size;
		raddr = kaddr + _ALIGN(hdr->kernel_size, hdr->page_size);
#ifndef CONFIG_CMD_BOOTAI_IGNORE_HDR_ADDR
		kernel_addr = hdr->kernel_addr;
#endif
		memmove((void *) kernel_addr, (void *)kaddr, hdr->kernel_size);

		/* check if ramdisk_addr is an offset rather than full addr */
		if (hdr->ramdisk_addr < MEMORY_BASE)
			hdr->ramdisk_addr += MEMORY_BASE;

#ifndef CONFIG_CMD_BOOTAI_IGNORE_HDR_ADDR
		ramdisk_addr = hdr->ramdisk_addr;
#endif
		memmove((void *) ramdisk_addr, (void *)raddr, hdr->ramdisk_size);

		raddr +=  _ALIGN(hdr->ramdisk_size, hdr->page_size);

		if ((hdr->second_size) && (!hdr->dt_size)) {
			memmove((void *) fdt_addr, (void *)raddr, hdr->second_size);
		}
		else if (hdr->dt_size) {
			raddr +=  _ALIGN(hdr->second_size, hdr->page_size);
			memmove((void *) fdt_addr, (void *)raddr, hdr->dt_size);
		}
		else {
			/* load 1mb of DTB partition */
		}
	}

	// add boot.img cmdline to dtbootargs
	if (hdr->cmdline) {
		char temp[1024]; /* twice the size as normal */
		char *append = getenv("bootargs_append");
		if (append) {
			sprintf(temp, "%s %s", append, hdr->cmdline);
			setenv("bootargs", temp);
		}
	}

	printf("kernel    @ %08x (%d)\n", kernel_addr, hdr->kernel_size);
	printf("ramdisk   @ %08x (%d)\n", ramdisk_addr, hdr->ramdisk_size);
	printf("dtb       @ %08x (%d)\n", fdt_addr, hdr->second_size);
	printf("dt-size          (%d)\n", hdr->dt_size);

	status = do_bootm_states(cmdtp, flag, argc, argv, BOOTM_STATE_START,
				 &images, 1);

	images.ep = kernel_addr;
	images.rd_start = ramdisk_addr;
	images.rd_end = ramdisk_addr + hdr->ramdisk_size;
	if ((hdr->second_size) && (!hdr->dt_size)) {
		images.ft_len = hdr->second_size;
		images.ft_addr = (void *)fdt_addr;
	}
	else if (hdr->dt_size) {
		images.ft_len = hdr->dt_size;
		images.ft_addr = (void *)fdt_addr;
	}
	/*
	 * We are doing the BOOTM_STATE_LOADOS state ourselves, so must
	 * disable interrupts ourselves
	 */
	bootm_disable_interrupts();

	images.os.os = IH_OS_LINUX;
	status = do_bootm_states(cmdtp, flag, argc, argv,
			      BOOTM_STATE_OS_PREP | BOOTM_STATE_OS_FAKE_GO |
			      BOOTM_STATE_OS_GO,
			      &images, 1);

	printf("%s: Control returned to monitor - resetting...\n", __func__);
	return status;

fail:
	return 1;
}


U_BOOT_CMD(
	bootai,	3,	1,	do_bootai,
	"bootai   - boot android bootimg from memory\n",
	"<addr>\n    - boot application image stored in memory\n"
	"\t'addr' should be the address of boot image which is zImage+ramdisk.img\n"
);
#endif	/* CONFIG_CMD_BOOTAI */

#ifdef CONFIG_CMD_BOOTI
/* See Documentation/arm64/booting.txt in the Linux kernel */
struct Image_header {
	uint32_t	code0;		/* Executable code */
	uint32_t	code1;		/* Executable code */
	uint64_t	text_offset;	/* Image load offset, LE */
	uint64_t	image_size;	/* Effective Image size, LE */
	uint64_t	res1;		/* reserved */
	uint64_t	res2;		/* reserved */
	uint64_t	res3;		/* reserved */
	uint64_t	res4;		/* reserved */
	uint32_t	magic;		/* Magic number */
	uint32_t	res5;
};

#define LINUX_ARM64_IMAGE_MAGIC	0x644d5241

static int booti_setup(bootm_headers_t *images)
{
	struct Image_header *ih;
	uint64_t dst;

	ih = (struct Image_header *)map_sysmem(images->ep, 0);

	if (ih->magic != le32_to_cpu(LINUX_ARM64_IMAGE_MAGIC)) {
		puts("Bad Linux ARM64 Image magic!\n");
		return 1;
	}
	
	if (ih->image_size == 0) {
		puts("Image lacks image_size field, assuming 16MiB\n");
		ih->image_size = (16 << 20);
	}

	/*
	 * If we are not at the correct run-time location, set the new
	 * correct location and then move the image there.
	 */
	dst = gd->bd->bi_dram[0].start + le32_to_cpu(ih->text_offset);
	if (images->ep != dst) {
		void *src;

		debug("Moving Image from 0x%lx to 0x%llx\n", images->ep, dst);

		src = (void *)images->ep;
		images->ep = dst;
		memmove((void *)dst, src, le32_to_cpu(ih->image_size));
	}

	return 0;
}

/*
 * Image booting support
 */
static int booti_start(cmd_tbl_t *cmdtp, int flag, int argc,
			char * const argv[], bootm_headers_t *images)
{
	int ret;
	struct Image_header *ih;

	ret = do_bootm_states(cmdtp, flag, argc, argv, BOOTM_STATE_START,
			      images, 1);

	/* Setup Linux kernel Image entry point */
	if (!argc) {
		images->ep = load_addr;
		debug("*  kernel: default image load address = 0x%08lx\n",
				load_addr);
	} else {
		images->ep = simple_strtoul(argv[0], NULL, 16);
		debug("*  kernel: cmdline image address = 0x%08lx\n",
			images->ep);
	}

	ret = booti_setup(images);
	if (ret != 0)
		return 1;

	ih = (struct Image_header *)map_sysmem(images->ep, 0);

	lmb_reserve(&images->lmb, images->ep, le32_to_cpu(ih->image_size));

	/*
	 * Handle the BOOTM_STATE_FINDOTHER state ourselves as we do not
	 * have a header that provide this informaiton.
	 */
	if (bootm_find_ramdisk_fdt(flag, argc, argv))
		return 1;

	return 0;
}

int do_booti(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int ret;

	/* Consume 'booti' */
	argc--; argv++;

	if (booti_start(cmdtp, flag, argc, argv, &images))
		return 1;

	/*
	 * We are doing the BOOTM_STATE_LOADOS state ourselves, so must
	 * disable interrupts ourselves
	 */
	bootm_disable_interrupts();

	images.os.os = IH_OS_LINUX;
	ret = do_bootm_states(cmdtp, flag, argc, argv,
			      BOOTM_STATE_OS_PREP | BOOTM_STATE_OS_FAKE_GO |
			      BOOTM_STATE_OS_GO,
			      &images, 1);

	return ret;
}

#ifdef CONFIG_SYS_LONGHELP
static char booti_help_text[] =
	"[addr [initrd[:size]] [fdt]]\n"
	"    - boot Linux Image stored in memory\n"
	"\tThe argument 'initrd' is optional and specifies the address\n"
	"\tof the initrd in memory. The optional argument ':size' allows\n"
	"\tspecifying the size of RAW initrd.\n"
#if defined(CONFIG_OF_LIBFDT)
	"\tSince booting a Linux kernelrequires a flat device-tree\n"
	"\ta third argument is required which is the address of the\n"
	"\tdevice-tree blob. To boot that kernel without an initrd image,\n"
	"\tuse a '-' for the second argument.\n"
#endif
	"";
#endif

U_BOOT_CMD(
	booti,	CONFIG_SYS_MAXARGS,	1,	do_booti,
	"boot arm64 Linux Image image from memory", booti_help_text
);
#endif	/* CONFIG_CMD_BOOTI */
