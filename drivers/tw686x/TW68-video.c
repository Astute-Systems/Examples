/*
 * device driver for TW6868 based PCIe analog video capture cards
 * video4linux video interface
 *
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/sort.h>

#include <media/v4l2-common.h>
#include "TW68.h"
#include "TW68_defines.h"
#include "s812ioctl.h"

#define TW686X_VIDSTAT_VDLOSS BIT(7)
#define TW686X_VIDSTAT_HLOCK BIT(6)
#define VDREG8(a0) ((const u16[8]){                 \
	a0 + 0x000, a0 + 0x010, a0 + 0x020, a0 + 0x030, \
	a0 + 0x100, a0 + 0x110, a0 + 0x120, a0 + 0x130})
#define VIDSTAT VDREG8(0x100)

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 7, 0)
#error "kernel version not supported. Please see README.txt"
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 10, 0)
void v4l2_get_timestamp(struct timeval *tv)
{
	struct timespec ts;

	ktime_get_ts(&ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / NSEC_PER_USEC;
}
#endif
/* const was added to some v4l2_ioctl_ops (see v4l2-ioctl.h) */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
#define V4L_CONST const
#else
#define V4L_CONST
#endif

unsigned int video_debug;
static unsigned int gbuffers = 8;
static unsigned int noninterlaced; /* 0 */
static unsigned int gbufsize = 800 * 576 * 4;
static unsigned int gbufsize_max = 800 * 576 * 4;
static unsigned int sfield = 1;
static unsigned int vidstd_open = -1; // -1 == autodetect, 1 = NTSC, 2==PAL
// this setting is overriden if driver compiled with
// ntsc_op =y or pal_op =y
module_param(vidstd_open, int, 0644);
MODULE_PARM_DESC(vidstd_open, "-1 == autodetect 1==NTSC, 2==PAL. Othervalues defualt to autodetect Note: this setting is overriden if driver compiled with ntsc_op =y or pal_op =y");

static unsigned int video_nr[] = {[0 ...(TW68_MAXBOARDS - 1)] = UNSET};

module_param_array(video_nr, int, NULL, 0444);
MODULE_PARM_DESC(video_nr, "video device number");
module_param(sfield, int, 0644);
MODULE_PARM_DESC(sfield, "single field mode (half size image will not be scaled from the full image");
module_param(video_debug, int, 0644);
MODULE_PARM_DESC(video_debug, "enable debug messages [video]");
module_param(gbuffers, int, 0444);
MODULE_PARM_DESC(gbuffers, "number of capture buffers, range 2-32");
module_param(noninterlaced, int, 0644);
MODULE_PARM_DESC(noninterlaced, "capture non interlaced video");

