/* $Id$
 *  linux/drivers/video/aty128fb.c -- Frame buffer device for ATI Rage128
 *
 *  Copyright (C) 1999-2000, Brad Douglas <brad@neruo.com>
 *  Copyright (C) 1999, Anthony Tong <atong@uiuc.edu>
 *
 *                Ani Joshi / Jeff Garzik
 *                      - Code cleanup
 *
 *  Based off of Geert's atyfb.c and vfb.c.
 *
 *  TODO:
 *		- panning
 *		- monitor sensing (DDC)
 *              - virtual display
 *		- other platform support (only ppc/x86 supported)
 *		- hardware cursor support
 *		- ioctl()'s
 *
 *    Please cc: your patches to brad@neruo.com.
 */

/*
 * A special note of gratitude to ATI's devrel for providing documentation,
 * example code and hardware. Thanks Nitya.	-atong and brad
 */


#include <linux/config.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <asm/uaccess.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/ioport.h>
#include <asm/io.h>

#ifdef CONFIG_PPC
#include <asm/prom.h>
#include <asm/pci-bridge.h>
#include <video/macmodes.h>
#ifdef CONFIG_NVRAM
#include <linux/nvram.h>
#endif
#endif

#ifdef CONFIG_ADB_PMU
#include <linux/adb.h>
#include <linux/pmu.h>
#endif

#ifdef CONFIG_PMAC_BACKLIGHT
#include <asm/backlight.h>
#endif

#ifdef CONFIG_MTRR
#include <asm/mtrr.h>
#endif

#include <video/aty128.h>

/* Debug flag */
#undef DEBUG

#ifdef DEBUG
#define DBG(fmt, args...)		printk(KERN_DEBUG "aty128fb: %s " fmt, __FUNCTION__, ##args);
#else
#define DBG(fmt, args...)
#endif

#ifndef CONFIG_PPC
/* default mode */
static struct fb_var_screeninfo default_var __initdata = {
    /* 640x480, 60 Hz, Non-Interlaced (25.175 MHz dotclock) */
    640, 480, 640, 480, 0, 0, 8, 0,
    {0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
    0, 0, -1, -1, 0, 39722, 48, 16, 33, 10, 96, 2,
    0, FB_VMODE_NONINTERLACED
};

#else /* CONFIG_PPC */
/* default to 1024x768 at 75Hz on PPC - this will work
 * on the iMac, the usual 640x480 @ 60Hz doesn't. */
static struct fb_var_screeninfo default_var = {
    /* 1024x768, 75 Hz, Non-Interlaced (78.75 MHz dotclock) */
    1024, 768, 1024, 768, 0, 0, 8, 0,
    {0, 8, 0}, {0, 8, 0}, {0, 8, 0}, {0, 0, 0},
    0, 0, -1, -1, 0, 12699, 160, 32, 28, 1, 96, 3,
    FB_SYNC_HOR_HIGH_ACT|FB_SYNC_VERT_HIGH_ACT, FB_VMODE_NONINTERLACED
};
#endif /* CONFIG_PPC */

/* default modedb mode */
/* 640x480, 60 Hz, Non-Interlaced (25.172 MHz dotclock) */
static struct fb_videomode defaultmode __initdata = {
	refresh:	60,
	xres:		640,
	yres:		480,
	pixclock:	39722,
	left_margin:	48,
	right_margin:	16,
	upper_margin:	33,
	lower_margin:	10,
	hsync_len:	96,
	vsync_len:	2,
	sync:		0,
	vmode:		FB_VMODE_NONINTERLACED
};

static struct fb_fix_screeninfo aty128fb_fix __initdata = {
    "ATY Rage128", (unsigned long) NULL, 0, FB_TYPE_PACKED_PIXELS, 0,
    FB_VISUAL_PSEUDOCOLOR, 8, 1, 0, 0, (unsigned long) NULL, 0x1fff, 
    FB_ACCEL_ATI_RAGE128
};

/* struct to hold chip description information */
struct aty128_chip_info {
    const char *name;
    unsigned short device;
    int chip_gen;
};

/* Chip generations */
enum {
	rage_128,
	rage_128_pro,
	rage_M3
};

/* supported Rage128 chipsets */
static struct aty128_chip_info aty128_pci_probe_list[] __initdata =
{
    {"Rage128 RE (PCI)", PCI_DEVICE_ID_ATI_RAGE128_RE, rage_128},
    {"Rage128 RF (AGP)", PCI_DEVICE_ID_ATI_RAGE128_RF, rage_128},
    {"Rage128 RK (PCI)", PCI_DEVICE_ID_ATI_RAGE128_RK, rage_128},
    {"Rage128 RL (AGP)", PCI_DEVICE_ID_ATI_RAGE128_RL, rage_128},
    {"Rage128 Pro PF (AGP)", PCI_DEVICE_ID_ATI_RAGE128_PF, rage_128_pro},
    {"Rage128 Pro PR (PCI)", PCI_DEVICE_ID_ATI_RAGE128_PR, rage_128_pro},
    {"Rage Mobility M3 (PCI)", PCI_DEVICE_ID_ATI_RAGE128_LE, rage_M3},
    {"Rage Mobility M3 (AGP)", PCI_DEVICE_ID_ATI_RAGE128_LF, rage_M3},
    {NULL, 0, rage_128}
 };

/* packed BIOS settings */
#ifndef CONFIG_PPC
typedef struct {
	u8 clock_chip_type;
	u8 struct_size;
	u8 accelerator_entry;
	u8 VGA_entry;
	u16 VGA_table_offset;
	u16 POST_table_offset;
	u16 XCLK;
	u16 MCLK;
	u8 num_PLL_blocks;
	u8 size_PLL_blocks;
	u16 PCLK_ref_freq;
	u16 PCLK_ref_divider;
	u32 PCLK_min_freq;
	u32 PCLK_max_freq;
	u16 MCLK_ref_freq;
	u16 MCLK_ref_divider;
	u32 MCLK_min_freq;
	u32 MCLK_max_freq;
	u16 XCLK_ref_freq;
	u16 XCLK_ref_divider;
	u32 XCLK_min_freq;
	u32 XCLK_max_freq;
} __attribute__ ((packed)) PLL_BLOCK;
#endif /* !CONFIG_PPC */

/* onboard memory information */
struct aty128_meminfo {
    u8 ML;
    u8 MB;
    u8 Trcd;
    u8 Trp;
    u8 Twr;
    u8 CL;
    u8 Tr2w;
    u8 LoopLatency;
    u8 DspOn;
    u8 Rloop;
    const char *name;
};

/* various memory configurations */
static const struct aty128_meminfo sdr_128   =
    { 4, 4, 3, 3, 1, 3, 1, 16, 30, 16, "128-bit SDR SGRAM (1:1)" };
static const struct aty128_meminfo sdr_64    =
    { 4, 8, 3, 3, 1, 3, 1, 17, 46, 17, "64-bit SDR SGRAM (1:1)" };
static const struct aty128_meminfo sdr_sgram =
    { 4, 4, 1, 2, 1, 2, 1, 16, 24, 16, "64-bit SDR SGRAM (2:1)" };
static const struct aty128_meminfo ddr_sgram =
    { 4, 4, 3, 3, 2, 3, 1, 16, 31, 16, "64-bit DDR SGRAM" };

static int  noaccel __initdata = 0;
static char *mode __initdata = NULL;
static int  nomtrr __initdata = 0;

static const char *mode_option __initdata = NULL;

/* PLL constants */
struct aty128_constants {
    u32 dotclock;
    u32 ppll_min;
    u32 ppll_max;
    u32 ref_divider;
    u32 xclk;
    u32 fifo_width;
    u32 fifo_depth;
};

struct aty128_crtc {
    u32 h_total, h_sync_strt_wid;
    u32 v_total, v_sync_strt_wid;
    u32 offset, offset_cntl;
    u32 gen_cntl;
    u32 ext_cntl;
    u32 pitch;
    u32 bpp;
};

struct aty128_pll {
    u32 post_divider;
    u32 feedback_divider;
    u32 vclk;
};

struct aty128_ddafifo {
    u32 dda_config;
    u32 dda_on_off;
};

/* register values for a specific mode */
struct aty128fb_par {
    struct aty128_constants constants;  /* PLL and others      */
    const struct aty128_meminfo *mem;   /* onboard mem info    */
    struct aty128_ddafifo fifo_reg;
    int blitter_may_be_busy;
    struct aty128_crtc crtc;
    struct aty128_pll pll;
#ifdef CONFIG_PCI
    struct pci_dev *pdev;
#endif
    int fifo_slots;			/* free slots in FIFO (64 max) */
    void *regbase;                      /* remapped mmio       */
    int chip_gen;
};

struct fb_info_aty128 {
    struct fb_info_aty128 *next;
};

static struct fb_info_aty128 *board_list = NULL;
static u32 aty128fb_pseudo_palette[17];
static struct aty128fb_par default_par;

#define round_div(n, d) ((n+(d/2))/d)

