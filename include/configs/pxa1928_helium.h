/*
 * (C) Copyright 2011
 * Marvell Semiconductor <www.marvell.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __CONFIG_PXA1928_H
#define __CONFIG_PXA1928_H

#define CONFIG_ARM64
#define CONFIG_REMAKE_ELF

/*
 * High Level Configuration Options
 */
#define CONFIG_PXA1928
#define CONFIG_SYS_TEXT_BASE            0x9000000

#define CONFIG_POWER
#define CONFIG_POWER_I2C
#define CONFIG_POWER_88PM860
#define CONFIG_PXA1928_POWER

#define CONFIG_SYS_CACHELINE_SIZE	64
#define CONFIG_SYS_DCACHE_OFF
#define CONFIG_SYS_GENERIC_BOARD

/* Generic Timer Definitions for arm timer */
#define COUNTER_FREQUENCY              (0x340000)     /* 3.25MHz */
#define SECONDARY_CPU_MAILBOX           0x01210000

#define CONFIG_CONSOLE_MUX
#define CONFIG_SYS_CONSOLE_IS_IN_ENV
#define CONFIG_OF_STDOUT_VIA_ALIAS

#define CPU_RELEASE_ADDR		0xffffffff /* should not be touched */

#define CONFIG_SMP

#define CONFIG_SYS_SDRAM_BASE           0
#define CONFIG_NR_DRAM_BANKS_MAX	2

#define CONFIG_SRAM_BASE                0xd1020000
#define CONFIG_SYS_INIT_SP_ADDR         (CONFIG_SRAM_BASE + 0x1000)
#define CONFIG_SYS_RELOC_END            0x09700000

#define CONFIG_TZ_HYPERVISOR_SIZE       (0x01200000)

#define CONFIG_PARTITION_UUIDS
#define CONFIG_RANDOM_UUID
#define CONFIG_CMD_UUID

/*
 * Commands configuration
 */
#define CONFIG_SYS_NO_FLASH		/* Declare no flash (NOR/SPI) */
#include <config_cmd_default.h>
#define CONFIG_CMD_GPIO
#define CONFIG_CMD_MFP
#define CONFIG_CMD_MIPS
#define CONFIG_CMD_I2C
#define CONFIG_CMD_MMC
#define CONFIG_FS_EXT4
#define CONFIG_CMD_FS_GENERIC
#define CONFIG_CMD_GPT
#undef CONFIG_CMD_NET
#undef CONFIG_CMD_NFS

#define CONFIG_MRVL_USB_PHY 1
#define CONFIG_MRVL_USB_PHY_28LP 1
#define CONFIG_CI_UDC
#define CONFIG_USB_ETHER
#define CONFIG_USB_ETH_CDC
#define CONFIG_CMD_FASTBOOT
#define CONFIG_CMD_BOOTI

#define CONFIG_USB_EHCI
#define CONFIG_USB_EHCI_PXA
#define CONFIG_USB_GADGET
#define CONFIG_USB_GADGET_VBUS_DRAW     0
#define CONFIG_USBDOWNLOAD_GADGET
#define CONFIG_G_DNL_VENDOR_NUM         0x0451
#define CONFIG_G_DNL_PRODUCT_NUM        0xd022
#define CONFIG_G_DNL_MANUFACTURER       "Marvell"
#define CONFIG_ANDROID_BOOT_IMAGE
#define CONFIG_USB_FASTBOOT_BUF_ADDR    0x10000000
#define CONFIG_USB_FASTBOOT_BUF_SIZE    0x20000000
#define CONFIG_FASTBOOT_FLASH
#define CONFIG_FASTBOOT_FLASH_MMC_DEV	0

#define CONFIG_CMD_USB_MASS_STORAGE
#define CONFIG_USB_GADGET_MASS_STORAGE

/*
 * mv-common.h should be defined after CMD configs since it used them
 * to enable certain macros
 */
#include "mv-common.h"

#undef CONFIG_ARCH_MISC_INIT

#undef CONFIG_SYS_NS16550_COM1
#define CONFIG_SYS_NS16550_COM1		PXA1928_UART1_BASE
#define CONFIG_SYS_NS16550_COM3		PXA1928_UART3_BASE

#undef CONFIG_CONS_INDEX
#define CONFIG_CONS_INDEX		3	/*Console on UART3 */

/*
 * Boot setting
 */
#define CONFIG_ZERO_BOOTDELAY_CHECK
#define CONFIG_SHOW_BOOT_PROGRESS
#define CONFIG_MISC_INIT_R

#define CONFIG_BOOTARGS \
	"rw console=ttyS0,115200 panic_debug uart_dma"

#define CONFIG_BOOTCOMMAND \
	"while true; do " \
	"mmc read ${fdt_addr_r} 0x10000 0x1000; " \
	"fastboot; " \
	"mmc read ${fdt_addr_r} 0x10000 0x1000; " \
	"mmc read ${kernel_addr_r} 0x8000 0x8000 && " \
	"bootm ${kernel_addr_r} ${kernel_addr_r} ${fdt_addr_r};" \
	"done"

#define CONFIG_MMC_BOOT_DEV     "mmc dev 0 0"
#define CONFIG_OF_LIBFDT

/*
 * Environment variables configurations
 */
#define CONFIG_ENV_IS_IN_MMC		/* save env in MMV */
#define CONFIG_SYS_MMC_ENV_DEV	0	/* save env in eMMC */
#define CONFIG_CMD_SAVEENV
#define CONFIG_ENV_SIZE		0x8000		/* env size set to 32KB */
#define CONFIG_ENV_OFFSET	(0x1000000 - CONFIG_ENV_SIZE)
#define CONFIG_SYS_HUSH_PARSER

#undef CONFIG_EXTRA_ENV_SETTINGS
#define CONFIG_EXTRA_ENV_SETTINGS \
	"stdin=eserial0,eserial2\0" \
	"stdout=eserial0,eserial2\0" \
	"stderr=eserial0,eserial2\0" \
        "autostart=yes\0" \
        "verify=yes\0" \
        "cdc_connect_timeout=60\0" \
	"fdt_addr_r=0x03000000\0" \
	"kernel_addr_r=0x0127f800\0" \
	"partitions=" \
	"name=DTIM,start=0x00200000,size=0x200000;" \
	"name=recovery,start=0x00400000,size=0xb00000;" \
	"name=bootloader,start=0x00F00000,size=0x100000;" \
	"name=boot,start=0x01000000,size=0x1000000;" \
	"name=dtb,start=0x02000000,size=0x1000000;" \
	"name=system,start=0x03000000,size=0x30000000;" \
	"name=vendor,start=0x33000000,size=0x10000000;" \
	"name=cache,start=0x43000000,size=0x20000000;" \
	"name=userdata,start=0x63000000,size=0x15d000000\0"

/* Marvell PXAV3 MMC Controller Configuration */
#define CONFIG_SDHCI_PXAV3
#define CONFIG_DDR_HW_DFC

#define CONFIG_SUPPORT_RAW_INITRD
#define CONFIG_PXA1928_DFC
#define CONFIG_PXA1928_LPM
#define CONFIG_PXA1928_COMM_D2

#endif	/* __CONFIG_PXA1928_H */