#define dprintk(fmt, arg...) \
	if (video_debug & 0x04)  \
	printk(KERN_DEBUG "%s/video: " fmt, dev->name, ##arg)

static struct TW68_format formats[] = {
	{
		.name = "15 bpp RGB, le",
		.fourcc = V4L2_PIX_FMT_RGB555,
		.depth = 16,
		.pm = 0x13 | 0x80,
	},
	{
		.name = "16 bpp RGB, le",
		.fourcc = V4L2_PIX_FMT_RGB565,
		.depth = 16,
		.pm = 0x10 | 0x80,
	},
	{
		.name = "4:2:2 packed, YUYV",
		.fourcc = V4L2_PIX_FMT_YUYV,
		.depth = 16,
		.pm = 0x00,
		.bswap = 1,
		.yuv = 1,
	},
	{
		.name = "4:2:2 packed, UYVY",
		.fourcc = V4L2_PIX_FMT_UYVY,
		.depth = 16,
		.pm = 0x00,
		.yuv = 1,
	}};

#define FORMATS ARRAY_SIZE(formats)

#define NORM_625_50       \
	.h_start = 0,         \
	.h_stop = 719,        \
	.video_v_start = 24,  \
	.video_v_stop = 311,  \
	.vbi_v_start_0 = 7,   \
	.vbi_v_stop_0 = 22,   \
	.vbi_v_start_1 = 319, \
	.src_timing = 4

#define NORM_525_60       \
	.h_start = 0,         \
	.h_stop = 719,        \
	.video_v_start = 23,  \
	.video_v_stop = 262,  \
	.vbi_v_start_0 = 10,  \
	.vbi_v_stop_0 = 21,   \
	.vbi_v_start_1 = 273, \
	.src_timing = 7

static struct TW68_tvnorm tvnorms[] = {
	{
		.name = "PAL", /* autodetect */
		.id = V4L2_STD_PAL,
		NORM_625_50,
		.sync_control = 0x18,
		.luma_control = 0x40,
		.chroma_ctrl1 = 0x81,
		.chroma_gain = 0x2a,
		.chroma_ctrl2 = 0x06,
		.vgate_misc = 0x1c,
	},
	{
		.name = "PAL-BG",
		.id = V4L2_STD_PAL_BG,
		NORM_625_50,
		.sync_control = 0x18,
		.luma_control = 0x40,
		.chroma_ctrl1 = 0x81,
		.chroma_gain = 0x2a,
		.chroma_ctrl2 = 0x06,
		.vgate_misc = 0x1c,
	},
	{
		.name = "PAL-I",
		.id = V4L2_STD_PAL_I,
		NORM_625_50,
		.sync_control = 0x18,
		.luma_control = 0x40,
		.chroma_ctrl1 = 0x81,
		.chroma_gain = 0x2a,
		.chroma_ctrl2 = 0x06,
		.vgate_misc = 0x1c,
	},
	{
		.name = "PAL-DK",
		.id = V4L2_STD_PAL_DK,
		NORM_625_50,
		.sync_control = 0x18,
		.luma_control = 0x40,
		.chroma_ctrl1 = 0x81,
		.chroma_gain = 0x2a,
		.chroma_ctrl2 = 0x06,
		.vgate_misc = 0x1c,
	},
	{
		.name = "NTSC",
		.id = V4L2_STD_NTSC,
		NORM_525_60,

		.sync_control = 0x59,
		.luma_control = 0x40,
		.chroma_ctrl1 = 0x89,
		.chroma_gain = 0x2a,
		.chroma_ctrl2 = 0x0e,
		.vgate_misc = 0x18,
	}};

#define TVNORMS ARRAY_SIZE(tvnorms)
#define V4L2_CID_PRIVATE_LASTP1 (V4L2_CID_PRIVATE_BASE + 4)

static const struct v4l2_queryctrl video_ctrls[] = {
	{
		.id = V4L2_CID_BRIGHTNESS,
		.name = "Brightness",
		.minimum = 0,
		.maximum = 255,
		.step = 1,
		.default_value = 125,
		.type = V4L2_CTRL_TYPE_INTEGER,
	},
	{
		.id = V4L2_CID_CONTRAST,
		.name = "Contrast",
		.minimum = 0,
		.maximum = 200,
		.step = 1,
		.default_value = 96,
		.type = V4L2_CTRL_TYPE_INTEGER,
	},
	{
		.id = V4L2_CID_SATURATION,
		.name = "Saturation",
		.minimum = 0,
		.maximum = 127,
		.step = 1,
		.default_value = 64,
		.type = V4L2_CTRL_TYPE_INTEGER,
	},
	{
		.id = V4L2_CID_HUE,
		.name = "Hue",
		.minimum = -124,
		.maximum = 125,
		.step = 1,
		.default_value = 0,
		.type = V4L2_CTRL_TYPE_INTEGER,
	},
	{
		.id = V4L2_CID_AUDIO_MUTE,
		.name = "Mute",
		.minimum = 0,
		.maximum = 1,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
	},
	{
		.id = V4L2_CID_AUDIO_VOLUME,
		.name = "Volume",
		.minimum = -15,
		.maximum = 15,
		.step = 1,
		.default_value = 0,
		.type = V4L2_CTRL_TYPE_INTEGER,
	},
};

static const unsigned int CTRLS = ARRAY_SIZE(video_ctrls);
static int TW68_s_ctrl(struct v4l2_ctrl *ctrl);
static int TW68_g_volatile_ctrl(struct v4l2_ctrl *ctrl);

static const struct v4l2_ctrl_ops TW68_ctrl_ops = {
	.s_ctrl = TW68_s_ctrl,
	.g_volatile_ctrl = TW68_g_volatile_ctrl,
};

static const struct v4l2_ctrl_config TW68_gpio_ctrl = {
	.ops = &TW68_ctrl_ops,
	.name = "gpio",
	.id = S812_CID_GPIO,
	.type = V4L2_CTRL_TYPE_INTEGER,
	.min = 0,
	.max = 0xffff,
	.def = 0xff,
	.step = 1,
	.flags = V4L2_CTRL_FLAG_VOLATILE,
};

static const struct v4l2_queryctrl *ctrl_by_id(unsigned int id)
{
	unsigned int i;

	for (i = 0; i < CTRLS; i++)
		if (video_ctrls[i].id == id)
			return video_ctrls + i;
	return NULL;
}

static struct TW68_format *format_by_fourcc(unsigned int fourcc)
{
	unsigned int i;

	for (i = 0; i < FORMATS; i++)
		if (formats[i].fourcc == fourcc)
			return formats + i;
	return NULL;
}

static void set_tvnorm(struct TW68_dev *dev, struct TW68_tvnorm *norm)
{
	int framesize;

	dprintk("%s: %s\n", __func__, norm->name);
	printk(KERN_INFO "------set tv norm = %s\n", norm->name);
	dev->tvnorm = norm;
	/* setup cropping */
	dev->crop_bounds.left = norm->h_start;
	dev->crop_defrect.left = norm->h_start;
	dev->crop_bounds.width = norm->h_stop - norm->h_start + 1;
	dev->crop_defrect.width = norm->h_stop - norm->h_start + 1;
	dev->crop_bounds.top = (norm->vbi_v_stop_0 + 1) * 2;
	dev->crop_defrect.top = norm->video_v_start * 2;
	dev->crop_bounds.height = ((norm->id & V4L2_STD_525_60) ? 524 : 622) - dev->crop_bounds.top;
	dev->crop_defrect.height = (norm->video_v_stop -
								norm->video_v_start + 1) *
							   2;

	/* calculate byte size for 1 frame */
	framesize = dev->crop_bounds.width *
					dev->crop_bounds.height * 16 >>
				3;

	dprintk("crop set tv norm = %s, width%d   height%d size %d\n",
			norm->name, dev->crop_bounds.width, dev->crop_bounds.height,
			framesize);
}

static int buffer_prepare(struct vb2_buffer *vb)
{
	struct TW68_vc *vc = vb2_get_drv_priv(vb->vb2_queue);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct TW68_buf *buf = container_of(vbuf, struct TW68_buf, vb);
#else
	struct TW68_buf *buf = container_of(vb, struct TW68_buf, vb);
#endif
	unsigned int size;
	if (NULL == vc->fmt)
		return -EINVAL;
	size = (vc->width * vc->height * vc->fmt->depth) >> 3;
	if (vb2_plane_size(vb, 0) < size)
		return -EINVAL;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	vb2_set_plane_payload(&buf->vb, 0, size);
#else
	vb2_set_plane_payload(&buf->vb.vb2_buf, 0, size);
#endif
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 8, 0)
static int queue_setup(struct vb2_queue *vq,
					   unsigned int *nbuffers, unsigned int *nplanes,
					   unsigned int sizes[], struct device *alloc_ctxs[])

#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
static int queue_setup(struct vb2_queue *vq,
					   unsigned int *nbuffers, unsigned int *nplanes,
					   unsigned int sizes[], void *alloc_ctxs[])
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
static int queue_setup(struct vb2_queue *vq, const void *parg,
					   unsigned int *nbuffers, unsigned int *nplanes,
					   unsigned int sizes[], void *alloc_ctxs[])
#else
static int queue_setup(struct vb2_queue *vq, const struct v4l2_format *fmt,
					   unsigned int *nbuffers, unsigned int *nplanes,
					   unsigned int sizes[], void *alloc_ctxs[])
#endif
{
	unsigned int ChannelOffset, nId, pgn;
	u32 m_dwCHConfig, dwReg, dwRegH, dwRegW, nScaler, dwReg2;
	u32 m_StartIdx, m_EndIdx, m_nVideoFormat,
		m_bHorizontalDecimate, m_bVerticalDecimate,
		m_nDropChannelNum, m_bDropMasterOrSlave,
		m_bDropField, m_bDropOddOrEven, m_nCurVideoChannelNum;
	struct TW68_vc *vc;
	struct TW68_dev *dev;
	int single_field = 0;

	if (vq == NULL)
		return -ENODEV;
	vc = vb2_get_drv_priv(vq);
	dev = vc->dev;
	dprintk("vc %p, dev %p\n", vc, dev);
	sizes[0] = vc->fmt->depth * vc->width * vc->height >> 3;
	dprintk("sizes queue setup %d\n", sizes[0]);
	*nplanes = 1;
	if (*nbuffers < 2)
		*nbuffers = 2;
	ChannelOffset = (PAGE_SIZE << 1) / 8 / 8;
	/* NTSC	 FIELD entry number for 720*240*2 */
	/*  ChannelOffset = 128;   */
	nId = vc->nId;
	dwReg2 = tw68_reg_readl(dev, DMA_CH0_CONFIG + 2);
	dwReg = tw68_reg_readl(dev, DMA_CH0_CONFIG + nId);

	printk(KERN_DEBUG
		   " ****buffer_setu#  CH%d::   dwReg2: 0x%X   deReg 0x%X\n",
		   nId, dwReg2, dwReg);

	single_field = (vc->field == V4L2_FIELD_TOP) ||
				   (vc->field == V4L2_FIELD_BOTTOM) || 
				   (vc->field == V4L2_FIELD_ALTERNATE);
	dprintk("queue setup single field : %d\n", single_field);

	if (sfield)
	{
		if (single_field)
		{
			DecoderResize(dev, nId, vc->height, vc->width);
			BFDMA_setup(dev, nId, vc->height,
						sizes[0] / vc->height);
		}
		else
		{
			DecoderResize(dev, nId, vc->height / 2, vc->width);
			BFDMA_setup(dev, nId, (vc->height / 2),
						(sizes[0] / vc->height));
		}
	}
	else
	{
		DecoderResize(dev, nId, vc->height / 2, vc->width);
		BFDMA_setup(dev, nId, (vc->height / 2),
					(sizes[0] / vc->height));
		/* BFbuf setup  DMA mode ... */
	}

	dwReg2 = tw68_reg_readl(dev, DMA_CH0_CONFIG + 2);
	dwReg = tw68_reg_readl(dev, DMA_CH0_CONFIG + nId);

	dprintk(" *# FM CH%d::dwReg2: 0x%X deReg 0x%X  H:%d W:%d\n", nId,
			dwReg2, dwReg, vc->height / 2, (sizes[0] / vc->height));
	/* page number for 1 field */
	pgn = TW68_buffer_pages(sizes[0] / 2) - 1;
	m_nDropChannelNum = 0;
	m_bDropMasterOrSlave = 1; /* master */
	m_bDropField = 0;
	m_bDropOddOrEven = 0;
	m_bHorizontalDecimate = 0;
	m_bVerticalDecimate = 0;
	m_StartIdx = ChannelOffset * nId;
	m_EndIdx = m_StartIdx + pgn; /* pgn;	 85 :: 720 * 480 */
	m_nCurVideoChannelNum = 0;	 /* real-time video channel	starts 0 */
	m_nVideoFormat = vc->nVideoFormat;
	m_dwCHConfig = (m_StartIdx & 0x3FF) |		// 10 bits
				   ((m_EndIdx & 0x3FF) << 10) | // 10 bits
				   ((m_nVideoFormat & 7) << 20) |
				   ((m_bHorizontalDecimate & 1) << 23) |
				   ((m_bVerticalDecimate & 1) << 24) |
				   ((m_nDropChannelNum & 3) << 25) |
				   ((m_bDropMasterOrSlave & 1) << 27) | // 1 bit
				   ((m_bDropField & 1) << 28) |
				   ((m_bDropOddOrEven & 1) << 29) |
				   ((m_nCurVideoChannelNum & 3) << 30);

	tw68_reg_writel(dev, DMA_CH0_CONFIG + nId, m_dwCHConfig);
	dwReg = tw68_reg_readl(dev, DMA_CH0_CONFIG + nId);
	printk(KERN_DEBUG
		   " **# %s CH%d: m_SIdx 0X%x  pgn%d m_dwCHConfig: %X dwReg: %X\n",
		   __func__, nId, m_StartIdx, pgn, m_dwCHConfig, dwReg);
	/* external video decoder settings */
	dwRegW = vc->width;
	if (sfield && single_field)
		dwRegH = vc->height;
	else
		dwRegH = vc->height / 2; /* frame height */

	dwReg = dwRegW | (dwRegH << 16) | (1 << 31);
	dwRegW = dwRegH = dwReg;
	/* Video Size */
	tw68_reg_writel(dev, VIDEO_SIZE_REG, dwReg); /*for Rev.A backward comp.*/
	/*  xxx dwReg = tw68_reg_readl(dev, VIDEO_SIZE_REG); */
	//	printk(KERN_INFO " #### buffer_setup:: VIDEO_SIZE_REG: 0x%X,  0x%X\n",
	//	       VIDEO_SIZE_REG, dwReg);
	tw68_reg_writel(dev, VIDEO_SIZE_REG0 + nId, dwReg); /* for Rev.B or later only */
	/*Scaler*/
	dwRegW &= 0x7FF;
	dwRegW = (720 * 256) / dwRegW;
	dwRegH = (dwRegH >> 16) & 0x1FF;
	// 60HZ video
	dwRegH = (240 * 256) / dwRegH;
	/// 0915  rev B	 black ....
	nScaler = VSCALE1_LO;  /// + (nId<<4); //VSCALE1_LO + 0|0x10|0x20|0x30
	dwReg = dwRegH & 0xFF; // V
	/// if(nId >= 4) DeviceWrite2864(nAddr,tmp);
	///  tw68_reg_writel(dev, nScaler,	 dwReg);
	nScaler++; // VH
	dwReg = (((dwRegH >> 8) & 0xF) << 4) | ((dwRegW >> 8) & 0xF);
	nScaler++; // H
	dwReg = dwRegW & 0xFF;
	// setup for Black stripe remover
	dwRegW = vc->width; // -12
	dwRegH = 4;			// start position
	dwReg = (dwRegW - dwRegH) * (1 << 16) / vc->width;
	dwReg = (dwRegH & 0x1F) |
			((dwRegH & 0x3FF) << 5) |
			(dwReg << 15);
	tw68_reg_writel(dev, DROP_FIELD_REG0 + nId, 0xBFFFFFFF); // 28 // B 30 FPS
	// 0xBFFFCFFF;	// 28  // 26 FPS   last xx FC
	// 0xBF3F3F3F;  // 24 FPS
	dwReg2 = tw68_reg_readl(dev, DMA_CH0_CONFIG + 2);
	dwReg = tw68_reg_readl(dev, DMA_CH0_CONFIG + nId);
	printk(KERN_INFO " ********#### buffer_setup CH%d::   dwReg2: 0x%X   deReg 0x%X\n", nId, dwReg2, dwReg);
	return 0;
}

static void buffer_queue(struct vb2_buffer *vb)
{
	struct TW68_vc *vc = vb2_get_drv_priv(vb->vb2_queue);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)
	struct vb2_v4l2_buffer *vbuf = to_vb2_v4l2_buffer(vb);
	struct TW68_buf *buf = container_of(vbuf, struct TW68_buf, vb);
#else
	struct TW68_buf *buf = container_of(vb, struct TW68_buf, vb);
#endif
	unsigned long flags = 0;

	spin_lock_irqsave(&vc->qlock, flags);
	list_add_tail(&buf->list, &vc->buf_list);
	spin_unlock_irqrestore(&vc->qlock, flags);
	return;
}