    /*
     *  Internal routines
     */
static int aty128_pci_register(struct pci_dev *pdev,
                               const struct aty128_chip_info *aci);
static struct fb_info_aty128 *aty128_board_list_add(struct fb_info_aty128
				*board_list, struct fb_info_aty128 *new_node);
#if !defined(CONFIG_PPC) && !defined(__sparc__)
static void __init aty128_get_pllinfo(struct aty128fb_par *par,
			char *bios_seg);
static char __init *aty128find_ROM(struct aty128fb_par *par);
#endif
static void aty128_timings(struct aty128fb_par *par);
static void aty128_init_engine(struct fb_info *info);
static void aty128_reset_engine(const struct aty128fb_par *par);
static void aty128_flush_pixel_cache(const struct aty128fb_par *par);
static void do_wait_for_fifo(u16 entries, struct aty128fb_par *par);
static void wait_for_fifo(u16 entries, struct aty128fb_par *par);
static void wait_for_idle(struct aty128fb_par *par);
static u32 bpp_to_depth(u32 bpp);

    /*
     *  Interface used by the world
     */
int aty128fb_init(void);
int aty128fb_setup(char *options);

static int aty128fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info);
static int aty128fb_set_par(struct fb_info *info);
static int aty128fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
				u_int transp, struct fb_info *info);
static int aty128fb_pan_display(struct fb_var_screeninfo *var, 
			   	struct fb_info *fb);
static int aty128fb_blank(int blank, struct fb_info *fb);


static struct fb_ops aty128fb_ops = {
	owner:		THIS_MODULE,
	fb_check_var:	aty128fb_check_var,
	fb_set_par:	aty128fb_set_par,
	fb_setcolreg:	aty128fb_setcolreg,
	fb_pan_display:	aty128fb_pan_display,
	fb_blank:	aty128fb_blank,
	fb_fillrect:	cfb_fillrect,
	fb_copyarea:    cfb_copyarea,
   	fb_imageblit:   cfb_imageblit,
};

#ifdef CONFIG_PMAC_BACKLIGHT
static int aty128_set_backlight_enable(int on, int level, void* data);
static int aty128_set_backlight_level(int level, void* data);

static struct backlight_controller aty128_backlight_controller = {
	aty128_set_backlight_enable,
	aty128_set_backlight_level
};
#endif /* CONFIG_PMAC_BACKLIGHT */

    /*
     * Functions to read from/write to the mmio registers
     *	- endian conversions may possibly be avoided by
     *    using the other register aperture. TODO.
     */
static inline u32
_aty_ld_le32(volatile unsigned int regindex, const struct aty128fb_par *par) 
{
    u32 val;

#if defined(__powerpc__)
    asm("lwbrx %0,%1,%2;eieio" : "=r"(val) : "b"(regindex), "r"(par->regbase));
#else
    val = readl(par->regbase + regindex);
#endif
    return val;
}

static inline void
_aty_st_le32(volatile unsigned int regindex, u32 val, 
				const struct aty128fb_par *par) 
{
#if defined(__powerpc__)
    asm("stwbrx %0,%1,%2;eieio" : : "r"(val), "b"(regindex),
                "r"(par->regbase) : "memory");
#else
    writel(val, par->regbase + regindex);
#endif
}

static inline u8
_aty_ld_8(unsigned int regindex, const struct aty128fb_par *par)
{
    return readb(par->regbase + regindex);
}

static inline void
_aty_st_8(unsigned int regindex, u8 val, const struct aty128fb_par *par)
{
    writeb (val, par->regbase + regindex);
}

#define aty_ld_le32(regindex)		_aty_ld_le32(regindex, par)
#define aty_st_le32(regindex, val)	_aty_st_le32(regindex, val, par)
#define aty_ld_8(regindex)		_aty_ld_8(regindex, par)
#define aty_st_8(regindex, val)		_aty_st_8(regindex, val, par)

    /*
     * Functions to read from/write to the pll registers
     */

#define aty_ld_pll(pll_index)		_aty_ld_pll(pll_index, par)
#define aty_st_pll(pll_index, val)	_aty_st_pll(pll_index, val, par)


static u32
_aty_ld_pll(unsigned int pll_index, const struct aty128fb_par *par)
{       
    aty_st_8(CLOCK_CNTL_INDEX, pll_index & 0x1F);
    return aty_ld_le32(CLOCK_CNTL_DATA);
}

    
static void
_aty_st_pll(unsigned int pll_index, u32 val, const struct aty128fb_par *par)
{
    aty_st_8(CLOCK_CNTL_INDEX, (pll_index & 0x1F) | PLL_WR_EN);
    aty_st_le32(CLOCK_CNTL_DATA, val);
}


/* return true when the PLL has completed an atomic update */
static int
aty_pll_readupdate(const struct aty128fb_par *par)
{
    return !(aty_ld_pll(PPLL_REF_DIV) & PPLL_ATOMIC_UPDATE_R);
}


static void
aty_pll_wait_readupdate(const struct aty128fb_par *par)
{
    unsigned long timeout = jiffies + HZ/100;	// should be more than enough
    int reset = 1;

    while (time_before(jiffies, timeout))
	if (aty_pll_readupdate(par)) {
	    reset = 0;
	    break;
	}

    if (reset)	/* reset engine?? */
	printk(KERN_DEBUG "aty128fb: PLL write timeout!\n");
}


/* tell PLL to update */
static void
aty_pll_writeupdate(const struct aty128fb_par *par)
{
    aty_pll_wait_readupdate(par);

    aty_st_pll(PPLL_REF_DIV,
	aty_ld_pll(PPLL_REF_DIV) | PPLL_ATOMIC_UPDATE_W);
}


/* write to the scratch register to test r/w functionality */
static int __init
register_test(const struct aty128fb_par *par)
{
    u32 val;
    int flag = 0;

    val = aty_ld_le32(BIOS_0_SCRATCH);

    aty_st_le32(BIOS_0_SCRATCH, 0x55555555);
    if (aty_ld_le32(BIOS_0_SCRATCH) == 0x55555555) {
	aty_st_le32(BIOS_0_SCRATCH, 0xAAAAAAAA);

	if (aty_ld_le32(BIOS_0_SCRATCH) == 0xAAAAAAAA)
	    flag = 1; 
    }

    aty_st_le32(BIOS_0_SCRATCH, val);	// restore value
    return flag;
}


    /*
     * Accelerator engine functions
     */
