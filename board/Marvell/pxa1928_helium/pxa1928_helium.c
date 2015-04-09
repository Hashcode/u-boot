/*
e* (C) Copyright 2011
 * Marvell Semiconductors Ltd. <www.marvell.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/cpu.h>
#ifdef CONFIG_SDHCI
#include <sdhci.h>
#endif
#include <mvmfp.h>
#include <mv_recovery.h>
#include <asm/arch/mfp.h>
#include <malloc.h>
#include <usb.h>
#include <power/pmic.h>
#include <power/marvell88pm_pmic.h>
#include <power/pxa1928_freq.h>
#include <asm/gpio.h>
#include <linux/usb/mv-phy.h>
#ifdef CONFIG_OF_LIBFDT
#include <libfdt.h>
#endif
#include "../common/cmdline.h"

DECLARE_GLOBAL_DATA_PTR;

#define DVC_CONTROL_REG	0x4F
/*two dvc control register bits can support 4 control level
4(control level) * 4 (DVC level registers) can support 16level DVC*/
#define DVC_SET_ADDR1	(1 << 0)
#define DVC_SET_ADDR2	(1 << 1)
#define DVC_CTRl_LVL	4

#define PMIC_I2C_BUS 0
#define FG_I2C_BUS 0

unsigned int mv_profile = 0xFF;
/* Define CPU/DDR default max frequency
   CPU: 1300MHz
   DDR: 528MHz
   GC3D: 624MHz
   GC2D: 312MHz
*/
#define CPU_MAX_FREQ_DEFAULT	1300
#define DDR_MAX_FREQ_DEFAULT	528
#define GC3D_MAX_FREQ_DEFAULT	624
#define GC2D_MAX_FREQ_DEFAULT	312
/* Define CPU/DDR max frequency
   CPU: 1508MHz
   GC3D: 797MHz
   GC2D: 416MHz
*/
#define CPU_MAX_FREQ	1508
#define GC3D_MAX_FREQ	797
#define GC2D_MAX_FREQ   416

static int highperf;

int board_early_init_f(void)
{
	u32 mfp_cfg[] = {
		/* UART1 */
		UART1_RXD_GPIO51_MFP51,
		UART1_TXD_GPIO52_MFP52,
		UART1_CTS_GPIO53_MFP53,
		UART1_RTS_GPIO54_MFP54,
		/* UART3 */
		UART3_RXD_MMC2_DAT7_MFP33,
		UART3_TXD_MMC2_DAT6_MFP34,
		/* TWSI */
		PWR_SCL_MFP67,
		PWR_SDA_MFP68,
		TWSI6_SCL_MMC2_DAT5_MFP35,
		TWSI6_SDA_MMC2_DAT4_MFP36,
		/* eMMC */
		MMC3_DAT0_ND_IO8_MFP87,
		MMC3_DAT1_ND_IO9_MFP86,
		MMC3_DAT2_ND_IO10_MFP85,
		MMC3_DAT3_ND_IO11_MFP84,
		MMC3_DAT4_ND_IO12_MFP83,
		MMC3_DAT5_ND_IO13_MFP82,
		MMC3_DAT6_ND_IO14_MFP81,
		MMC3_DAT7_ND_IO15_MFP80,
		MMC3_CLK_SM_ADVMUX_MFP88,
		MMC3_CMD_SM_RDY_MFP89,
		MMC3_RST_ND_CLE_MFP90,
		/* DVC pin */
		DVC_PIN0_MFP107,
		DVC_PIN1_MFP108,
		DVC_PIN2_MFP99,
		DVC_PIN3_MFP103,
		GPIO17_OUT_MFP17,

		GPIO125_MFP125,
		GPIO134_MFP134,
		GPIO135_MFP135,
		VCXO_REQ_MFP77,
		VCXO_OUT_MFP78,
		GPIO0_MFP0,
		GPIO2_MFP2,

		GPIO3_MFP3,
		GPIO132_MFP132,
		GPIO9_IN_MFP9,

		/*End of configureation*/
		MFP_EOC
	};
	mfp_config(mfp_cfg);

	gpio_direction_output(125,0);	/* Select HSCI to USB USB */
	gpio_direction_output(17,0);	/* HSIC mux output enable */

	gpio_direction_output(0, 0); 	/* Reset USB Hub */
	gpio_direction_output(2, 1);	/* Disable USB Hub Vbus */
	gpio_direction_output(135,1);	/* Enable USB Hub Connect */
	gpio_direction_output(134, 1);	/* Enable USB Hub 26MHz */

	gpio_direction_output(132, 0);	/* DSI mux select HDMI */
	gpio_direction_output(3, 0);	/* DSI mux output enable */

	/* Enable 26MHz clock out for USB hub */
	writel(0x100, 0xd4050018);

	return 0;
}