static int start_streaming(struct vb2_queue *vq, unsigned int count);

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 15, 0)
static int stop_streaming(struct vb2_queue *vq);
#else
static void stop_streaming(struct vb2_queue *vq);
#endif

static struct vb2_ops s812_vb2_ops = {
	.queue_setup = queue_setup,
	.buf_prepare = buffer_prepare,
	.buf_queue = buffer_queue,
	.start_streaming = start_streaming,
	.stop_streaming = stop_streaming,
	.wait_prepare = vb2_ops_wait_prepare,
	.wait_finish = vb2_ops_wait_finish,
};

int TW68_g_volatile_ctrl(struct v4l2_ctrl *c)
{
	struct TW68_vc *vc = container_of(c->handler, struct TW68_vc, hdl);
	struct TW68_dev *dev = vc->dev;
	int DMA_nCH = vc->nId;
	int nId = DMA_nCH & 0xF;
	int regval = 0;

	dprintk("%s %x\n", __func__, c->id);
	switch (c->id)
	{
	case V4L2_CID_BRIGHTNESS:
		if (DMA_nCH < 4)
		{
			regval = tw68_reg_readl(dev, CH1_BRIGHTNESS_REG + DMA_nCH * 0x10);
			regval = (regval + 0x80) & 0xFF;
		}
		else
		{
			regval = tw68_reg_readl(dev, CH1_BRIGHTNESS_REG + 0x100 + (DMA_nCH - 4) * 0x10);
			regval = (regval + 0x80) & 0xFF;
		}
		c->val = vc->video_param.ctl_bright = regval;
		break;
	case V4L2_CID_HUE:
		if (DMA_nCH < 4)
			regval = tw68_reg_readl(dev, CH1_HUE_REG + DMA_nCH * 0x10);
		else
			regval = tw68_reg_readl(dev, CH1_HUE_REG + 0x100 + (DMA_nCH - 4) * 0x10);
		if (regval < 0x80)
			c->val = vc->video_param.ctl_hue = regval;
		else
			c->val = vc->video_param.ctl_hue = (regval - 0x100);
		break;
	case V4L2_CID_CONTRAST:
		if (DMA_nCH < 4)
			regval = tw68_reg_readl(dev, CH1_CONTRAST_REG + DMA_nCH * 0x10);
		else
			regval = tw68_reg_readl(dev, CH1_CONTRAST_REG + 0x100 + (DMA_nCH - 4) * 0x10);
		c->val = vc->video_param.ctl_contrast = regval;
		break;
	case V4L2_CID_SATURATION:
		if (DMA_nCH < 4)
			regval = tw68_reg_readl(dev, CH1_SAT_U_REG + DMA_nCH * 0x10);
		else
			regval = tw68_reg_readl(dev, CH1_SAT_U_REG + 0x100 + (DMA_nCH - 4) * 0x10);
		c->val = vc->video_param.ctl_saturation = regval / 2;
		break;
	case V4L2_CID_AUDIO_MUTE:
		c->val = vc->video_param.ctl_mute;
		break;
	case V4L2_CID_AUDIO_VOLUME:
		c->val = vc->video_param.ctl_volume;
		break;
	case S812_CID_GPIO:
		regval = tw68_reg_readl(dev, GPIO_REG_BANK_2);
		c->val = regval & 0xffff;
		printk(KERN_INFO "read gpio %x\n", c->val);
		break;
	default:
		return -EINVAL;
	}
	printk(KERN_INFO
		   "  nId%d %s Get_control name=%s val=%d  regval 0x%X\n",
		   nId, __func__, c->name, c->val, regval);
	return 0;
}

static int TW68_s_ctrl(struct v4l2_ctrl *c)
{
	int DMA_nCH, nId, err;
	int regval = 0;
	struct TW68_vc *vc = container_of(c->handler, struct TW68_vc, hdl);
	struct TW68_dev *dev = vc->dev;

	DMA_nCH = vc->nId;
	nId = DMA_nCH & 0xF;
	switch (c->id)
	{
	case V4L2_CID_BRIGHTNESS:
		vc->video_param.ctl_bright = c->val;
		regval = ((c->val - 0x80)) & 0xFF;
		if (DMA_nCH < 4)
			tw68_reg_writel(dev, CH1_BRIGHTNESS_REG + DMA_nCH * 0x10, regval);
		else
		{
			if (DMA_nCH < 8)
				tw68_reg_writel(dev, CH1_BRIGHTNESS_REG + 0x100 + (DMA_nCH - 4) * 0x10, regval);
		}
		break;
	case V4L2_CID_CONTRAST:
		vc->video_param.ctl_contrast = c->val;
		if (DMA_nCH < 4)
			tw68_reg_writel(dev, CH1_CONTRAST_REG + DMA_nCH * 0x10, c->val);
		else
		{
			if (DMA_nCH < 8)
				tw68_reg_writel(dev, CH1_CONTRAST_REG + 0x100 + (DMA_nCH - 4) * 0x10, c->val);
		}
		break;
	case V4L2_CID_HUE:
		vc->video_param.ctl_hue = c->val;
		regval = c->val; //  &0xFF;
		if (DMA_nCH < 4)
			tw68_reg_writel(dev, CH1_HUE_REG + DMA_nCH * 0x10, regval);
		else
		{
			if (DMA_nCH < 8)
				tw68_reg_writel(dev, CH1_HUE_REG + 0x100 + (DMA_nCH - 4) * 0x10, regval);
		}
		break;
	case V4L2_CID_SATURATION:
		vc->video_param.ctl_saturation = c->val;
		regval = c->val * 2;
		if (DMA_nCH < 4)
		{
			tw68_reg_writel(dev, CH1_SAT_U_REG + DMA_nCH * 0x10, regval);
			tw68_reg_writel(dev, CH1_SAT_V_REG + DMA_nCH * 0x10, regval);
		}
		else
		{
			if (DMA_nCH < 8)
			{
				tw68_reg_writel(dev, CH1_SAT_U_REG + 0x100 + (DMA_nCH - 4) * 0x10, regval);
				tw68_reg_writel(dev, CH1_SAT_V_REG + 0x100 + (DMA_nCH - 4) * 0x10, regval);
			}
		}
		break;
	case V4L2_CID_AUDIO_MUTE:
		vc->video_param.ctl_mute = c->val;
		break;
	case V4L2_CID_AUDIO_VOLUME:
		vc->video_param.ctl_volume = c->val;
		break;
	case S812_CID_GPIO:
		regval = (c->val);
		tw68_reg_writel(dev, GPIO_REG_BANK_2, regval);
		break;
	default:
		goto error;
	}
	err = 0;
error:
	return err;
}