static void
do_wait_for_fifo(u16 entries, struct aty128fb_par *par)
{
    int i;

    for (;;) {
        for (i = 0; i < 2000000; i++) {
            par->fifo_slots = aty_ld_le32(GUI_STAT) & 0x0fff;
            if (par->fifo_slots >= entries)
                return;
        }
	aty128_reset_engine(par);
    }
}


static void
wait_for_idle(struct aty128fb_par *par)
{
    int i;

    do_wait_for_fifo(64, par);

    for (;;) {
        for (i = 0; i < 2000000; i++) {
            if (!(aty_ld_le32(GUI_STAT) & (1 << 31))) {
                aty128_flush_pixel_cache(par);
                par->blitter_may_be_busy = 0;
                return;
            }
        }
        aty128_reset_engine(par);
    }
}

static void
wait_for_fifo(u16 entries, struct aty128fb_par *par)
{
    if (par->fifo_slots < entries)
        do_wait_for_fifo(64, par);
    par->fifo_slots -= entries;
}

static void
aty128_flush_pixel_cache(const struct aty128fb_par *par)
{
    int i;
    u32 tmp;

    tmp = aty_ld_le32(PC_NGUI_CTLSTAT);
    tmp &= ~(0x00ff);
    tmp |= 0x00ff;
    aty_st_le32(PC_NGUI_CTLSTAT, tmp);

    for (i = 0; i < 2000000; i++)
        if (!(aty_ld_le32(PC_NGUI_CTLSTAT) & PC_BUSY))
            break;
}

static void
aty128_reset_engine(const struct aty128fb_par *par)
{
    u32 gen_reset_cntl, clock_cntl_index, mclk_cntl;

    aty128_flush_pixel_cache(par);

    clock_cntl_index = aty_ld_le32(CLOCK_CNTL_INDEX);
    mclk_cntl = aty_ld_pll(MCLK_CNTL);

    aty_st_pll(MCLK_CNTL, mclk_cntl | 0x00030000);

    gen_reset_cntl = aty_ld_le32(GEN_RESET_CNTL);
    aty_st_le32(GEN_RESET_CNTL, gen_reset_cntl | SOFT_RESET_GUI);
    aty_ld_le32(GEN_RESET_CNTL);
    aty_st_le32(GEN_RESET_CNTL, gen_reset_cntl & ~(SOFT_RESET_GUI));
    aty_ld_le32(GEN_RESET_CNTL);

    aty_st_pll(MCLK_CNTL, mclk_cntl);
    aty_st_le32(CLOCK_CNTL_INDEX, clock_cntl_index);
    aty_st_le32(GEN_RESET_CNTL, gen_reset_cntl);

    /* use old pio mode */
    aty_st_le32(PM4_BUFFER_CNTL, PM4_BUFFER_CNTL_NONPM4);

    DBG("engine reset");
}


static void
aty128_init_engine(struct fb_info *info)
{
    struct aty128fb_par *par = (struct aty128fb_par *) info->par;	
    u32 pitch_value;

    wait_for_idle(par);

    /* 3D scaler not spoken here */
    wait_for_fifo(1, par);
    aty_st_le32(SCALE_3D_CNTL, 0x00000000);

    aty128_reset_engine(par);

    pitch_value = par->crtc.pitch;
    if (par->crtc.bpp == 24) {
        pitch_value = pitch_value * 3;
    }

    wait_for_fifo(4, par);
    /* setup engine offset registers */
    aty_st_le32(DEFAULT_OFFSET, 0x00000000);

    /* setup engine pitch registers */
    aty_st_le32(DEFAULT_PITCH, pitch_value);

    /* set the default scissor register to max dimensions */
    aty_st_le32(DEFAULT_SC_BOTTOM_RIGHT, (0x1FFF << 16) | 0x1FFF);

    /* set the drawing controls registers */
    aty_st_le32(DP_GUI_MASTER_CNTL,
		GMC_SRC_PITCH_OFFSET_DEFAULT		|
		GMC_DST_PITCH_OFFSET_DEFAULT		|
		GMC_SRC_CLIP_DEFAULT			|
		GMC_DST_CLIP_DEFAULT			|
		GMC_BRUSH_SOLIDCOLOR			|
		(bpp_to_depth(par->crtc.bpp) << 8)	|
		GMC_SRC_DSTCOLOR			|
		GMC_BYTE_ORDER_MSB_TO_LSB		|
		GMC_DP_CONVERSION_TEMP_6500		|
		ROP3_PATCOPY				|
		GMC_DP_SRC_RECT				|
		GMC_3D_FCN_EN_CLR			|
		GMC_DST_CLR_CMP_FCN_CLEAR		|
		GMC_AUX_CLIP_CLEAR			|
		GMC_WRITE_MASK_SET);

    wait_for_fifo(8, par);
    /* clear the line drawing registers */
    aty_st_le32(DST_BRES_ERR, 0);
    aty_st_le32(DST_BRES_INC, 0);
    aty_st_le32(DST_BRES_DEC, 0);

    /* set brush color registers */
    aty_st_le32(DP_BRUSH_FRGD_CLR, 0xFFFFFFFF); /* white */
    aty_st_le32(DP_BRUSH_BKGD_CLR, 0x00000000); /* black */

    /* set source color registers */
    aty_st_le32(DP_SRC_FRGD_CLR, 0xFFFFFFFF);   /* white */
    aty_st_le32(DP_SRC_BKGD_CLR, 0x00000000);   /* black */

    /* default write mask */
    aty_st_le32(DP_WRITE_MASK, 0xFFFFFFFF);

    /* Wait for all the writes to be completed before returning */
    wait_for_idle(par);
}


/* convert bpp values to their register representation */
static u32
bpp_to_depth(u32 bpp)
{
    if (bpp <= 8)
	return DST_8BPP;
    else if (bpp <= 16)
        return DST_15BPP;
    else if (bpp <= 24)
	return DST_24BPP;
    else if (bpp <= 32)
	return DST_32BPP;

    return -EINVAL;
}

    /*
     * CRTC programming
     */

/* Program the CRTC registers */
static void
aty128_set_crtc(const struct aty128_crtc *crtc,
		const struct aty128fb_par *par)
{
    aty_st_le32(CRTC_GEN_CNTL, crtc->gen_cntl);
    aty_st_le32(CRTC_H_TOTAL_DISP, crtc->h_total);
    aty_st_le32(CRTC_H_SYNC_STRT_WID, crtc->h_sync_strt_wid);
    aty_st_le32(CRTC_V_TOTAL_DISP, crtc->v_total);
    aty_st_le32(CRTC_V_SYNC_STRT_WID, crtc->v_sync_strt_wid);
    aty_st_le32(CRTC_PITCH, crtc->pitch);
    aty_st_le32(CRTC_OFFSET, crtc->offset);
    aty_st_le32(CRTC_OFFSET_CNTL, crtc->offset_cntl);
    /* Disable ATOMIC updating.  Is this the right place?
     * -- BenH: Breaks on my G4
     */
#if 0
    aty_st_le32(PPLL_CNTL, aty_ld_le32(PPLL_CNTL) & ~(0x00030000));
#endif
}