int board_usb_init(int index, enum usb_init_type init)
{
        return 0;
}

int g_dnl_board_usb_cable_connected(void)
{
	int chrg_type = mrvl_usb_phy_28nm_charger_detect(CONFIG_USB_PHY_BASE);
	return !(chrg_type == DCP_CHARGER || chrg_type == NULL_CHARGER);
}

int board_init(void)
{
	/* Reset USB hub */
	gpio_direction_output(0, 0);
	mdelay(1);
	gpio_direction_output(0, 1);
	gpio_direction_output(2, 0);	// Enable USB Hub Vbus

	return 0;
}

#if defined(CONFIG_OF_BOARD_SETUP)
void ft_board_setup(void *devtree, bd_t *bd)
{
	char cmd[128];

	/* set dtb addr */
	sprintf(cmd, "fdt addr 0x%p", devtree);
	run_command(cmd, 0);

	/* pass profile number */
	sprintf(cmd, "fdt set /profile marvell,profile-number <%d>\n", mv_profile);
	run_command(cmd, 0);

	/*
	 * we use 1926 pp table by default, if if sethighperf cmd is set,
	 * use pxa1928 pp instead.
	 */
	if (highperf)
		run_command("fdt set /pp_version version pxa1928", 0);

	/* update dtb so as not to enable ICU for B0 stepping */
	run_command("fdt set /pxa1928_apmu_ver version bx", 0);
	run_command("fdt rm /soc/axi/wakeupgen@d4284000", 0);
}
#endif

#ifndef CONFIG_GLB_SECURE_EN
#define SOC_POWER_POINT_ADDR0	0xD4292AAC
#define SOC_POWER_POINT_ADDR1	0xD4292AB0
static int svt_profile_map_table[] = {
	999, 445, 433, 421, 408, 395, 383, 371,
	358, 358, 333, 333, 309, 309, 1,
};
void show_dro(void)
{
	u32 val0, val1;
	int lvt, nlvt, nsvt, svt;
	int i;

	val0 = readl(SOC_POWER_POINT_ADDR0);
	val1 = readl(SOC_POWER_POINT_ADDR1);
	lvt = val0 & 0x7FF;
	nlvt = (val0 & 0x3FF800) >> 11;
	nsvt = ((val1 & 0x1) << 10) | ((val0 & 0xFFC00000) >> 22);
	svt = (val1 & 0xFFE) >> 1;
	printf("----show dro----\n");
	printf("LVT NUMBER: %d\n", lvt);
	printf("NLVT NUMBER: %d\n", nlvt);
	printf("NSVT NUMBER: %d\n", nsvt);
	printf("SVT NUMBER: %d\n", svt);
	printf("----------------\n");

	for (i = 1; i < 15; i++) {
		if (svt >= svt_profile_map_table[i] &&
			svt < svt_profile_map_table[i - 1])
			break;
	}
	mv_profile = i + 1;
	if (mv_profile >= 15 || mv_profile < 0)
		mv_profile = 0;
	printf("SoC Profile Number: %d\n", mv_profile);
}
#endif

static int do_sethighperf(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	char *cmdline;
	char *ep;
	unsigned int new_set;
	static unsigned int old_set;
	u32 cpu_max = CPU_MAX_FREQ_DEFAULT;
	u32 ddr_max = DDR_MAX_FREQ_DEFAULT;
	u32 gc3d_max = GC3D_MAX_FREQ_DEFAULT;
	u32 gc2d_max = GC2D_MAX_FREQ_DEFAULT;

	if (argc != 2) {
		printf("usage: sethighperf 0 or 1 to enable low or high performance\n");
		return -1;
	}
	new_set = simple_strtoul((const char *)argv[1], &ep, 10);
	if (new_set != 0 && new_set != 1) {
		printf("usage: sethighperf 0 or 1 to enable low or high performance\n");
		return -1;
	}
	if (old_set != new_set) {
		if (new_set == 1)
			highperf = 1;
		else if (new_set == 0)
			highperf = 0;

		old_set = new_set;
		cmdline = malloc(COMMAND_LINE_SIZE);
		strncpy(cmdline, getenv("bootargs"), COMMAND_LINE_SIZE);
		remove_cmdline_param(cmdline, "cpu_max=");
		remove_cmdline_param(cmdline, "ddr_max=");
		remove_cmdline_param(cmdline, "gc3d_max=");
		remove_cmdline_param(cmdline, "gc2d_max=");
		if (1 == new_set) {
			cpu_max = CPU_MAX_FREQ;
			gc3d_max = GC3D_MAX_FREQ;
			gc2d_max = GC2D_MAX_FREQ;
		}
		sprintf(cmdline + strlen(cmdline), " cpu_max=%u000", cpu_max);
		sprintf(cmdline + strlen(cmdline), " ddr_max=%u000", ddr_max);
		sprintf(cmdline + strlen(cmdline), " gc3d_max=%u000", gc3d_max);
		sprintf(cmdline + strlen(cmdline), " gc2d_max=%u000", gc2d_max);
		setenv("bootargs", cmdline);
		free(cmdline);
	}
	printf("sethighperf success\n");
	return 0;
}