static int video_open(struct file *file)
{
	struct TW68_vc *vc = video_drvdata(file);
	struct TW68_dev *dev = vc->dev;
	unsigned int request = 0;
	unsigned int dmaCH;
	int rc;
	int k;
	rc = v4l2_fh_open(file);
	if (rc != 0)
		return rc;
	k = vc->nId;
	request = 1 << k;
	/* check video decoder video standard and change default tvnormf */
	dmaCH = 0xF;
	dmaCH = k;
	printk(KERN_INFO "video_open ID:%d  dmaCH %x   request %X\n",
		   k, dmaCH, request);

	if (!vb2_is_busy(&vc->vb_vidq))
	{
		if (dev->vc[k].viddetected == 0)
		{
#if defined(NTSC_STANDARD_NODETECT)
			printk("%s: NTSC standard mode\n", __func__);
			vc->tvnormf = &tvnorms[4];
			vc->PAL50 = 0;
			vc->dW = NTSC_default_width;
			vc->dH = NTSC_default_height;
#elif defined(PAL_STANDARD_NODETECT)
			printk("%s: PAL standard\n", __func__);
			vc->tvnormf = &tvnorms[0];
			vc->PAL50 = 1;
			vc->dW = PAL_default_width;
			vc->dH = PAL_default_height;
#else
			switch (vidstd_open)
			{
			case 1:
				printk("NTSC standard\n");
				vc->tvnormf = &tvnorms[4];
				vc->PAL50 = 0;
				vc->dW = NTSC_default_width;
				vc->dH = NTSC_default_height;
				break;
			case 2:
				printk("PAL standard\n");
				vc->tvnormf = &tvnorms[0];
				vc->PAL50 = 1;
				vc->dW = PAL_default_width;
				vc->dH = PAL_default_height;
				break;
			default:
				printk("vidstd auto-detect\n");
				if (VideoDecoderDetect(dev, dmaCH) == 50)
				{
					vc->tvnormf = &tvnorms[0];
					vc->PAL50 = 1;
					vc->dW = PAL_default_width;
					vc->dH = PAL_default_height;
				}
				else
				{
					vc->tvnormf = &tvnorms[4];
					vc->PAL50 = 0;
					vc->dW = NTSC_default_width;
					vc->dH = NTSC_default_height;
				}
				break;
			}
#endif
			vc->height = vc->dH;
			vc->width = vc->dW;
			vc->viddetected = 1;
			if (vc->PAL50)
			{
				tw68_reg_writel(dev, MISC_CONTROL3, 0xc5);
				tw68_reg_writel(dev, MISC_CONTROL3 + 0x100, 0xc5);
			}
			else
			{
				tw68_reg_writel(dev, MISC_CONTROL3, 0x85);
				tw68_reg_writel(dev, MISC_CONTROL3 + 0x100, 0x85);
			}
			if (k <= 3)
			{
				tw68_reg_writel(dev, DECODER0_SDT + (k * 0x10), vc->PAL50 ? 1 : 0);
			}
			else
			{
				tw68_reg_writel(dev, DECODER0_SDT + 0x100 + ((k - 4) * 0x10), vc->PAL50 ? 1 : 0);
			}
		}
	} // vb2 busy
	dev->video_opened = dev->video_opened | request;
	printk(KERN_DEBUG "%s: 0X%p open video%d  minor%d,type=%s, vid_opn=0x%X k:%d std:%s  %d\n",
		   __func__, dev, vc->vdev.num, vc->nId,
		   v4l2_type_names[V4L2_BUF_TYPE_VIDEO_CAPTURE], dev->video_opened, k, vc->tvnormf->name,
		   vc->PAL50);
	return 0;
}

static int TW68_g_fmt_vid_cap(struct file *file, void *priv,
							  struct v4l2_format *f)
{
	struct TW68_vc *vc = video_drvdata(file);
	f->fmt.pix.width = vc->width;
	f->fmt.pix.height = vc->height;
	f->fmt.pix.field = vc->field;

	f->fmt.pix.pixelformat = vc->fmt->fourcc;
	f->fmt.pix.bytesperline =
		(f->fmt.pix.width * vc->fmt->depth) >> 3;
	f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;
	f->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
	f->fmt.pix.priv = 0;
	printk(KERN_INFO " _g_fmt_vid_cap: width %d  height %d\n", f->fmt.pix.width, f->fmt.pix.height);
	return 0;
}

static int TW68_try_fmt_vid_cap(struct file *file, void *priv,
								struct v4l2_format *f)
{
	struct TW68_vc *vc = video_drvdata(file);
	struct TW68_format *fmt;
	enum v4l2_field field;
	unsigned int maxw, maxh;
	u32 k;
	u32 nId = vc->nId;
	struct TW68_dev *dev = vc->dev;
	fmt = format_by_fourcc(f->fmt.pix.pixelformat);
	dprintk("TW68 input  nId:%x   try_fmt:: %x  | width %d	height %d\n",
			nId, f->fmt.pix.pixelformat, f->fmt.pix.width, f->fmt.pix.height);

	if (NULL == fmt)
	{
		printk(KERN_WARNING "TW68 fmt:: no valid pixel format\n");
		return -EINVAL;
	}
	switch (fmt->fourcc)
	{
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
	case V4L2_PIX_FMT_RGB565:
		break;
	default:
		return -EINVAL;
	}
	k = nId;

	if (vc->tvnormf->id & V4L2_STD_525_60)
	{
		maxw = 720;
		maxh = 480;
	}
	else
	{
		maxw = 720;
		maxh = 576;
	}
	dprintk("tvnormf %d->name %s  id %X maxh %d\n", nId,
			vc->tvnormf->name,
			(unsigned int)vc->tvnormf->id, maxh);

	field = f->fmt.pix.field;

	if (V4L2_FIELD_ANY == field)
	{
		field = (f->fmt.pix.height > maxh / 2)
					? V4L2_FIELD_INTERLACED
					: V4L2_FIELD_BOTTOM;
		dprintk("field now %d, h %d, max %d\n", field,
				f->fmt.pix.height, maxh / 2);
	}
	switch (field)
	{
	case V4L2_FIELD_TOP:
	case V4L2_FIELD_BOTTOM:
	case V4L2_FIELD_ALTERNATE:
		maxh = maxh / 2;
		if (f->fmt.pix.height > maxh)
		{
			field = V4L2_FIELD_INTERLACED;
			dprintk("forcing interlace %d\n", field);
		}
		break;
	case V4L2_FIELD_INTERLACED:
		break;
	default:
		dprintk("invalid field\n");
		return -EINVAL;
	}
	f->fmt.pix.field = field;
	dprintk("TW68 _try_fmt_vid_cap fmt::pixelformat %x  field: %d _fmt: width %d  height %d\n", fmt->fourcc, field, f->fmt.pix.width, f->fmt.pix.height);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 32)
	v4l_bound_align_image(&f->fmt.pix.width, 128, maxw, 2, // 4 pixel  test 360	4,
						  &f->fmt.pix.height, 60, maxh, 0, 0);
#endif
	f->fmt.pix.bytesperline =
		(f->fmt.pix.width * fmt->depth) >> 3;
	f->fmt.pix.sizeimage =
		f->fmt.pix.height * f->fmt.pix.bytesperline;
	f->fmt.pix.colorspace = V4L2_COLORSPACE_SMPTE170M;
	f->fmt.pix.priv = 0;
	dprintk("TW68 _try_fmt_vid_cap: width %d  height %d %d  %d\n", f->fmt.pix.width, f->fmt.pix.height, maxw, maxh);
	return 0;
}

static int TW68_s_fmt_vid_cap(struct file *file, void *priv,
							  struct v4l2_format *f)
{
	struct TW68_vc *vc = video_drvdata(file);
	int rc;
	struct vb2_queue *q = &vc->vb_vidq;
	struct TW68_format *fmt = format_by_fourcc(f->fmt.pix.pixelformat);
	struct TW68_dev *dev = vc->dev;
	dprintk("%s: %x  W:%d H:%d, field:%X\n", __func__, f->fmt.pix.pixelformat, vc->width, vc->height, vc->field);
	if (vb2_is_busy(q))
		return -EBUSY;

	rc = TW68_try_fmt_vid_cap(file, priv, f);
	if (rc < 0)
	{
		printk(KERN_DEBUG "try fmt fail %x\n", rc);
		return rc;
	}