static int
aty128_var_to_crtc(const struct fb_var_screeninfo *var,
			struct aty128_crtc *crtc,
			const struct aty128fb_par *par)
{
    u32 h_total, h_disp, h_sync_strt, h_sync_wid, h_sync_pol, c_sync;
    u32 v_total, v_disp, v_sync_strt, v_sync_wid, v_sync_pol;
    u8 hsync_strt_pix[5] = { 0, 0x12, 9, 6, 5 };
    u8 mode_bytpp[7] = { 0, 0, 1, 2, 2, 3, 4 };
    u32 depth, bytpp;

    depth = bpp_to_depth(var->bits_per_pixel);
    /* convert depth to bpp */
    bytpp = mode_bytpp[depth];

    h_total = (((var->xres + var->right_margin + var->hsync_len + var->left_margin) >> 3) - 1) & 0xFFFFL;
    v_total = (var->yres + var->upper_margin + var->vsync_len + var->lower_margin - 1) & 0xFFFFL;

    h_disp = (var->xres >> 3) - 1;
    v_disp = var->yres - 1;

    h_sync_wid = (var->hsync_len + 7) >> 3;
    if (h_sync_wid == 0)
	h_sync_wid = 1;
    else if (h_sync_wid > 0x3f)        /* 0x3f = max hwidth */
	h_sync_wid = 0x3f;

    h_sync_strt = h_disp + (var->right_margin >> 3);

    v_sync_wid = var->vsync_len;
    if (v_sync_wid == 0)
	v_sync_wid = 1;
    else if (v_sync_wid > 0x1f)        /* 0x1f = max vwidth */
	v_sync_wid = 0x1f;
    
    v_sync_strt = v_disp + var->lower_margin;

    h_sync_pol = var->sync & FB_SYNC_HOR_HIGH_ACT ? 0 : 1;
    v_sync_pol = var->sync & FB_SYNC_VERT_HIGH_ACT ? 0 : 1;
    
    c_sync = var->sync & FB_SYNC_COMP_HIGH_ACT ? (1 << 4) : 0;

    crtc->gen_cntl = 0x3000000L | c_sync | (depth << 8);

    crtc->h_total = h_total | (h_disp << 16);
    crtc->v_total = v_total | (v_disp << 16);

    crtc->h_sync_strt_wid = hsync_strt_pix[bytpp] | (h_sync_strt << 3) |
                (h_sync_wid << 16) | (h_sync_pol << 23);
    crtc->v_sync_strt_wid = v_sync_strt | (v_sync_wid << 16) |
                (v_sync_pol << 23);

    crtc->pitch = var->xres_virtual >> 3;

    crtc->offset = 0;
    crtc->offset_cntl = 0;
    crtc->bpp = var->bits_per_pixel;
    return 0;
}


static void
aty128_set_pll(struct aty128fb_par *par, const struct fb_info *info)
{
    struct aty128_pll *pll = &par->pll; 
    u32 div3;

    unsigned char post_conv[] =	/* register values for post dividers */
        { 2, 0, 1, 4, 2, 2, 6, 2, 3, 2, 2, 2, 7 };

    /* select PPLL_DIV_3 */
    aty_st_le32(CLOCK_CNTL_INDEX, aty_ld_le32(CLOCK_CNTL_INDEX) | (3 << 8));

    /* reset PLL */
    aty_st_pll(PPLL_CNTL,
		aty_ld_pll(PPLL_CNTL) | PPLL_RESET | PPLL_ATOMIC_UPDATE_EN);

    /* write the reference divider */
    aty_pll_wait_readupdate(par);
    aty_st_pll(PPLL_REF_DIV, par->constants.ref_divider & 0x3ff);
    aty_pll_writeupdate(par);

    div3 = aty_ld_pll(PPLL_DIV_3);
    div3 &= ~PPLL_FB3_DIV_MASK;
    div3 |= pll->feedback_divider;
    div3 &= ~PPLL_POST3_DIV_MASK;
    div3 |= post_conv[pll->post_divider] << 16;

    /* write feedback and post dividers */
    aty_pll_wait_readupdate(par);
    aty_st_pll(PPLL_DIV_3, div3);
    aty_pll_writeupdate(par);

    aty_pll_wait_readupdate(par);
    aty_st_pll(HTOTAL_CNTL, 0);	/* no horiz crtc adjustment */
    aty_pll_writeupdate(par);

    /* clear the reset, just in case */
    aty_st_pll(PPLL_CNTL, aty_ld_pll(PPLL_CNTL) & ~PPLL_RESET);
}

static int
aty128_var_to_pll(u32 period_in_ps, struct aty128fb_par *par,
			struct fb_info *info)
{
    const struct aty128_constants c = par->constants;
    unsigned char post_dividers[] = {1,2,4,8,3,6,12};
    struct aty128_pll *pll = &par->pll;
    u32 output_freq;
    u32 vclk;        /* in .01 MHz */
    int i;
    u32 n, d;

    vclk = 100000000 / period_in_ps;	/* convert units to 10 kHz */

    /* adjust pixel clock if necessary */
    if (vclk > c.ppll_max)
	vclk = c.ppll_max;
    if (vclk * 12 < c.ppll_min)
	vclk = c.ppll_min/12;

    /* now, find an acceptable divider */
    for (i = 0; i < sizeof(post_dividers); i++) {
	output_freq = post_dividers[i] * vclk;
	if (output_freq >= c.ppll_min && output_freq <= c.ppll_max)
	    break;
    }

    /* calculate feedback divider */
    n = c.ref_divider * output_freq;
    d = c.dotclock;

    pll->post_divider = post_dividers[i];
    pll->feedback_divider = round_div(n, d);
    pll->vclk = vclk;

    info->var.pixclock = 100000000 / pll->vclk;

    DBG("post %d feedback %d vlck %d output %d ref_divider %d "
			"vclk_per: %d\n", pll->post_divider,
			pll->feedback_divider, vclk, output_freq,
			c.ref_divider, period_in_ps);
    return 0;
}


static void
aty128_set_fifo(const struct aty128_ddafifo *dsp,
			const struct aty128fb_par *par)
{
    aty_st_le32(DDA_CONFIG, dsp->dda_config);
    aty_st_le32(DDA_ON_OFF, dsp->dda_on_off);
}


static int
aty128_ddafifo(struct aty128fb_par *par)
{
    const struct aty128_meminfo *m = par->mem;
    struct aty128_ddafifo *dsp = &par->fifo_reg;
    const struct aty128_pll *pll = &par->pll;
    u32 xclk = par->constants.xclk;
    u32 fifo_width = par->constants.fifo_width;
    u32 fifo_depth = par->constants.fifo_depth;
    s32 x, b, p, ron, roff;
    u32 n, d, bpp = par->crtc.bpp;

    /* 15bpp is really 16bpp */
    if (bpp == 15)
	bpp = 16;

    n = xclk * fifo_width;
    d = pll->vclk * bpp;
    x = round_div(n, d);

    ron = 4 * m->MB +
	3 * ((m->Trcd - 2 > 0) ? m->Trcd - 2 : 0) +
	2 * m->Trp +
	m->Twr +
	m->CL +
	m->Tr2w +
	x;

    DBG("x %x\n", x);

    b = 0;
    while (x) {
	x >>= 1;
	b++;
    }
    p = b + 1;

    ron <<= (11 - p);

    n <<= (11 - p);
    x = round_div(n, d);
    roff = x * (fifo_depth - 4);

    if ((ron + m->Rloop) >= roff) {
	printk(KERN_ERR "aty128fb: Mode out of range!\n");
	return -EINVAL;
    }

    DBG("p: %x rloop: %x x: %x ron: %x roff: %x\n",
			p, m->Rloop, x, ron, roff);

    dsp->dda_config = p << 16 | m->Rloop << 20 | x;
    dsp->dda_on_off = ron << 16 | roff;

    return 0;
}