U_BOOT_CMD(
	sethighperf, 3, 0, do_sethighperf,
	"Setting cpu, ddr frequence for high performance or low performance",
	""
);

static void set_limit_max_frequency(unsigned int type, u32 max)
{
	char *cmdline;
	char *rem;
	const char *add;
	u32 max_default;
	u32 max_preferred;

	switch (type) {
	case SKU_CPU_MAX_PREFER:
		rem = "cpu_max=";
		add = " cpu_max=%u000";
		max_default = CPU_MAX_FREQ_DEFAULT;
		break;

	case SKU_DDR_MAX_PREFER:
		rem = "ddr_max=";
		add = " ddr_max=%u000";
		max_default = DDR_MAX_FREQ_DEFAULT;
		break;

	case SKU_GC3D_MAX_PREFER:
		rem = "gc3d_max=";
		add = " gc3d_max=%u000";
		max_default = GC3D_MAX_FREQ_DEFAULT;
		break;

	case SKU_GC2D_MAX_PREFER:
		rem = "gc2d_max=";
		add = " gc2d_max=%u000";
		max_default = GC2D_MAX_FREQ_DEFAULT;
		break;

	default:
		return;
	}

	if (0 == max)
		max = max_default;

	max_preferred = get_sku_max_setting(type);
	if (0 == max_preferred)
		max_preferred = min(max_default, max);
	else
		max_preferred = min(max_preferred, max);

	cmdline = malloc(COMMAND_LINE_SIZE);
	strncpy(cmdline, getenv("bootargs"), COMMAND_LINE_SIZE);
	remove_cmdline_param(cmdline, rem);
	sprintf(cmdline + strlen(cmdline), add, max_preferred);
	setenv("bootargs", cmdline);
	free(cmdline);
}

#if defined(CONFIG_PXA1928_DFC)
static u32 get_ddr_type(void)
{
	if (((readl(0xd0000300) >> 4) & 0xf) == 0xa) /* lpddr3 */
		if (((readl(0xd0000200) >> 16) & 0x1f) == 0xd) /* 1GB */
			return 1;
		else if (((readl(0xd0000210) >> 8) & 0xf) == 0x5)/* Hynix 2GB */
			return readl(0xD4282C98) ? 4 : 2; /* 4 for dis, 2 for pop */
		else /* Elpida 2GB */
			return 3;
	else /* lpddr2 */
		return 0;
}
#endif

int misc_init_r(void)
{
#if defined(CONFIG_PXA1928_DFC)
	u32 ddr_type;
	ddr_type = get_ddr_type();
#endif

	/* set cpu, gc3d, gc2d  max frequency */
	set_limit_max_frequency(SKU_CPU_MAX_PREFER, CPU_MAX_FREQ);
	set_limit_max_frequency(SKU_GC3D_MAX_PREFER, GC3D_MAX_FREQ);
	set_limit_max_frequency(SKU_GC2D_MAX_PREFER, GC2D_MAX_FREQ);

#ifndef CONFIG_GLB_SECURE_EN
	show_dro();
#endif
	/*
	 * bus 0 is used by pmic, set here for debug with
	 * "i2c probe", this should be the called just before exit,
	 * in case the default bus number is changed
	 */
	i2c_set_bus_num(0);

#if defined(CONFIG_PXA1928_DFC)
	pxa1928_fc_init(ddr_type);

	run_command("setcpurate 624", 0);
	run_command("setddrrate hwdfc 312", 0);
	run_command("setaxirate 156", 0);
#endif

	srand(get_ticks());

	return 0;
}

#ifdef CONFIG_MV_SDHCI
 /* eMMC: BUCK2/VCC_IO_NAND(1.8v)->eMMC(vmmcq), LDO4/V3P3_NAND(2.8v)->eMMC(vmmc) (default on)
 * SD: LDO6/VCC_IO_MMC1(3.3v)->SD(vmmcq), LDO10/V_MMC_CARD(3.3v)->SD(vmmc) (default off)
 */