	switch (fmt->fourcc)
	{
	case V4L2_PIX_FMT_YUYV:
		vc->nVideoFormat = VIDEO_FORMAT_YUYV;
		break;
	case V4L2_PIX_FMT_UYVY:
		vc->nVideoFormat = VIDEO_FORMAT_UYVY;
		break;
	case V4L2_PIX_FMT_RGB565:
		vc->nVideoFormat = VIDEO_FORMAT_RGB565;
		break;
	default:
		printk(KERN_INFO "s_fmt: invalid\n");
		return -EINVAL;
	}

	vc->fmt = format_by_fourcc(f->fmt.pix.pixelformat);
	vc->width = f->fmt.pix.width;
	vc->height = f->fmt.pix.height;
	vc->field = f->fmt.pix.field;
	printk(KERN_ERR "%s: vc->fmt   W:%d H:%d  field:%X\n", __func__, vc->width, vc->height, vc->field);
	return 0;
}

static int TW68_enum_input(struct file *file, void *priv,
						   struct v4l2_input *i)
{
	unsigned int vidstat;
	struct TW68_vc *vc = video_drvdata(file);
	struct TW68_dev *dev = vc->dev;
	int nId = vc->nId;

	if (i->index != 0)
		return -EINVAL;
	i->type = V4L2_INPUT_TYPE_CAMERA;
	i->status = 0;
	i->std = TW68_NORMS;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 33)
	i->capabilities = V4L2_IN_CAP_STD;
#endif
	vidstat = tw68_reg_readl(dev, VIDSTAT[nId]);
	if (vidstat & TW686X_VIDSTAT_VDLOSS)
		i->status |= V4L2_IN_ST_NO_SIGNAL;
	if (!(vidstat & TW686X_VIDSTAT_HLOCK))
		i->status |= V4L2_IN_ST_NO_H_LOCK;

	strlcpy(i->name, "Composite", sizeof(i->name));
	return 0;
}

static int TW68_g_input(struct file *file, void *priv, unsigned int *i)
{
	*i = 0;
	return 0;
}

static int TW68_s_input(struct file *file, void *priv, unsigned int i)
{
	if (i > 0)
		return -EINVAL;
	return 0;
}

static int TW68_querycap(struct file *file, void *priv,
						 struct v4l2_capability *cap)
{
	struct TW68_vc *vc = video_drvdata(file);
	struct TW68_dev *dev = vc->dev;

	strcpy(cap->driver, "TW6869");
	strlcpy(cap->card, TW68_boards[dev->board].name,
			sizeof(cap->card));
	sprintf(cap->bus_info, "PCI:%s", pci_name(dev->pci));
	cap->device_caps = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING |
					   V4L2_CAP_READWRITE;
	cap->capabilities = cap->device_caps | V4L2_CAP_DEVICE_CAPS;
	return 0;
}

int TW68_s_std_internal(struct TW68_vc *vc, v4l2_std_id *id)
{
	unsigned int i, nId;
	struct TW68_dev *dev = vc->dev;

	nId = vc->nId;
	for (i = 0; i < TVNORMS; i++)
		if (*id == tvnorms[i].id)
			break;
	if (i == TVNORMS)
		for (i = 0; i < TVNORMS; i++)
			if (*id & tvnorms[i].id)
				break;
	if (i == TVNORMS)
		return -EINVAL;
	*id = tvnorms[i].id;
	if (*id != V4L2_STD_NTSC)
	{
		vc->PAL50 = 1;
		tw68_reg_writel(dev, MISC_CONTROL3, 0xc5);
		tw68_reg_writel(dev, MISC_CONTROL3 + 0x100, 0xc5);
		if (nId <= 3)
			tw68_reg_writel(dev, DECODER0_SDT + (nId * 0x10), 1);
		else
			tw68_reg_writel(dev, DECODER0_SDT + 0x100 + ((nId - 4) * 0x10), 1);
	}
	else
	{
		vc->PAL50 = 0;
		tw68_reg_writel(dev, MISC_CONTROL3, 0x85);
		tw68_reg_writel(dev, MISC_CONTROL3 + 0x100, 0x85);
		if (nId <= 3)
			tw68_reg_writel(dev, DECODER0_SDT + (nId * 0x10), 0);
		else
			tw68_reg_writel(dev, DECODER0_SDT + 0x100 + ((nId - 4) * 0x10), 0);
	}

	set_tvnorm(dev, &tvnorms[i]);
	vc->tvnormf = &tvnorms[i];
	printk(KERN_INFO "%s: *id = %x  i= %x\n", __func__, (int)*id, i);
	return 0;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
static int TW68_s_std(struct file *file, void *priv, v4l2_std_id id)
#else
static int TW68_s_std(struct file *file, void *priv, v4l2_std_id *id)
#endif
{
	struct TW68_vc *vc = video_drvdata(file);
	struct vb2_queue *q = &vc->vb_vidq;
	int rc;
	struct TW68_dev *dev = vc->dev;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
	dprintk("%s, _s_std v4l2_std_id =%d\n", __func__, (int)id);
#else
	dprintk("%s, _s_std v4l2_std_id =%d\n", __func__, (int)*id);
#endif
	if (vb2_is_busy(q))
		return -EBUSY;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
	rc = TW68_s_std_internal(vc, &id);
#else
	rc = TW68_s_std_internal(vc, id);
#endif
	return rc;
}

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 29) || V4L_DVB_TREE
#define TW686X_VIDEO_WIDTH 720
#define TW686X_VIDEO_HEIGHT(id) ((id & V4L2_STD_525_60) ? 480 : 576)
#define TW686X_MAX_FPS(id) ((id & V4L2_STD_525_60) ? 30 : 25)

static int TW68_enum_framesizes(struct file *file, void *priv,
								struct v4l2_frmsizeenum *fsize)
{
	struct TW68_vc *vc = video_drvdata(file);

	if (fsize->index)
		return -EINVAL;
	fsize->type = V4L2_FRMSIZE_TYPE_STEPWISE;
	fsize->stepwise.max_width = TW686X_VIDEO_WIDTH;
	fsize->stepwise.min_width = fsize->stepwise.max_width / 2;
	fsize->stepwise.step_width = fsize->stepwise.min_width;
	fsize->stepwise.max_height = TW686X_VIDEO_HEIGHT(vc->tvnormf->id);
	fsize->stepwise.min_height = fsize->stepwise.max_height / 2;
	fsize->stepwise.step_height = fsize->stepwise.min_height;
	return 0;
}

static int TW68_enum_frameintervals(struct file *file, void *priv,
									struct v4l2_frmivalenum *ival)
{
	struct TW68_vc *vc = video_drvdata(file);
	int max_fps = TW686X_MAX_FPS(vc->tvnormf->id);
	int max_rates = DIV_ROUND_UP(max_fps, 2);

	if (ival->index >= max_rates)
		return -EINVAL;

	ival->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	ival->discrete.numerator = 1;
	if (ival->index < (max_rates - 1))
		ival->discrete.denominator = (ival->index + 1) * 2;
	else
		ival->discrete.denominator = max_fps;
	return 0;
}
#endif

static int TW68_g_std(struct file *file, void *priv, v4l2_std_id *id)
{
	struct TW68_vc *vc = video_drvdata(file);
	struct TW68_dev *dev = vc->dev;

	*id = dev->tvnorm->id;
	*id = vc->tvnormf->id;
	dprintk("%s, _g_std v4l2_std_id =%d\n", __func__, (int)*id);
	return 0;
}

static int TW68_enum_fmt_vid_cap(struct file *file, void *priv,
								 struct v4l2_fmtdesc *f)
{
	if (f->index >= FORMATS)
		return -EINVAL;
	strlcpy(f->description, formats[f->index].name,
			sizeof(f->description));
	f->pixelformat = formats[f->index].fourcc;
	printk(KERN_DEBUG "========TW68__enum_fmt_vid_cap	 description %s, type %d\n", f->description, f->type);
	return 0;
}

static int start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct TW68_vc *vc = vb2_get_drv_priv(vq);
	struct TW68_dev *dev = vc->dev;

	mutex_lock(&dev->start_lock);
	msleep(100);
	mutex_unlock(&dev->start_lock);
	vc->Done = 0;
	vc->framecount = 0;
	dprintk("%s: start_streaming(TW68_queue(fh)) DMA %d  q->streaming:%X  streaming:%x.\n",
			dev->name, vc->nId, 1, 1);
	TW68_set_dmabits(dev, vc->nId);
	return 0;
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 15, 0)
static int stop_streaming(struct vb2_queue *vq)
#else
static void stop_streaming(struct vb2_queue *vq)
#endif
{
	struct TW68_vc *vc = vb2_get_drv_priv(vq);
	struct TW68_dev *dev = vc->dev;
	struct TW68_buf *buf, *node;
	int DMA_nCH = vc->nId;
	unsigned long flags = 0;

	dprintk("stop streaming\n");
	mutex_lock(&dev->start_lock);
	spin_lock_irqsave(&dev->slock, flags);
	vc->framecount = 0;
	stop_video_DMA(dev, DMA_nCH);
	spin_unlock_irqrestore(&dev->slock, flags);
	msleep(50);
	dprintk("%s:  DMA_nCH:%x   videobuf_streamoff delete video timeout\n", dev->name, vc->nId);
	spin_lock_irqsave(&vc->qlock, flags);
	list_for_each_entry_safe(buf, node, &vc->buf_list, list)
	{
		list_del(&buf->list);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_ERROR);
#else
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_ERROR);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
		dprintk("[%p/%d] done\n", buf, buf->vb.v4l2_buf.index);