static int 
aty128fb_check_var(struct fb_var_screeninfo *var, struct fb_info *info) 
{
    u8 mode_bytpp[7] = { 0, 0, 1, 2, 2, 3, 4 };
    u32 depth, bytpp, h_total, v_total;
    
    /* basic (in)sanity checks */
    if (!var->xres)
        var->xres = 1;
    if (!var->yres)
        var->yres = 1;
    if (var->xres > var->xres_virtual)
        var->xres_virtual = var->xres;
    if (var->yres > var->yres_virtual)
        var->yres_virtual = var->yres;

    switch (var->bits_per_pixel) {
        case 0 ... 8:
            var->bits_per_pixel = 8;
	    var->red.offset = 0;
	    var->red.length = 8;
	    var->green.offset = 0;
	    var->green.length = 8;
	    var->blue.offset = 0;
	    var->blue.length = 8;
	    var->transp.offset = 0;
	    var->transp.length = 0;
            break;
        case 9 ... 16:
	    var->bits_per_pixel = 16;
	    var->red.offset = 10;
	    var->red.length = 5;
	    var->green.offset = 5;
	    var->green.length = 5;
	    var->blue.offset = 0;
	    var->blue.length = 5;
	    var->transp.offset = 0;
	    var->transp.length = 0;
            break;
        case 17 ... 24:
            var->bits_per_pixel = 24;
            var->red.offset = 16;
            var->red.length = 8;
            var->green.offset = 8;
            var->green.length = 8;
            var->blue.offset = 0;
            var->blue.length = 8;
            var->transp.offset = 0;
            var->transp.length = 0;
            break;
        case 25 ... 32:
            var->bits_per_pixel = 32;
	    var->red.offset = 16;
	    var->red.length = 8;
	    var->green.offset = 8;
	    var->green.length = 8;
	    var->blue.offset = 0;
	    var->blue.length = 8;
	    var->transp.offset = 24;
	    var->transp.length = 8;
	    break;
    	default:
       	    printk(KERN_ERR "aty128fb: Invalid pixel width\n");
            return -EINVAL;
    }
    
    /* check for mode eligibility
     * accept only non interlaced modes */
    if ((var->vmode & FB_VMODE_MASK) != FB_VMODE_NONINTERLACED)
	return -EINVAL;
  
    /* convert (and round up) and validate */
    var->xres = (var->xres + 7) & ~7;
    var->xoffset = (var->xoffset + 7) & ~7;

    if (var->xres_virtual < var->xres + var->xoffset)
	var->xres_virtual = var->xres + var->xoffset;

    if (var->yres_virtual < var->yres + var->yoffset)
	var->yres_virtual = var->yres + var->yoffset;

    /* convert bpp into ATI register depth */
    depth = bpp_to_depth(var->bits_per_pixel);

    /* make sure we didn't get an invalid depth */
    if (depth == -EINVAL) {
        printk(KERN_ERR "aty128fb: Invalid depth\n");
        return -EINVAL;
    }

    /* convert depth to bpp */
    bytpp = mode_bytpp[depth];

    /* make sure there is enough video ram for the mode */
    if ((u32)(var->xres_virtual * var->yres_virtual * bytpp) > info->fix.smem_len) {
        printk(KERN_ERR "aty128fb: Not enough memory for mode\n");
        return -EINVAL;
    }

    h_total = (((var->xres + var->right_margin + var->hsync_len + var->left_margin) >> 3) - 1) & 0xFFFFL;
    v_total = (var->yres + var->upper_margin + var->vsync_len + var->lower_margin - 1) & 0xFFFFL;

    /* check to make sure h_total and v_total are in range */
    if (((h_total >> 3) - 1) > 0x1ff || (v_total - 1) > 0x7FF) {
        printk(KERN_ERR "aty128fb: invalid width ranges\n");
        return -EINVAL;
    }

    var->red.msb_right = 0;
    var->green.msb_right = 0;
    var->blue.msb_right = 0;
    var->transp.msb_right = 0;

    var->nonstd = 0;
    var->activate = 0;

    var->height = -1;
    var->width = -1;
    var->accel_flags = FB_ACCELF_TEXT;
    return 0;	
}

/*
 * This actually sets the video mode.
 */
static int aty128fb_set_par(struct fb_info *info)
{ 
    struct aty128fb_par *par;
    u32 config;

    info->par = par = &default_par;

    aty128_var_to_crtc(&info->var, &par->crtc, par);
    aty128_var_to_pll(info->var.pixclock, par, info);
    aty128_ddafifo(par);

    if (par->blitter_may_be_busy)
        wait_for_idle(par);

    /* clear all registers that may interfere with mode setting */
    aty_st_le32(OVR_CLR, 0);
    aty_st_le32(OVR_WID_LEFT_RIGHT, 0);
    aty_st_le32(OVR_WID_TOP_BOTTOM, 0);
    aty_st_le32(OV0_SCALE_CNTL, 0);
    aty_st_le32(MPP_TB_CONFIG, 0);
    aty_st_le32(MPP_GP_CONFIG, 0);
    aty_st_le32(SUBPIC_CNTL, 0);
    aty_st_le32(VIPH_CONTROL, 0);
    aty_st_le32(I2C_CNTL_1, 0);         /* turn off i2c */
    aty_st_le32(GEN_INT_CNTL, 0);	/* turn off interrupts */
    aty_st_le32(CAP0_TRIG_CNTL, 0);
    aty_st_le32(CAP1_TRIG_CNTL, 0);

    aty_st_8(CRTC_EXT_CNTL + 1, 4);	/* turn video off */

    aty128_set_crtc(&par->crtc, par);
    aty128_set_pll(par, info);
    aty128_set_fifo(&par->fifo_reg, par);

    config = aty_ld_le32(CONFIG_CNTL) & ~3;

#if defined(__BIG_ENDIAN)
    if (par->crtc.bpp >= 24)
	config |= 2;	/* make aperture do 32 byte swapping */
    else if (par->crtc.bpp > 8)
	config |= 1;	/* make aperture do 16 byte swapping */
#endif

    aty_st_le32(CONFIG_CNTL, config);
    aty_st_8(CRTC_EXT_CNTL + 1, 0);	/* turn the video back on */

    if (info->var.accel_flags & FB_ACCELF_TEXT)
        aty128_init_engine(info);

    info->fix.line_length = (info->var.xres_virtual * par->crtc.bpp) >> 3;
    info->fix.visual = par->crtc.bpp <= 8 ? FB_VISUAL_PSEUDOCOLOR : FB_VISUAL_DIRECTCOLOR;

    return 0;	
}

    /*
     *  Pan or Wrap the Display
     *
     *  Not supported (yet!)
     */
static int
aty128fb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
    struct aty128fb_par *par = (struct aty128fb_par *) info->par;
    u32 xoffset, yoffset;
    u32 offset;
    u32 xres, yres;

    xres = (((par->crtc.h_total >> 16) & 0xff) + 1) << 3;
    yres = ((par->crtc.v_total >> 16) & 0x7ff) + 1;

    xoffset = (var->xoffset +7) & ~7;
    yoffset = var->yoffset;

    if (xoffset+xres > info->var.xres_virtual || yoffset+yres > info->var.yres_virtual)
        return -EINVAL;

    info->var.xoffset = xoffset;
    info->var.yoffset = yoffset;

    offset = ((yoffset * info->var.xres_virtual + xoffset) * par->crtc.bpp) >> 6;
    aty_st_le32(CRTC_OFFSET, offset);
    return 0;
}