int board_mmc_init(bd_t *bd)
{
	ulong mmc_base_address[CONFIG_SYS_MMC_NUM] = CONFIG_SYS_MMC_BASE;
	u8 i;
	u32 val;

	for (i = 0; i < CONFIG_SYS_MMC_NUM; i++) {
		if (mv_sdh_init(mmc_base_address[i], 104000000, 0,
				SDHCI_QUIRK_32BIT_DMA_ADDR))
			return 1;
		/*
		 * use default hardware clock gating
		 * by default, SD_FIFO_PARM = 0x70005
		 */
		if (i == 0) {
			/*
			 * emmc need to tune RX/TX under HS50
			 * RX need to set proper delays cycles.
			 * TX can work just invert the internal clock (TX_CFG_REG[30])
			 * but also set the delay cycles here for safety.
			 */
			writel(TX_MUX_DLL | TX_HOLD_DELAY0(0x16A),
						mmc_base_address[i] + TX_CFG_REG);
			writel(SDCLK_DELAY(0xA0) | SDCLK_SEL1(0x1),
						mmc_base_address[i] + RX_CFG_REG);
		} else {
			/*
			 * sd card can work under HS50 by default.
			 * but also invert TX internal clock (TX_CFG_REG[30]) here for safety.
			 */
			val = readl(mmc_base_address[i] + TX_CFG_REG);
			val |= TX_INT_CLK_INV;
			writel(val, mmc_base_address[i] + TX_CFG_REG);
		}
	}

	return 0;
}
#endif

void reset_cpu(ulong ignored)
{
#ifdef CONFIG_POWER_88PM860
	printf("pxa1928 board rebooting...\n");
	pmic_reset_bd();
#endif
}

#ifdef CONFIG_POWER_88PM860
void board_pmic_power_fixup(struct pmic *p_power)
{
	u32 val;
	unsigned int mask, dvc_ctrl;

	/* enable buck1 dual phase mode */
	pmic_reg_read(p_power, 0x8e, &val);
	val |= (1 << 2);
	pmic_reg_write(p_power, 0x8e, val);

	/* set buck1 all the DVC register 16 levels all at 1.2v
	 * dvc_ctrl is the value of the two dvc control bits
	 */
	for (dvc_ctrl = 0; dvc_ctrl < DVC_CTRl_LVL; dvc_ctrl++) {
		pmic_reg_read(p_power, DVC_CONTROL_REG, &val);
		mask = (DVC_SET_ADDR1 | DVC_SET_ADDR2);
		val &= ~mask;
		val |= dvc_ctrl & mask;
		pmic_reg_write(p_power, DVC_CONTROL_REG, val);

		val = 0x30;
		pmic_reg_write(p_power, 0x3c, val);
		pmic_reg_write(p_power, 0x3d, val);
		pmic_reg_write(p_power, 0x3e, val);
		pmic_reg_write(p_power, 0x3f, val);

		pmic_reg_write(p_power, 0x4b, val);
		pmic_reg_write(p_power, 0x4c, val);
		pmic_reg_write(p_power, 0x4d, val);
		pmic_reg_write(p_power, 0x4e, val);

		/* set buck3 all the DVC register at 1.2v */
		val = 0x30;
		pmic_reg_write(p_power, 0x41, val);
		pmic_reg_write(p_power, 0x42, val);
		pmic_reg_write(p_power, 0x43, val);
		pmic_reg_write(p_power, 0x44, val);

		/* set buck5 all the DVC register at 3.3v for WIB_SYS */
		val = 0x72;
		pmic_reg_write(p_power, 0x46, val);
		pmic_reg_write(p_power, 0x47, val);
		pmic_reg_write(p_power, 0x48, val);
		pmic_reg_write(p_power, 0x49, val);
	}
}
#endif

__weak int power_init_common(void) {return -1; }
__weak int pmic_init(unsigned char bus) {return -1; }
int power_init_board(void)
{
	/* init PMIC  */
	if (pmic_init(PMIC_I2C_BUS))
		return -1;
	if (power_init_common()) {
		printf("%s: init pmic fails.\n", __func__);
		return -1;
	}

	return 0;
}

#ifdef CONFIG_CMD_NET
int board_eth_init(bd_t *bis)
{
	int res = -1;

#if defined(CONFIG_CI_UDC)
	if (usb_eth_initialize(bis) >= 0)
		res = 0;
#endif
	return res;
}
#endif