#else
		dprintk("[%p/%d] done\n", buf, buf->vb.vb2_buf.index);
#endif
	}
	spin_unlock_irqrestore(&vc->qlock, flags);
	msleep(50);
	mutex_unlock(&dev->start_lock);
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3, 15, 0)
	return 0;
#else
	return;
#endif
}

static int video_release(struct file *file)
{
	int rc;

	rc = vb2_fop_release(file);
	msleep(33);
	return rc;
}

static const struct v4l2_file_operations video_fops = {
	.owner = THIS_MODULE,
	.open = video_open,
	.release = video_release, // vb2_fop_release,
	.poll = vb2_fop_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap = vb2_fop_mmap,
	.read = vb2_fop_read,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
static int TW68_g_selection(struct file *file, void *priv,
							struct v4l2_selection *sel)
{
	struct TW68_vc *vc = video_drvdata(file);

	if (sel->type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	switch (sel->target)
	{
	case V4L2_SEL_TGT_CROP_BOUNDS:
		sel->r = vc->dev->crop_bounds;
		break;
	case V4L2_SEL_TGT_CROP_DEFAULT:
		sel->r = vc->dev->crop_defrect;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int TW68_g_pixelaspect(struct file *file, void *priv,
							  int type, struct v4l2_fract *f)
{
	struct TW68_vc *vc = video_drvdata(file);

	if (type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
		return -EINVAL;

	f->numerator = 1;
	f->denominator = 1;
	if (vc->tvnormf->id & V4L2_STD_525_60)
	{
		f->numerator = 11;
		f->denominator = 10;
	}
	if (vc->tvnormf->id & V4L2_STD_625_50)
	{
		f->numerator = 54;
		f->denominator = 59;
	}
	return 0;
}
#endif

static const struct v4l2_ioctl_ops video_ioctl_ops = {
	.vidioc_querycap = TW68_querycap,
	.vidioc_enum_fmt_vid_cap = TW68_enum_fmt_vid_cap,
	.vidioc_g_fmt_vid_cap = TW68_g_fmt_vid_cap,
	.vidioc_try_fmt_vid_cap = TW68_try_fmt_vid_cap,
	.vidioc_s_fmt_vid_cap = TW68_s_fmt_vid_cap,
	.vidioc_reqbufs = vb2_ioctl_reqbufs,
	.vidioc_querybuf = vb2_ioctl_querybuf,
	.vidioc_qbuf = vb2_ioctl_qbuf,
	.vidioc_dqbuf = vb2_ioctl_dqbuf,
	.vidioc_s_std = TW68_s_std,
	.vidioc_g_std = TW68_g_std,
#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 29) || V4L_DVB_TREE
	.vidioc_enum_framesizes = TW68_enum_framesizes,
	.vidioc_enum_frameintervals = TW68_enum_frameintervals,
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0)
	.vidioc_g_pixelaspect = TW68_g_pixelaspect,
	.vidioc_g_selection = TW68_g_selection,
#endif
	.vidioc_enum_input = TW68_enum_input,
	.vidioc_g_input = TW68_g_input,
	.vidioc_s_input = TW68_s_input,
	.vidioc_streamon = vb2_ioctl_streamon,
	.vidioc_streamoff = vb2_ioctl_streamoff,
	.vidioc_log_status = v4l2_ctrl_log_status,
	.vidioc_subscribe_event = v4l2_ctrl_subscribe_event,
	.vidioc_unsubscribe_event = v4l2_event_unsubscribe,
};

struct video_device tw68_video_template = {
	.name = "TW686v-video",
	.fops = &video_fops,
	.ioctl_ops = &video_ioctl_ops,
	.minor = -1,
	.tvnorms = TW68_NORMS,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 11, 0)
	.current_norm = V4L2_STD_NTSC,
#endif
};

int TW68_video_init1(struct TW68_dev *dev)
{
	int k, m, n;
	__le32 *cpu;
	dma_addr_t dma_addr;
	struct TW68_vc *vc;

	/* sanitycheck insmod options */
	if (gbuffers < 2 || gbuffers > VIDEO_MAX_FRAME)
		gbuffers = 2;
	if (gbufsize < 0 || gbufsize > gbufsize_max)
		gbufsize = gbufsize_max;
	gbufsize = (gbufsize + PAGE_SIZE - 1) & PAGE_MASK;
	// pci_alloc_consistent	  32 4 * 8  continuous field memory buffer
	for (n = 0; n < 8; n++)
		for (m = 0; m < 4; m++)
		{
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
			cpu = pci_alloc_consistent(dev->pci, 800 * 300 * 2, &dma_addr); /* 8* 4096 contiguous  */
#else
			cpu = dma_alloc_coherent(&dev->pci->dev, 800 * 300 * 2, &dma_addr, GFP_KERNEL); /* 8* 4096 contiguous  */
#endif
			dev->BDbuf[n][m].cpu = cpu;
			dev->BDbuf[n][m].dma_addr = dma_addr;
			/* assume aways successful   480k each field   total 32	 <16MB */
		}
	/* put some sensible defaults into the data structures ... */
	dev->ctl_bright = ctrl_by_id(V4L2_CID_BRIGHTNESS)->default_value;
	dev->ctl_contrast = ctrl_by_id(V4L2_CID_CONTRAST)->default_value;
	dev->ctl_hue = ctrl_by_id(V4L2_CID_HUE)->default_value;
	dev->ctl_saturation = ctrl_by_id(V4L2_CID_SATURATION)->default_value;
	dev->ctl_volume = ctrl_by_id(V4L2_CID_AUDIO_VOLUME)->default_value;
	dev->ctl_mute = 1; // ctrl_by_id(V4L2_CID_AUDIO_MUTE)->default_value;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
	init_timer(&dev->delay_resync); // 1021
	dev->delay_resync.function = resync;
	dev->delay_resync.data = (unsigned long)dev;
#else
	timer_setup(&dev->delay_resync, (void *)resync, 0);
#endif
	mod_timer(&dev->delay_resync, jiffies + msecs_to_jiffies(300));

	for (k = 0; k < 8; k++)
	{
		vc = &dev->vc[k];
		INIT_LIST_HEAD(&vc->buf_list);
		if (k < 4)
		{
			vc->video_param.ctl_bright = tw68_reg_readl(dev, CH1_BRIGHTNESS_REG + k * 0x10);
			vc->video_param.ctl_contrast = tw68_reg_readl(dev, CH1_CONTRAST_REG + k * 0x10);
			vc->video_param.ctl_hue = tw68_reg_readl(dev, CH1_HUE_REG + k * 0x10);
			vc->video_param.ctl_saturation = tw68_reg_readl(dev, CH1_SAT_U_REG + k * 0x10) / 2;
			vc->video_param.ctl_mute = tw68_reg_readl(dev, CH1_SAT_V_REG + k * 0x10) / 2;
		}
		else
		{
			vc->video_param.ctl_bright = tw68_reg_readl(dev, CH1_BRIGHTNESS_REG + (k - 4) * 0x10 + 0x100);
			vc->video_param.ctl_contrast = tw68_reg_readl(dev, CH1_CONTRAST_REG + (k - 4) * 0x10 + 0x100);
			vc->video_param.ctl_hue = tw68_reg_readl(dev, CH1_HUE_REG + (k - 4) * 0x10 + 0x100);
			vc->video_param.ctl_saturation = tw68_reg_readl(dev, CH1_SAT_U_REG + (k - 4) * 0x10 + 0x100) / 2;
			vc->video_param.ctl_mute = tw68_reg_readl(dev, CH1_SAT_V_REG + (k - 4) * 0x10 + 0x100) / 2;
		}
		printk(KERN_INFO
			   "TW68_  _video_init1[%d] def AMP: BR %d	CONT %d	 HUE_ %d  SAT_U_%d SAT_V_%d\n",
			   k,
			   vc->video_param.ctl_bright,
			   vc->video_param.ctl_contrast, vc->video_param.ctl_hue, vc->video_param.ctl_saturation, vc->video_param.ctl_mute);
	} // for k
	for (k = 7; k > 0; k--)
	{
		vc = &dev->vc[k];
		vc->video_param.ctl_bright = dev->vc[k - 1].video_param.ctl_bright;
		vc->video_param.ctl_contrast = dev->vc[k - 1].video_param.ctl_contrast;
		vc->video_param.ctl_hue = dev->vc[k - 1].video_param.ctl_hue;
		vc->video_param.ctl_saturation = dev->vc[k - 1].video_param.ctl_saturation;
		vc->video_param.ctl_mute = dev->vc[k - 1].video_param.ctl_mute;
		printk(KERN_INFO
			   "%s: get decoder %d default AMP: BRIGHTNESS %d  CONTRAST %d  HUE_ %d  SAT_U_%d SAT_V_%d\n",
			   __func__,
			   k,
			   vc->video_param.ctl_bright,
			   vc->video_param.ctl_contrast,
			   vc->video_param.ctl_hue,
			   vc->video_param.ctl_saturation,
			   vc->video_param.ctl_mute);
	}
	// Normalize the reg value to standard value range
	for (k = 0; k < 8; k++)
	{
		vc = &dev->vc[k];
		vc->video_param.ctl_bright = (vc->video_param.ctl_bright + 0x80) & 0xFF;
		vc->video_param.ctl_contrast = vc->video_param.ctl_contrast & 0x7F;
		vc->video_param.ctl_hue = vc->video_param.ctl_hue & 0xFF;
		vc->video_param.ctl_saturation = vc->video_param.ctl_saturation & 0xFF;
		vc->video_param.ctl_mute = vc->video_param.ctl_mute & 0xFF;
		dprintk("TW68_  _video_init1   remap  %d def AMP: BR %d	CONT %d	 HUE_ %d  SAT_U_%d SAT_V_%d\n", k,
				vc->video_param.ctl_bright, vc->video_param.ctl_contrast,
				vc->video_param.ctl_hue, vc->video_param.ctl_saturation,
				vc->video_param.ctl_mute);
	}
	return 0;
}

int TW68_video_init2(struct TW68_dev *dev)
{
	int k;

	set_tvnorm(dev, &tvnorms[0]);
	for (k = 0; k < 8; k++)
		dev->vc[k].tvnormf = &tvnorms[0];
	return 0;
}

int BF_Copy(struct TW68_dev *dev, int nDMA_channel, u32 Fn, u32 PB, u32 single, struct TW68_buf *buf)
{
	int n, Hmax, Wmax, h, pos, pitch;
	struct TW68_vc *vc;
	int nId = nDMA_channel;
	void *vbuf, *srcbuf;

	vc = &dev->vc[nId];
	// fill P field half frame SG mapping entries
	pos = 0;
	n = 0;
	if (Fn)
		n = 2;
	if (PB)
		n++;
	srcbuf = dev->BDbuf[nDMA_channel][n].cpu;
	if (buf == NULL)
		return 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	vbuf = vb2_plane_vaddr(&buf->vb, 0);
#else
	vbuf = vb2_plane_vaddr(&buf->vb.vb2_buf, 0);
#endif
	if (vbuf == NULL)
		return 0;
	if (srcbuf == NULL)
		return 0;
	Hmax = vc->height / 2;
	Wmax = vc->width;
	pitch = Wmax * vc->fmt->depth / 8;
	if (Fn)
		pos = pitch;

	if (single)
	{
		pos = 0;
		Hmax = vc->height;
		if (Hmax > 288)
		{
			printk(KERN_INFO "hmax out of range!\n");
			Hmax = 288;
		}
	}
	if (single)
	{
		for (h = 0; h < Hmax; h++)
		{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
			if (pos + pitch > vb2_plane_size(&buf->vb, 0))
				break;
#else
			if (pos + pitch > vb2_plane_size(&buf->vb.vb2_buf, 0))
				break;
#endif
			memcpy(vbuf + pos, srcbuf, pitch);
			pos += pitch;
			srcbuf += pitch;
		}
	}
	else
	{
		for (h = Hmax; h < Hmax + Hmax; h++)
		{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
			if ((pos + pitch) > vb2_plane_size(&buf->vb, 0))
				break;
#else
			if ((pos + pitch) > vb2_plane_size(&buf->vb.vb2_buf, 0))
				break;
#endif
			memcpy(vbuf + pos, srcbuf, pitch);
			pos += pitch * (single ? 1 : 2);
			srcbuf += pitch;
		}
	}
	return 1;
}

void TW68_video_irq(struct TW68_dev *dev, unsigned long requests, unsigned int pb_status, unsigned int fifo_status,
					unsigned int *reset_ch)
{
	enum v4l2_field field;
	int Fn, PB;
	int single_field;
	unsigned long flags = 0;
	struct TW68_vc *vc;
	struct TW68_buf *buf;
	unsigned int ch;
	bool process_field;
	for_each_set_bit(ch, &requests, MAX_CHANNELS)
	{
		vc = &dev->vc[ch];
		if (!vb2_is_streaming(&vc->vb_vidq))
			continue;
		if (vc->no_signal && !(fifo_status & BIT(ch)))
		{
			v4l2_printk(KERN_DEBUG, &dev->v4l2_dev, "video%d: signal recovered\n", vc->nId);
			vc->no_signal = false;
			*reset_ch |= BIT(ch);
			vc->Done = 0;
			continue;
		}
		vc->no_signal = !!(fifo_status & BIT(ch));
		/* check fifo errors if there's a signal */
		if (!vc->no_signal)
		{
			u32 fifo_ov, fifo_bad;
			fifo_ov = (fifo_status >> 24) & BIT(ch);
			fifo_bad = (fifo_status >> 16) & BIT(ch);
			if (fifo_ov || fifo_bad)
			{
				/* Mark this channel for reset */
				v4l2_printk(KERN_DEBUG, &dev->v4l2_dev,
							"video%d: FIFO error\n", vc->nId);
				*reset_ch |= BIT(ch);
				vc->Done = 0;
				continue;
			}
		}
		Fn = (pb_status >> 24) & (1 << ch);
		PB = (pb_status) & (1 << ch);
		spin_lock_irqsave(&vc->qlock, flags);
		if (list_empty(&vc->buf_list))
		{
			vc->Done = 0;
			spin_unlock_irqrestore(&vc->qlock, flags);
			continue;
		}
		buf = list_entry(vc->buf_list.next, struct TW68_buf, list);
		spin_unlock_irqrestore(&vc->qlock, flags);
		if (buf == NULL)
		{
			printk(KERN_ERR "no buffer\n");
			continue;
		}
		field = vc->field;
		process_field = false;
		single_field = ((field == V4L2_FIELD_BOTTOM) || (field == V4L2_FIELD_TOP) || (field == V4L2_FIELD_ALTERNATE)) && sfield;
		if (field==V4L2_FIELD_ALTERNATE) process_field =true; else if (Fn == 0) process_field =true;

		//  weave frame output, fill queued buffer.
		if (process_field)
		{
			// field 0 interrupt  program update  P field mapping
			if (!vb2_is_streaming(&vc->vb_vidq))
				continue;
			vc->Done = BF_Copy(dev, ch, Fn, PB, single_field, buf);
			vc->curPB = PB;
			if (single_field && vc->Done)
			{
				spin_lock_irqsave(&vc->qlock, flags);
				vc->Done = 0;
				list_del(&buf->list);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
				buf->vb.v4l2_buf.field = vc->field;
				buf->vb.v4l2_buf.sequence = vc->framecount;
#else
				// buf->vb.field = vc->field;
				if (Fn == 0)
					buf->vb.field = V4L2_FIELD_TOP;
				else
					buf->vb.field = V4L2_FIELD_BOTTOM;

				buf->vb.sequence = vc->framecount;
#endif
				vc->framecount++;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
				v4l2_get_timestamp(&buf->vb.v4l2_buf.timestamp);
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
				v4l2_get_timestamp(&buf->vb.timestamp);
#else
				buf->vb.vb2_buf.timestamp = ktime_get_ns();
#endif
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
				vb2_buffer_done(&buf->vb, VB2_BUF_STATE_DONE);
#else
				vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
#endif
				spin_unlock_irqrestore(&vc->qlock, flags);
			}
			continue;
		}
		if (single_field || !vc->Done)
			continue;
		// copy bottom field, but only copy if first field was done.
		// otherwise it will start on the next frame as desired
		if (PB != vc->curPB)
		{
			printk(KERN_DEBUG "s812: DMA mismatch, skipping frame\n");
			vc->Done = 0;
			vc->stat.pb_mismatch++;
			*reset_ch |= BIT(ch);
			continue;
		}
		vc->Done = 0;
		if (!vb2_is_streaming(&vc->vb_vidq))
			continue;
		BF_Copy(dev, ch, Fn, PB, 0, buf);
		spin_lock_irqsave(&vc->qlock, flags);
		list_del(&buf->list);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
		buf->vb.v4l2_buf.field = vc->field;
		buf->vb.v4l2_buf.sequence = vc->framecount;
		v4l2_get_timestamp(&buf->vb.v4l2_buf.timestamp);
#else
		buf->vb.field = vc->field;
		buf->vb.sequence = vc->framecount;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
		v4l2_get_timestamp(&buf->vb.timestamp);
#else
		buf->vb.vb2_buf.timestamp = ktime_get_ns();
#endif
#endif
		vc->framecount++;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
		vb2_buffer_done(&buf->vb, VB2_BUF_STATE_DONE);
#else
		vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
#endif
		spin_unlock_irqrestore(&vc->qlock, flags);
	}
	return;
}

void TW68_irq_video_done(struct TW68_dev *dev, unsigned int nId, u32 dwRegPB)
{
	enum v4l2_field field;
	int Fn, PB;
	int single_field;
	unsigned long flags = 0;
	struct TW68_vc *vc;
	struct TW68_buf *buf;

	if (nId >= 8)
		return;

	vc = &dev->vc[nId];
	if (!vb2_is_streaming(&vc->vb_vidq))
		return;
	Fn = (dwRegPB >> 24) & (1 << nId);
	PB = (dwRegPB) & (1 << nId);
	spin_lock_irqsave(&vc->qlock, flags);
	if (list_empty(&vc->buf_list))
	{
		vc->Done = 0;
		spin_unlock_irqrestore(&vc->qlock, flags);
		return;
	}
	buf = list_entry(vc->buf_list.next, struct TW68_buf, list);
	spin_unlock_irqrestore(&vc->qlock, flags);
	if (buf == NULL)
	{
		printk(KERN_ERR "no buffer\n");
		return;
	}
	field = vc->field;
	single_field = ((field == V4L2_FIELD_BOTTOM) || (field == V4L2_FIELD_TOP) || (field == V4L2_FIELD_ALTERNATE)) && sfield;
	//  weave frame output, fill queued buffer.
	if (Fn == 0)
	{
		// field 0 interrupt  program update  P field mapping
		if (!vb2_is_streaming(&vc->vb_vidq))
			return;
		vc->Done = BF_Copy(dev, nId, Fn, PB, single_field, buf);
		vc->curPB = PB;
		if (single_field && vc->Done)
		{
			spin_lock_irqsave(&vc->qlock, flags);
			vc->Done = 0;
			list_del(&buf->list);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
			buf->vb.v4l2_buf.field = vc->field;
			buf->vb.v4l2_buf.sequence = vc->framecount;
#else
			buf->vb.field = vc->field;
			buf->vb.sequence = vc->framecount;
#endif
			vc->framecount++;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
			v4l2_get_timestamp(&buf->vb.v4l2_buf.timestamp);
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
			v4l2_get_timestamp(&buf->vb.timestamp);
#else
			buf->vb.vb2_buf.timestamp = ktime_get_ns();
#endif
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
			vb2_buffer_done(&buf->vb, VB2_BUF_STATE_DONE);
#else
			vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
#endif
			spin_unlock_irqrestore(&vc->qlock, flags);
		}
		return;
	}
	if (single_field || !vc->Done)
		return;
	// copy bottom field, but only copy if first field was done.
	// otherwise it will start on the next frame as desired
	if (PB != vc->curPB)
	{
		printk(KERN_DEBUG "s812: DMA mismatch, skipping frame\n");
		vc->Done = 0;
		vc->stat.pb_mismatch++;
		return;
	}
	vc->Done = 0;
	if (!vb2_is_streaming(&vc->vb_vidq))
		return;
	BF_Copy(dev, nId, Fn, PB, 0, buf);
	spin_lock_irqsave(&vc->qlock, flags);
	list_del(&buf->list);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	buf->vb.v4l2_buf.field = vc->field;
	buf->vb.v4l2_buf.sequence = vc->framecount;
	v4l2_get_timestamp(&buf->vb.v4l2_buf.timestamp);
#else
	buf->vb.field = vc->field;
	buf->vb.sequence = vc->framecount;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 5, 0)
	v4l2_get_timestamp(&buf->vb.timestamp);
#else
	buf->vb.vb2_buf.timestamp = ktime_get_ns();
#endif
#endif
	vc->framecount++;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
	vb2_buffer_done(&buf->vb, VB2_BUF_STATE_DONE);
#else
	vb2_buffer_done(&buf->vb.vb2_buf, VB2_BUF_STATE_DONE);
#endif
	spin_unlock_irqrestore(&vc->qlock, flags);
	return;
}

static void TW68_video_device_release(struct video_device *vdev)
{
	struct TW68_vc *vc =
		container_of(vdev, struct TW68_vc, vdev);

	v4l2_ctrl_handler_free(&vc->hdl);
	return;
}

int vdev_init(struct TW68_dev *dev)
{
	struct video_device *vdev;
	int k;
	int err0;
	struct vb2_queue *q;
	struct TW68_vc *vc;
	int rc;
	int j;

	mutex_init(&dev->start_lock);
	for (k = 0; k < 8; k++)
	{
		vc = &dev->vc[k];
		mutex_init(&vc->vb_lock);
		spin_lock_init(&vc->qlock);
		v4l2_ctrl_handler_init(&vc->hdl, CTRLS);
		for (j = 0; j < CTRLS; j++)
		{
			v4l2_ctrl_new_std(&vc->hdl, &TW68_ctrl_ops,
							  video_ctrls[j].id,
							  video_ctrls[j].minimum,
							  video_ctrls[j].maximum,
							  video_ctrls[j].step,
							  video_ctrls[j].default_value);
			if (vc->hdl.error)
			{
				printk(KERN_ERR
					   "error %x at %d\n",
					   vc->hdl.error, j);
				break;
			}
		}
		v4l2_ctrl_new_custom(&vc->hdl, &TW68_gpio_ctrl, NULL);
		if (vc->hdl.error)
		{
			printk(KERN_ERR
				   "error adding ctrl %x\n",
				   vc->hdl.error);
			v4l2_ctrl_handler_free(&vc->hdl);
			return vc->hdl.error;
		}
		v4l2_ctrl_handler_setup(&vc->hdl);
		q = &vc->vb_vidq;
		q->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		q->io_modes = VB2_MMAP | VB2_READ | VB2_USERPTR;
		q->lock = &vc->vb_lock;
		q->buf_struct_size = sizeof(struct TW68_buf);
		q->mem_ops = &vb2_vmalloc_memops;
		q->ops = &s812_vb2_ops;
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 9, 0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 15, 0)
		q->timestamp_type = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
#else
		q->timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC;
#endif
#endif
		q->drv_priv = vc;
		rc = vb2_queue_init(q);
		if (rc != 0)
		{
			printk(KERN_ERR "vb2_queue_init failed! %x\n", rc);
			return rc;
		}
		vc->height = 640;
		vc->width = 480;
		vc->fmt = format_by_fourcc(V4L2_PIX_FMT_YUYV); /// YUY2 by default
		vc->field = V4L2_FIELD_INTERLACED;
		vdev = &vc->vdev;
		*vdev = tw68_video_template;
		vdev->v4l2_dev = &dev->v4l2_dev;
		vdev->ctrl_handler = &vc->hdl;
		vdev->queue = q;
		vdev->minor = -1;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)
		vdev->device_caps = V4L2_CAP_VIDEO_CAPTURE |
							V4L2_CAP_STREAMING | V4L2_CAP_READWRITE;
#endif
		vdev->release = TW68_video_device_release;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
		vdev->debug = video_debug;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 7, 0)
		vdev->vfl_dir = VFL_DIR_RX;
#endif
		vdev->lock = &vc->vb_lock;
		snprintf(vdev->name, sizeof(vdev->name), "%s (%s22)",
				 dev->name, TW68_boards[dev->board].name);
		printk(KERN_INFO "%s:[%d],vdevname: %s, vdevev[%d] 0x%p\n", __func__, k,
			   vc->vdev.name, k, vdev);
		video_set_drvdata(vdev, vc);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)
		err0 = video_register_device(&vc->vdev, VFL_TYPE_VIDEO, video_nr[dev->nr]);
#else
		err0 = video_register_device(&vc->vdev, VFL_TYPE_GRABBER, video_nr[dev->nr]);
#endif
		vc->vfd_DMA_num = vdev->num;
		printk(KERN_INFO "%s:[%d],vdevname: %s, minor %d, DMA %d, err0 %d\n", __func__,
			   k, vdev->name, vc->vdev.minor, vc->vfd_DMA_num, err0);
	}
	printk(KERN_INFO "%s Video DEVICE NAME : %s\n", __func__, dev->vc[0].vdev.name);
	return 0;
}