int __init
aty128fb_setup(char *options)
{
    char *this_opt;

    if (!options || !*options)
	return 0;

    while ((this_opt = strsep(&options, ","))) {
	if (!strncmp(this_opt, "font:", 5)) {
	    char *p;
	    int i;
	    
	    p = this_opt +5;
	    for (i = 0; i < sizeof(fontname) - 1; i++)
		if (!*p || *p == ' ' || *p == ',')
		    break;
	    memcpy(fontname, this_opt + 5, i);
	    fontname[i] = 0;
	} else if (!strncmp(this_opt, "noaccel", 7)) {
	    noaccel = 1;
        }
        else
            mode_option = this_opt;
    }
    return 0;
}


    /*
     *  Initialisation
     */

static int __init
aty128_init(struct fb_info *info, struct pci_dev *pdev, const char *name)
{
    struct fb_var_screeninfo var;
    struct aty128fb_par *par = &default_par;	
    u32 dac;
    u8 chip_rev;
    const struct aty128_chip_info *aci = &aty128_pci_probe_list[0];
    char *video_card = "Rage128";

    if (!info->fix.smem_len)	/* may have already been probed */
	info->fix.smem_len = aty_ld_le32(CONFIG_MEMSIZE) & 0x03FFFFFF;

    /* Get the chip revision */
    chip_rev = (aty_ld_le32(CONFIG_CNTL) >> 16) & 0x1F;

    /* put a name with the face */
    while (aci->name && pdev->device != aci->device) { aci++; }
    video_card = (char *)aci->name;
    default_par.chip_gen = aci->chip_gen;

    printk(KERN_INFO "aty128fb: %s [chip rev 0x%x] ", video_card, chip_rev);

    if (info->fix.smem_len % (1024 * 1024) == 0)
	printk("%dM %s\n", info->fix.smem_len / (1024*1024), default_par.mem->name);
    else
	printk("%dk %s\n", info->fix.smem_len / 1024, default_par.mem->name);

    /* fill in info */
    info->node  = -1;
    info->fbops = &aty128fb_ops;
    info->flags = FBINFO_FLAG_DEFAULT;

    var = default_var;
#ifdef CONFIG_PPC
    if (_machine == _MACH_Pmac) {
        if (mode_option) {
            if (!mac_find_mode(&var, info, mode_option, 8))
                var = default_var;
        }
    } else
#endif /* CONFIG_PPC */
    {
        if (fb_find_mode(&var, info, mode_option, NULL, 0,
                          &defaultmode, 8) == 0)
            var = default_var;
    }

    if (noaccel)
        var.accel_flags &= ~FB_ACCELF_TEXT;
    else
        var.accel_flags |= FB_ACCELF_TEXT;

    if (aty128fb_check_var(&var, info)) {
	printk(KERN_ERR "aty128fb: Cannot set default mode.\n");
	return 0;
    }

    info->var = var;	
    info->par = &default_par;	

    /* setup the DAC the way we like it */
    dac = aty_ld_le32(DAC_CNTL);
    dac |= (DAC_8BIT_EN | DAC_RANGE_CNTL);
    dac |= DAC_MASK;
    aty_st_le32(DAC_CNTL, dac);

    /* turn off bus mastering, just in case */
    aty_st_le32(BUS_CNTL, aty_ld_le32(BUS_CNTL) | BUS_MASTER_DIS);

    //board_list = aty128_board_list_add(board_list, info);

    if (register_framebuffer(info) < 0)
	return 0;

#ifdef CONFIG_PMAC_BACKLIGHT
    /* Could be extended to Rage128Pro LVDS output too */
    if (default_par.chip_gen == rage_M3)
    	register_backlight_controller(&aty128_backlight_controller, info, "ati");
#endif /* CONFIG_PMAC_BACKLIGHT */

    printk(KERN_INFO "fb%d: %s frame buffer device on %s\n",
	   GET_FB_IDX(info->node), info->fix.id, name);
    return 1;	/* success! */
}


/* add a new card to the list  ++ajoshi */
static struct
fb_info_aty128 *aty128_board_list_add(struct fb_info_aty128 *board_list,
                                       struct fb_info_aty128 *new_node)
{
    struct fb_info_aty128 *i_p = board_list;

    new_node->next = NULL;
    if(board_list == NULL)
	return new_node;
    while(i_p->next != NULL)
	i_p = i_p->next;
    i_p->next = new_node;

    return board_list;
}


int __init
aty128fb_init(void)
{
#ifdef CONFIG_PCI
    struct pci_dev *pdev = NULL;
    const struct aty128_chip_info *aci = &aty128_pci_probe_list[0];

    while (aci->name != NULL) {
        pdev = pci_find_device(PCI_VENDOR_ID_ATI, aci->device, pdev);
        while (pdev != NULL) {
            if (aty128_pci_register(pdev, aci) == 0)
                return 0;
            pdev = pci_find_device(PCI_VENDOR_ID_ATI, aci->device, pdev);
        }
	aci++;
    }
#endif

    return 0;
}


#ifdef CONFIG_PCI
/* register a card    ++ajoshi */
static int __init
aty128_pci_register(struct pci_dev *pdev,
                               const struct aty128_chip_info *aci)
{
	struct fb_info *info = NULL;
	struct aty128fb_par *par = &default_par;
	unsigned long fb_addr, reg_addr;
	int err;
#if !defined(CONFIG_PPC) && !defined(__sparc__)
	char *bios_seg = NULL;
#endif

	/* Enable device in PCI config */
	if ((err = pci_enable_device(pdev))) {
		printk(KERN_ERR "aty128fb: Cannot enable PCI device: %d\n",
				err);
		goto err_out;
	}

	fb_addr = pci_resource_start(pdev, 0);
	if (!request_mem_region(fb_addr, pci_resource_len(pdev, 0),
				"aty128fb FB")) {
		printk(KERN_ERR "aty128fb: cannot reserve frame "
				"buffer memory\n");
		goto err_free_fb;
	}

	reg_addr = pci_resource_start(pdev, 2);
	if (!request_mem_region(reg_addr, pci_resource_len(pdev, 2),
				"aty128fb MMIO")) {
		printk(KERN_ERR "aty128fb: cannot reserve MMIO region\n");
		goto err_free_mmio;
	}

	/* We have the resources. Now virtualize them */
	if (!(info = kmalloc(sizeof(struct fb_info), GFP_ATOMIC))) {
		printk(KERN_ERR "aty128fb: can't alloc fb_info_aty128\n");
		goto err_unmap_out;
	}
	memset(info, 0, sizeof(struct fb_info));

	/* Copy PCI device info into info->pdev */
	default_par.pdev = pdev;

	info->fix = aty128fb_fix;
	
	/* Virtualize mmio region */
    	info->fix.mmio_start = (unsigned long) reg_addr;

	default_par.regbase = ioremap(reg_addr, 0x1FFF);

	if (!default_par.regbase)
		goto err_free_info;

	/* Grab memory size from the card */
	info->fix.smem_len = aty_ld_le32(CONFIG_MEMSIZE) & 0x03FFFFFF;

	/* Virtualize the framebuffer */
	info->fix.smem_start = (unsigned long) fb_addr;
	info->screen_base = ioremap(fb_addr, info->fix.smem_len);

	if (!info->screen_base) {
		iounmap((void *)default_par.regbase);
		goto err_free_info;
	}

	/* If we can't test scratch registers, something is seriously wrong */
	if (!register_test(&default_par)) {
		printk(KERN_ERR "aty128fb: Can't write to video register!\n");
		goto err_out;
	}

#if !defined(CONFIG_PPC) && !defined(__sparc__)
	if (!(bios_seg = aty128find_ROM(&default_par)))
		printk(KERN_INFO "aty128fb: Rage128 BIOS not located. "
					"Guessing...\n");
	else {
		printk(KERN_INFO "aty128fb: Rage128 BIOS located at "
				"segment %4.4X\n", (unsigned int)bios_seg);
		aty128_get_pllinfo(&default_par, bios_seg);
	}
#endif
	info->pseudo_palette = &aty128fb_pseudo_palette;
	aty128_timings(&default_par);

	if (!aty128_init(info, pdev, "PCI"))
		goto err_out;
	return 0;

err_out:
	iounmap(info->screen_base);
	iounmap(default_par.regbase);
err_free_info:
	kfree(info);
err_unmap_out:
	release_mem_region(pci_resource_start(pdev, 2),
			pci_resource_len(pdev, 2));
err_free_mmio:
	release_mem_region(pci_resource_start(pdev, 0),
			pci_resource_len(pdev, 0));
err_free_fb:
	release_mem_region(pci_resource_start(pdev, 1),
			pci_resource_len(pdev, 1));
	return -ENODEV;
}
#endif /* CONFIG_PCI */


/* PPC and Sparc cannot read video ROM */
#if !defined(CONFIG_PPC) && !defined(__sparc__)
static char __init
*aty128find_ROM(struct aty128fb_par *par)
{
	u32  segstart;
	char *rom_base;
	char *rom;
	int  stage;
	int  i;
	char aty_rom_sig[] = "761295520";   /* ATI ROM Signature      */
	char R128_sig[] = "R128";           /* Rage128 ROM identifier */

	for (segstart=0x000c0000; segstart<0x000f0000; segstart+=0x00001000) {
        	stage = 1;

		rom_base = (char *)ioremap(segstart, 0x1000);

		if ((*rom_base == 0x55) && (((*(rom_base + 1)) & 0xff) == 0xaa))
			stage = 2;

		if (stage != 2) {
			iounmap(rom_base);
			continue;
		}
		rom = rom_base;

		for (i = 0; (i < 128 - strlen(aty_rom_sig)) && (stage != 3); i++) {
			if (aty_rom_sig[0] == *rom)
				if (strncmp(aty_rom_sig, rom,
						strlen(aty_rom_sig)) == 0)
					stage = 3;
			rom++;
		}
		if (stage != 3) {
			iounmap(rom_base);
			continue;
		}
		rom = rom_base;

		/* ATI signature found.  Let's see if it's a Rage128 */
		for (i = 0; (i < 512) && (stage != 4); i++) {
			if (R128_sig[0] == *rom)
				if (strncmp(R128_sig, rom, 
						strlen(R128_sig)) == 0)
					stage = 4;
			rom++;
		}
		if (stage != 4) {
			iounmap(rom_base);
			continue;
		}

		return rom_base;
	}

	return NULL;
}


static void __init
aty128_get_pllinfo(struct aty128fb_par *par, char *bios_seg)
{
	void *bios_header;
	void *header_ptr;
	u16 bios_header_offset, pll_info_offset;
	PLL_BLOCK pll;

	bios_header = bios_seg + 0x48L;
	header_ptr  = bios_header;

	bios_header_offset = readw(header_ptr);
	bios_header = bios_seg + bios_header_offset;
	bios_header += 0x30;

	header_ptr = bios_header;
	pll_info_offset = readw(header_ptr);
	header_ptr = bios_seg + pll_info_offset;

	memcpy_fromio(&pll, header_ptr, 50);

	par->constants.ppll_max = pll.PCLK_max_freq;
	par->constants.ppll_min = pll.PCLK_min_freq;
	par->constants.xclk = (u32)pll.XCLK;
	par->constants.ref_divider = (u32)pll.PCLK_ref_divider;
	par->constants.dotclock = (u32)pll.PCLK_ref_freq;

	DBG("ppll_max %d ppll_min %d xclk %d ref_divider %d dotclock %d\n",
			par->constants.ppll_max, par->constants.ppll_min,
			par->constants.xclk, par->constants.ref_divider,
			par->constants.dotclock);

}           
#endif /* !CONFIG_PPC */


/* fill in known card constants if pll_block is not available */
static void __init
aty128_timings(struct aty128fb_par *par)
{
#ifdef CONFIG_PPC
    /* instead of a table lookup, assume OF has properly
     * setup the PLL registers and use their values
     * to set the XCLK values and reference divider values */

    u32 x_mpll_ref_fb_div;
    u32 xclk_cntl;
    u32 Nx, M;
    unsigned PostDivSet[] =
        { 0, 1, 2, 4, 8, 3, 6, 12 };
#endif

    if (!par->constants.dotclock)
        par->constants.dotclock = 2950;

#ifdef CONFIG_PPC
    x_mpll_ref_fb_div = aty_ld_pll(X_MPLL_REF_FB_DIV);
    xclk_cntl = aty_ld_pll(XCLK_CNTL) & 0x7;
    Nx = (x_mpll_ref_fb_div & 0x00ff00) >> 8;
    M  = x_mpll_ref_fb_div & 0x0000ff;

    par->constants.xclk = round_div((2 * Nx *
       	par->constants.dotclock), (M * PostDivSet[xclk_cntl]));

    par->constants.ref_divider =
        aty_ld_pll(PPLL_REF_DIV) & PPLL_REF_DIV_MASK;
#endif

    if (!par->constants.ref_divider) {
        par->constants.ref_divider = 0x3b;

        aty_st_pll(X_MPLL_REF_FB_DIV, 0x004c4c1e);
        aty_pll_writeupdate(par);
    }
    aty_st_pll(PPLL_REF_DIV, par->constants.ref_divider);
    aty_pll_writeupdate(par);

    /* from documentation */
    if (!par->constants.ppll_min)
        par->constants.ppll_min = 12500;
    if (!par->constants.ppll_max)
        par->constants.ppll_max = 25000;    /* 23000 on some cards? */
    if (!par->constants.xclk)
        par->constants.xclk = 0x1d4d;	     /* same as mclk */

    par->constants.fifo_width = 128;
    par->constants.fifo_depth = 32;

    switch (aty_ld_le32(MEM_CNTL) & 0x3) {
    case 0:
	par->mem = &sdr_128;
	break;
    case 1:
	par->mem = &sdr_sgram;
	break;
    case 2:
	par->mem = &ddr_sgram;
	break;
    default:
	par->mem = &sdr_sgram;
    }
}

    /*
     *  Blank the display.
     */
static int aty128fb_blank(int blank, struct fb_info *info)
{
    struct aty128fb_par *par = (struct aty128fb_par *) info->par;
    u8 state = 0;

#ifdef CONFIG_PMAC_BACKLIGHT
    if ((_machine == _MACH_Pmac) && blank)
    	set_backlight_enable(0);
#endif /* CONFIG_PMAC_BACKLIGHT */

    if (blank & VESA_VSYNC_SUSPEND)
	state |= 2;
    if (blank & VESA_HSYNC_SUSPEND)
	state |= 1;
    if (blank & VESA_POWERDOWN)
	state |= 4;

    aty_st_8(CRTC_EXT_CNTL+1, state);

#ifdef CONFIG_PMAC_BACKLIGHT
    if ((_machine == _MACH_Pmac) && !blank)
    	set_backlight_enable(1);
#endif /* CONFIG_PMAC_BACKLIGHT */
    return 0;	
}

    /*
     *  Set a single color register. The values supplied are already
     *  rounded down to the hardware's capabilities (according to the
     *  entries in the var structure). Return != 0 for invalid regno.
     */
static int
aty128fb_setcolreg(u_int regno, u_int red, u_int green, u_int blue,
                         u_int transp, struct fb_info *info)
{
    struct aty128fb_par *par = (struct aty128fb_par *) info->par;	
    u32 col;

    if (regno > 255)
	return 1;

    red >>= 8;
    green >>= 8;
    blue >>= 8;

    /* Note: For now, on M3, we set palette on both heads, which may
     * be useless. Can someone with a M3 check this ? */

    /* initialize gamma ramp for hi-color+ */

    if ((par->crtc.bpp > 8) && (regno == 0)) {
        int i;

        if (par->chip_gen == rage_M3)
            aty_st_le32(DAC_CNTL, aty_ld_le32(DAC_CNTL) & ~DAC_PALETTE_ACCESS_CNTL);

        for (i=16; i<256; i++) {
            aty_st_8(PALETTE_INDEX, i);
            col = (i << 16) | (i << 8) | i;
            aty_st_le32(PALETTE_DATA, col);
        }

        if (par->chip_gen == rage_M3) {
            aty_st_le32(DAC_CNTL, aty_ld_le32(DAC_CNTL) | DAC_PALETTE_ACCESS_CNTL);

            for (i=16; i<256; i++) {
                aty_st_8(PALETTE_INDEX, i);
                col = (i << 16) | (i << 8) | i;
                aty_st_le32(PALETTE_DATA, col);
            }
        }
    }

    /* initialize palette */
    if (par->chip_gen == rage_M3)
        aty_st_le32(DAC_CNTL, aty_ld_le32(DAC_CNTL) & ~DAC_PALETTE_ACCESS_CNTL);

    if (par->crtc.bpp == 16)
        aty_st_8(PALETTE_INDEX, (regno << 3));
    else
        aty_st_8(PALETTE_INDEX, regno);
    col = (red << 16) | (green << 8) | blue;
    aty_st_le32(PALETTE_DATA, col);
    if (par->chip_gen == rage_M3) {
    	aty_st_le32(DAC_CNTL, aty_ld_le32(DAC_CNTL) | DAC_PALETTE_ACCESS_CNTL);
        if (par->crtc.bpp == 16)
            aty_st_8(PALETTE_INDEX, (regno << 3));
        else
            aty_st_8(PALETTE_INDEX, regno);
        aty_st_le32(PALETTE_DATA, col);
    }

    if (regno < 16)
	switch (par->crtc.bpp) {
#ifdef FBCON_HAS_CFB16
	case 9 ... 16:
	    ((u16*)(info->pseudo_palette))[regno] = (regno << 10) | (regno << 5) | regno;
	    break;
#endif
#ifdef FBCON_HAS_CFB24
	case 17 ... 24:
	    ((u32*)(info->pseudo_palette))[regno] = (regno << 16) | (regno << 8) | regno;
	    break;
#endif
#ifdef FBCON_HAS_CFB32
	case 25 ... 32: {
            u32 i;

            i = (regno << 8) | regno;
            ((u16*)(info->pseudo_palette))[regno] = (i << 16) | i;
	    break;
        }
#endif
    }
    return 0;
}


#ifdef CONFIG_PMAC_BACKLIGHT
static int backlight_conv[] = {
	0xff, 0xc0, 0xb5, 0xaa, 0x9f, 0x94, 0x89, 0x7e,
	0x73, 0x68, 0x5d, 0x52, 0x47, 0x3c, 0x31, 0x24
};

static int
aty128_set_backlight_enable(int on, int level, void* data)
{
	struct aty128fb_par *par = (struct aty128fb_par *) data;
	unsigned int reg = aty_ld_le32(LVDS_GEN_CNTL);
	
	reg |= LVDS_BL_MOD_EN | LVDS_BLON;
	if (on && level > BACKLIGHT_OFF) {
		reg &= ~LVDS_BL_MOD_LEVEL_MASK;
		reg |= (backlight_conv[level] << LVDS_BL_MOD_LEVEL_SHIFT);
	} else {
		reg &= ~LVDS_BL_MOD_LEVEL_MASK;
		reg |= (backlight_conv[0] << LVDS_BL_MOD_LEVEL_SHIFT);
	}
	aty_st_le32(LVDS_GEN_CNTL, reg);
	return 0;
}

static int
aty128_set_backlight_level(int level, void* data)
{
	return aty128_set_backlight_enable(1, level, data);
}
#endif /* CONFIG_PMAC_BACKLIGHT */

    /*
     *  Accelerated functions
     */

static inline void
aty128_rectcopy(int srcx, int srcy, int dstx, int dsty,
		u_int width, u_int height,
		struct fb_info *info)
{
    struct aty128fb_par *par = (struct aty128fb_par *) info->par;	
    u32 save_dp_datatype, save_dp_cntl, bppval;

    if (!width || !height)
        return;

    bppval = bpp_to_depth(par->crtc.bpp);
    if (bppval == DST_24BPP) {
        srcx *= 3;
        dstx *= 3;
        width *= 3;
    } else if (bppval == -EINVAL) {
        printk("aty128fb: invalid depth\n");
        return;
    }

    wait_for_fifo(2, par);
    save_dp_datatype = aty_ld_le32(DP_DATATYPE);
    save_dp_cntl     = aty_ld_le32(DP_CNTL);

    wait_for_fifo(6, par);
    aty_st_le32(SRC_Y_X, (srcy << 16) | srcx);
    aty_st_le32(DP_MIX, ROP3_SRCCOPY | DP_SRC_RECT);
    aty_st_le32(DP_CNTL, DST_X_LEFT_TO_RIGHT | DST_Y_TOP_TO_BOTTOM);
    aty_st_le32(DP_DATATYPE, save_dp_datatype | bppval | SRC_DSTCOLOR);

    aty_st_le32(DST_Y_X, (dsty << 16) | dstx);
    aty_st_le32(DST_HEIGHT_WIDTH, (height << 16) | width);

    par->blitter_may_be_busy = 1;

    wait_for_fifo(2, par);
    aty_st_le32(DP_DATATYPE, save_dp_datatype);
    aty_st_le32(DP_CNTL, save_dp_cntl); 
}

#ifdef MODULE
MODULE_AUTHOR("(c)1999-2000 Brad Douglas <brad@neruo.com>");
MODULE_DESCRIPTION("FBDev driver for ATI Rage128 / Pro cards");
MODULE_PARM(noaccel, "i");
MODULE_PARM_DESC(noaccel, "Disable hardware acceleration (0 or 1=disabled) (default=0)");
MODULE_PARM(mode, "s");
MODULE_PARM_DESC(mode, "Specify resolution as \"<xres>x<yres>[-<bpp>][@<refresh>]\" ");

int __init
init_module(void)
{
    if (noaccel) {
        noaccel = 1;
        printk(KERN_INFO "aty128fb: Parameter NOACCEL set\n");
    }
    if (mode) {
        mode_option = mode;
        printk(KERN_INFO "aty128fb: Parameter MODE set to %s\n", mode);
    }
    aty128fb_init();
    return 0;
}

void __exit
cleanup_module(void)
{
    struct fb_info_aty128 *list = board_list;
    struct aty128fb_par *par;
    struct fb_info *info;	

    while (list) {
      	info = list->fb_info; 
	list = list->next;
	par = info->par;	
	
        unregister_framebuffer(&info);
        
	iounmap(par->regbase);
        iounmap(info->screen_base);

        release_mem_region(pci_resource_start(par->pdev, 0),
                           pci_resource_len(par->pdev, 0));
        release_mem_region(pci_resource_start(par->pdev, 1),
                           pci_resource_len(par->pdev, 1));
        release_mem_region(pci_resource_start(par->pdev, 2),
                           pci_resource_len(par->pdev, 2));
        kfree(info);
    }
}
#endif /* MODULE */