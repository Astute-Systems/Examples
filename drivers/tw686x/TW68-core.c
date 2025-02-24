/*
 *
 * device driver for TW6869 based PCIe capture cards
 * driver core for hardware
 *
 * VIDEOBUF2 code and improvements
 */

#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/kmod.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/pm.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/sound.h>

#include "TW68.h"
#include "TW68_defines.h"
#include "version.h"

MODULE_DESCRIPTION("v4l2 driver module for Intersil tw686x. Version: " TW686X_VERSION);
MODULE_AUTHOR("Ross Newman, Astute Systems");
MODULE_LICENSE("GPL");

#ifdef CONFIG_PROC_FS
static int tw68_read_show(struct seq_file *m, void *v);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 5, 0)
static const struct proc_ops tw68_procmem_fops;
#else
static const struct file_operations tw68_procmem_fops;
#endif
#endif

static unsigned int irq_debug;
module_param(irq_debug, int, 0644);
MODULE_PARM_DESC(irq_debug, "enable debug messages [IRQ handler]");

static unsigned int core_debug = 0;
module_param(core_debug, int, 0644);
MODULE_PARM_DESC(core_debug, "enable debug messages [core]");

static unsigned int gpio_tracking;
module_param(gpio_tracking, int, 0644);
MODULE_PARM_DESC(gpio_tracking, "enable debug messages [gpio]");

static unsigned int alsa = 1;
module_param(alsa, int, 0644);
MODULE_PARM_DESC(alsa, "enable/disable ALSA DMA sound [dmasound]");

static unsigned int latency = UNSET;
module_param(latency, int, 0444);
MODULE_PARM_DESC(latency, "pci latency timer");
int TW68_no_overlay = -1;

static unsigned int vbi_nr[] = {[0 ...(TW68_MAXBOARDS - 1)] = UNSET};
static unsigned int radio_nr[] = {[0 ...(TW68_MAXBOARDS - 1)] = UNSET};
static unsigned int tuner[] = {[0 ...(TW68_MAXBOARDS - 1)] = UNSET};
static unsigned int card[] = {[0 ...(TW68_MAXBOARDS - 1)] = UNSET};

module_param_array(vbi_nr, int, NULL, 0444);
module_param_array(radio_nr, int, NULL, 0444);
module_param_array(tuner, int, NULL, 0444);
module_param_array(card, int, NULL, 0444);

MODULE_PARM_DESC(vbi_nr, "vbi device number");
MODULE_PARM_DESC(radio_nr, "radio device number");
MODULE_PARM_DESC(tuner, "tuner type");
MODULE_PARM_DESC(card, "card type");

DEFINE_MUTEX(TW686v_devlist_lock);
EXPORT_SYMBOL(TW686v_devlist_lock);
LIST_HEAD(TW686v_devlist);
EXPORT_SYMBOL(TW686v_devlist);

#define container_of_local(ptr, type, member)          \
  ({                                                   \
    const typeof(((type *)0)->member) *__mptr = (ptr); \
    (type *)((char *)__mptr - offsetof(type, member)); \
  })

static char name_comp1[] = "Composite1";
static char name_comp2[] = "Composite2";
static char name_comp3[] = "Composite3";
static char name_comp4[] = "Composite4";

static DEFINE_MUTEX(TW68_devlist_lock);

struct TW68_board TW68_boards[] = {
    [TW68_BOARD_UNKNOWN] =
        {
            .name = "TW6869",
            .audio_clock = 0,
            .tuner_type = TUNER_ABSENT,
            .radio_type = UNSET,
            .tuner_addr = ADDR_UNSET,
            .radio_addr = ADDR_UNSET,

            .inputs = {{
                .name = name_comp1,
                .vmux = 0,
                .amux = LINE1,
            }},
        },

    [TW68_BOARD_A] =
        {
            .name = "TW6869",
            .audio_clock = 0,
            .tuner_type = TUNER_ABSENT,
            .radio_type = UNSET,
            .tuner_addr = ADDR_UNSET,
            .radio_addr = ADDR_UNSET,

            .inputs = {{
                           .name = name_comp1,
                           .vmux = 0,
                           .amux = LINE1,
                       },
                       {
                           .name = name_comp2,
                           .vmux = 1,
                           .amux = LINE2,
                       },
                       {
                           .name = name_comp3,
                           .vmux = 2,
                           .amux = LINE3,
                       },
                       {
                           .name = name_comp4,
                           .vmux = 4,
                           .amux = LINE4,
                       }},
        },

};

const unsigned int TW68_bcount = ARRAY_SIZE(TW68_boards);

struct pci_device_id TW68_pci_tbl[] = {

    {
        .vendor = 0x1797,
        .device = 0x6869,
        .subvendor = PCI_ANY_ID,
        .subdevice = PCI_ANY_ID,
    },
    {
        .vendor = 0x6000,
        .device = 0x0812,
        .subvendor = PCI_ANY_ID,
        .subdevice = PCI_ANY_ID,
    },
    {}};

MODULE_DEVICE_TABLE(pci, TW68_pci_tbl);
static LIST_HEAD(mops_list);
static unsigned int TW68_devcount;

static u32 video_framerate[3][6] = {{
                                        0xBFFFFFFF,  // 30 full
                                        0xBFFFCFFF,  // 28
                                        0x8FFFCFFF,  // 26
                                        0xBF3F3F3F,  // 24 fps
                                        0xB3CFCFC3,  // 22
                                        0x8F3CF3CF   // 20 fps
                                    },
                                    {30, 28, 26, 24, 22, 20},
                                    {25, 23, 20, 18, 16, 14}

};

int (*TW68_dmasound_init)(struct TW68_dev *dev);
int (*TW68_dmasound_exit)(struct TW68_dev *dev);
#define dprintk(fmt, arg...) \
  if (core_debug) printk(KERN_DEBUG "%s/core: " fmt, dev->name, ##arg)

void tw68v_set_framerate(struct TW68_dev *dev, u32 ch, u32 n) {
  if (n >= 0 && n < 6) {
    if (ch >= 0 && ch < MAX_CHANNELS) tw68_reg_writel(dev, DROP_FIELD_REG0 + ch, video_framerate[n][0]);
    printk("%s: ch:%d, n:%d, fps:%d FPS \n ", __func__, ch, n, video_framerate[n][1]);
  }
}

int TW68_buffer_pages(int size) {
  size = PAGE_ALIGN(size);
  size += PAGE_SIZE;  // for non-page-aligned buffers
  size /= 4096;
  return size;
}
/* calc max # of buffers from size (must not exceed the 4MB virtual
 * address space per DMA channel) */
int TW68_buffer_count(unsigned int size, unsigned int count) {
  unsigned int maxcount;

  maxcount = 1024 / TW68_buffer_pages(size);
  if (count > maxcount) count = maxcount;
  return count;
}

int videoDMA_pgtable_alloc(struct pci_dev *pci, struct TW68_pgtable *pt) {
  __le32 *cpu;
  dma_addr_t dma_addr, phy_addr;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
  cpu = pci_alloc_consistent(pci, PAGE_SIZE << 3, &dma_addr);
#else
  cpu = dma_alloc_coherent(&pci->dev, PAGE_SIZE << 3, &dma_addr, GFP_KERNEL);
#endif
  if (NULL == cpu) return -ENOMEM;
  pt->size = PAGE_SIZE << 3;
  pt->cpu = cpu;
  pt->dma = dma_addr;
  phy_addr = dma_addr + (PAGE_SIZE << 2) + (PAGE_SIZE << 1);
  return 0;
}

void TW68_pgtable_free(struct pci_dev *pci, struct TW68_pgtable *pt) {
  if (NULL == pt->cpu) return;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
  pci_free_consistent(pci, pt->size, pt->cpu, pt->dma);
#else
  dma_free_coherent(&pci->dev, pt->size, pt->cpu, pt->dma);
#endif
  pt->cpu = NULL;
}

void BFDMA_setup(struct TW68_dev *dev, int nDMA_channel, int H, int W) {
  u32 regDW, dwV, dn;
  // n   Field0   P B    Field1  P B     WidthHightPitch
  tw68_reg_writel(dev, (BDMA_ADDR_P_0 + nDMA_channel * 8), dev->BDbuf[nDMA_channel][0].dma_addr);  // P DMA page table
  tw68_reg_writel(dev, (BDMA_ADDR_B_0 + nDMA_channel * 8), dev->BDbuf[nDMA_channel][1].dma_addr);
  // [31:22] total active lines (height)
  // [21:11] total bytes per line (line_width)
  // [10:0] total active bytes per line (active_width)
  tw68_reg_writel(dev, (BDMA_WHP_0 + nDMA_channel * 8), (W & 0x7FF) | ((W & 0x7FF) << 11) | ((H & 0x3FF) << 22));
  tw68_reg_writel(dev, (BDMA_ADDR_P_F2_0 + nDMA_channel * 8),
                  dev->BDbuf[nDMA_channel][2].dma_addr);  // P DMA page table
  tw68_reg_writel(dev, (BDMA_ADDR_B_F2_0 + nDMA_channel * 8), dev->BDbuf[nDMA_channel][3].dma_addr);
  tw68_reg_writel(dev, (BDMA_WHP_F2_0 + nDMA_channel * 8), (W & 0x7FF) | ((W & 0x7FF) << 11) | ((H & 0x3FF) << 22));
  regDW = tw68_reg_readl(dev, PHASE_REF_CONFIG);
  dn = (nDMA_channel << 1) + 0x10;
  dwV = (0x3 << dn);
  regDW |= dwV;
  tw68_reg_writel(dev, PHASE_REF_CONFIG, regDW);
  dwV = tw68_reg_readl(dev, PHASE_REF_CONFIG);
  //  printk(KERN_INFO "DMA mode setup %s: %d PHASE_REF_CONFIG  dn 0x%lx    0x%lx  0x%lx  H%d W%d  \n",
  //		dev->name, nDMA_channel, regDW, dwV, dn, H, W );
}

void DecoderResize(struct TW68_dev *dev, int nId, int nHeight, int nWidth) {
  u32 nAddr, nHW, nH, nW, nVal, nReg, regDW;

  if (nId > 8) {
    printk("DecoderResize() error: nId:%d,Width=%d,Height=%d\n", nId, nWidth, nHeight);
    return;
  }
  // only for internal 4     HDelay VDelay   etc
  nReg = 0xe6;  //  blue back color
  tw68_reg_writel(dev, MISC_CONTROL2, nReg);
  tw68_reg_writel(dev, MISC_CONTROL2 + 0x100, nReg);
  if (dev->vc[nId].PAL50) {
    // VDelay
    tw68_reg_writel(dev, MISC_CONTROL3, 0xc5);
    tw68_reg_writel(dev, MISC_CONTROL3 + 0x100, 0xc5);
    regDW = 0x18;
    if (nId < 4) {
      nAddr = VDELAY0 + (nId * 0x10);
      tw68_reg_writel(dev, nAddr, regDW);
    } else {
      nAddr = VDELAY0 + ((nId - 4) * 0x10) + 0x100;
      tw68_reg_writel(dev, nAddr, regDW);
    }
    // HDelay
    regDW = 0x0A;
    regDW = 0x0C;
    if (nId < 4) {
      nAddr = HDELAY0 + (nId * 0x10);
      tw68_reg_writel(dev, nAddr, regDW);
    } else {
      nAddr = HDELAY0 + ((nId - 4) * 0x10) + 0x100;
      tw68_reg_writel(dev, nAddr, regDW);
    }
  } else {
    // VDelay
    tw68_reg_writel(dev, MISC_CONTROL3, 0x85);
    tw68_reg_writel(dev, MISC_CONTROL3 + 0x100, 0x85);
    regDW = 0x14;
    if (nId < 4) {
      nAddr = VDELAY0 + (nId * 0x10);
      tw68_reg_writel(dev, nAddr, regDW);
    } else {
      nAddr = VDELAY0 + ((nId - 4) * 0x10) + 0x100;
      tw68_reg_writel(dev, nAddr, regDW);
    }
    // HDelay
    regDW = 0x0D;
    regDW = 0x0E;
    if (nId < 4) {
      nAddr = HDELAY0 + (nId * 0x10);
      tw68_reg_writel(dev, nAddr, regDW);
    } else {
      nAddr = HDELAY0 + ((nId - 4) * 0x10) + 0x100;
      tw68_reg_writel(dev, nAddr, regDW);
    }
  }
  // adjust start pixel
  nVal = tw68_reg_readl(dev, HDELAY0 + nId);
  // reg_writel (HACTIVE_L0+nId, 0xC0);  // 2C0  704
  nHW = nWidth | (nHeight << 16) | (1 << 31);
  nH = nW = nHW;
  // Video Size
  tw68_reg_writel(dev, VIDEO_SIZE_REG, nHW);         // for Rev.A backward compatible
  tw68_reg_writel(dev, VIDEO_SIZE_REG0 + nId, nHW);  // for Rev.B or later only
  if (((nHeight == 240) || (nHeight == 288)) && (nWidth >= 700)) nWidth = 720;
  if (((nHeight == 240) || (nHeight == 288)) && (nWidth > 699))
    nWidth = 720;
  else
    nWidth = (16 * nWidth / 720) + nWidth;
  // decoder  Scale
  nW = nWidth & 0x7FF;
  nW = (720 * 256) / nW;
  nH = nHeight & 0x1FF;
  if (dev->vc[nId].PAL50)
    nH = (288 * 256) / nH;
  else
    nH = (240 * 256) / nH;

  if (nId >= 4) {
    nAddr = (EX_VSCALE1_LO + ((nId - 4) << 1) + (nId - 4)) + 0x100;  // EX_VSCALE1_LO + 0|3|6|9
    nAddr = VSCALE1_LO + ((nId - 4) << 4) + 0x100;                   // 6869 0x200 VSCALE1_LO + 0|0x10|0x20|0x30
  } else
    nAddr = VSCALE1_LO + (nId << 4);  // VSCALE1_LO + 0|0x10|0x20|0x30
  nVal = nH & 0xFF;                   // V
  tw68_reg_writel(dev, nAddr, nVal);
  nReg = tw68_reg_readl(dev, nAddr);
  nAddr++;  // V H
  nVal = (((nH >> 8) & 0xF) << 4) | ((nW >> 8) & 0xF);
  tw68_reg_writel(dev, nAddr, nVal);
  nReg = tw68_reg_readl(dev, nAddr);
  nAddr++;  // H
  nVal = nW & 0xFF;
  if (nId < 4) {
    tw68_reg_writel(dev, nAddr, nVal);
    nReg = tw68_reg_readl(dev, nAddr);
  }
  tw68_reg_writel(dev, nAddr, nVal);
  nReg = tw68_reg_readl(dev, nAddr);
  nAddr++;  // H
  nVal = nW & 0xFF;
  if (nId < 4) {
    tw68_reg_writel(dev, nAddr, nVal);
    nReg = tw68_reg_readl(dev, nAddr);
  }
  tw68_reg_writel(dev, nAddr, nVal);
  nReg = tw68_reg_readl(dev, nAddr);
  // H Scaler
  nVal = (nWidth - 12 - 4) * (1 << 16) / nWidth;
  nVal = (4 & 0x1F) | (((nWidth - 12) & 0x3FF) << 5) | (nVal << 15);
  tw68_reg_writel(dev, SHSCALER_REG0 + nId, nVal);
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
void resync(struct timer_list *t)
#else
void resync(unsigned long data)
#endif
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
  struct TW68_dev *dev = from_timer(dev, t, delay_resync);
#else
  struct TW68_dev *dev = (struct TW68_dev *)data;
#endif
  u32 dwRegE, dwRegF, k, m, mask;
  unsigned long now = jiffies;
  unsigned long flags = 0;
  int videoRS_ID = 0;

  if (dev == NULL) return;
  mod_timer(&dev->delay_resync, jiffies + msecs_to_jiffies(50));
  if (now - dev->vc[0].errlog < msecs_to_jiffies(50)) return;
  m = 0;
  mask = 0;
  spin_lock_irqsave(&dev->slock, flags);
  for (k = 0; k < MAX_CHANNELS; k++) {
    mask = ((dev->videoDMA_ID ^ dev->videoCap_ID) & (1 << k));
    if ((mask)) {
      m++;
      videoRS_ID |= mask;
      if ((m > 1) || dev->videoDMA_ID) k = 16;
    }
  }
  if (!videoRS_ID) {
    spin_unlock_irqrestore(&dev->slock, flags);
    return;
  }
  dev->videoRS_ID |= videoRS_ID;
  if ((dev->videoDMA_ID == 0) && dev->videoRS_ID) {
    int dwRegCur = tw68_reg_readl(dev, DMA_CHANNEL_ENABLE);
    dev->videoDMA_ID = dev->videoRS_ID;
    dwRegE = (dev->videoDMA_ID | (dwRegCur & 0xff00));
    tw68_reg_writel(dev, DMA_CHANNEL_ENABLE, dwRegE);
    dwRegE = tw68_reg_readl(dev, DMA_CHANNEL_ENABLE);
    dwRegF = (1 << 31) | dwRegE;
    dwRegF |= 0xff00;
    tw68_reg_writel(dev, DMA_CMD, dwRegF);
    dwRegF = tw68_reg_readl(dev, DMA_CMD);
    dev->videoRS_ID = 0;
  }
  spin_unlock_irqrestore(&dev->slock, flags);
}

int TW68_set_dmabits(struct TW68_dev *dev, unsigned int DMA_nCH) {
  u32 dwRegE, dwRegF, nId, k, run;
  unsigned long flags = 0;

  nId = DMA_nCH;
  spin_lock_irqsave(&dev->slock, flags);
  dwRegE = tw68_reg_readl(dev, DMA_CHANNEL_ENABLE);
  dev->video_DMA_1st_started += 1;
  dwRegE |= (1 << nId);  // +1 + 0  Fixed PB
  dev->videoCap_ID |= (1 << nId);
  dev->videoDMA_ID |= (1 << nId);
  tw68_reg_writel(dev, DMA_CHANNEL_ENABLE, dwRegE);
  run = 0;
  for (k = 0; k < 8; k++) {
    if (run < dev->vc[k].videoDMA_run) run = dev->vc[k].videoDMA_run;
  }
  dev->vc[nId].videoDMA_run = run + 1;
  dwRegF = (1 << 31);
  dwRegF |= dwRegE;
  tw68_reg_writel(dev, DMA_CMD, dwRegF);
  spin_unlock_irqrestore(&dev->slock, flags);
  return 0;
}

// called with spinlock held
int stop_video_DMA(struct TW68_dev *dev, unsigned int DMA_nCH) {
  u32 dwRegE, dwRegF, nId;
  u32 audioCap;

  nId = DMA_nCH;
  dwRegE = tw68_reg_readl(dev, DMA_CHANNEL_ENABLE);
  audioCap = (dwRegE >> 8) & 0xff;
  dwRegF = tw68_reg_readl(dev, DMA_CMD);
  dwRegE &= ~(1 << nId);
  tw68_reg_writel(dev, DMA_CHANNEL_ENABLE, dwRegE);
  dwRegE = tw68_reg_readl(dev, DMA_CHANNEL_ENABLE);
  dev->videoDMA_ID &= ~(1 << nId);
  dev->videoCap_ID &= ~(1 << nId);
  if ((dev->videoCap_ID != 0)) {
    dwRegF = 0x8000ffffUL;
    dwRegF &= ~(1 << nId);
    tw68_reg_writel(dev, DMA_CMD, dwRegF);
    tw68_reg_writel(dev, DMA_CMD, dwRegF);
    dwRegF = tw68_reg_readl(dev, DMA_CMD);
    dwRegF = tw68_reg_readl(dev, DMA_CMD);
  }
  dev->vc[nId].videoDMA_run = 0;

  // stop video, but not the audio stream
  if ((dev->videoCap_ID == 0) && !audioCap) {
    tw68_reg_writel(dev, DMA_CMD, 0);
    tw68_reg_writel(dev, DMA_CHANNEL_ENABLE, 0);
    tw68_reg_writel(dev, DMA_CHANNEL_ENABLE, 0);
    (void)tw68_reg_readl(dev, DMA_CHANNEL_ENABLE);
    dev->video_DMA_1st_started = 0;  // initial value for skipping startup DMA error
  }
  return 0;
}

int VideoDecoderDetect(struct TW68_dev *dev, unsigned int DMA_nCH) {
  u32 regDW, dwReg;

  if (DMA_nCH < 4) {
    // VD 1-4
    regDW = tw68_reg_readl(dev, DECODER0_STATUS + (DMA_nCH * 0x10));
    dwReg = tw68_reg_readl(dev, DECODER0_SDT + (DMA_nCH * 0x10));
  } else {
    // 6869  VD 5-8
    regDW = tw68_reg_readl(dev, DECODER0_STATUS + ((DMA_nCH - 4) * 0x10) + 0x100);
    dwReg = tw68_reg_readl(dev, DECODER0_SDT + ((DMA_nCH - 4) * 0x10) + 0x100);
    /// printk("\n\n Decoder 0x%X VideoStandardDetect DMA_nCH %d  regDW 0x%x  dwReg%d \n", (DECODER0_STATUS + (DMA_nCH*
    /// 0x10) + 0x100), regDW, dwReg );
  }

  if ((regDW & 1)) {
    // set to PAL 50
    printk("50HZ VideoStandardDetect DMA_nCH %d  regDW 0x%x  dwReg%d \n", DMA_nCH, regDW, dwReg);
    return 50;
  } else {
    printk("60HZ VideoStandardDetect DMA_nCH %d  regDW 0x%x  dwReg%d \n", DMA_nCH, regDW, dwReg);
    return 60;
  }
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
static void tw68_dma_delay(struct timer_list *t)
#else
static void tw68_dma_delay(unsigned long data)
#endif

{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0)
  struct TW68_dev *dev = from_timer(dev, t, dma_delay_timer);
#else
  struct TW68_dev *dev = (struct TW68_dev *)data;
#endif

  unsigned long flags;

  spin_lock_irqsave(&dev->slock, flags);
  tw68_reg_writel(dev, DMA_CHANNEL_ENABLE, dev->pending_dma_en);
  tw68_reg_writel(dev, DMA_CMD, dev->pending_dma_cmd);
  dev->pending_dma_en = 0;
  dev->pending_dma_cmd = 0;
  spin_unlock_irqrestore(&dev->slock, flags);
}

void tw68_reset_channels(struct TW68_dev *dev, unsigned int ch_mask) {
  u32 dma_en, dma_cmd;

  dma_en = tw68_reg_readl(dev, DMA_CHANNEL_ENABLE);
  dma_cmd = tw68_reg_readl(dev, DMA_CMD);

  /*
   * Save pending register status, the timer will
   * restore them.
   */
  dev->pending_dma_en |= dma_en;
  dev->pending_dma_cmd |= dma_cmd;

  /* Disable the reset channels */
  tw68_reg_writel(dev, DMA_CHANNEL_ENABLE, dma_en & ~ch_mask);

  if ((dma_en & ~ch_mask) == 0) {
    dev_dbg(&dev->pci->dev, "reset: stopping DMA\n");
    dma_cmd &= ~DMA_CMD;
  }
  tw68_reg_writel(dev, DMA_CMD, dma_cmd & ~ch_mask);
}

static irqreturn_t TW68_irq(int irq, void *dev_id) {
  struct TW68_dev *dev = (struct TW68_dev *)dev_id;
  unsigned long flags;
  //	u32 dwRegST, dwRegER, dwRegPB, dwRegE, dwRegF, dwRegVP, dwErrBit;
  u32 audio_requests = 0;
  u32 video_requests = 0;
  u32 int_status, dma_en, video_en, pb_status, fifo_status;
  unsigned int reset_ch;
  u32 fifo_ov, fifo_bad, fifo_errors, fifo_signal;
  int j;

  int_status = tw68_reg_readl(dev, DMA_INT_STATUS);
  // DMA_INT_ERROR named FIFO_STATUS in TW6869 manual
  fifo_status = tw68_reg_readl(dev, DMA_INT_ERROR);
  if (!int_status && !TW68_FIFO_ERROR(fifo_status)) return IRQ_NONE;

  if (int_status & TW6864_R_DMA_INT_STATUS_TOUT) {
    for (j = 0; j < 8; j++) {
      dev->vc[j].stat.dma_timeout++;
    }
    dev_dbg(&dev->pci->dev, "DMA timeout. resetting dma for all channels\n");
    reset_ch = ~0;
    goto reset_channels;
  }
  spin_lock_irqsave(&dev->slock, flags);
  dma_en = tw68_reg_readl(dev, DMA_CHANNEL_ENABLE);
  // dwRegER = tw68_reg_readl(dev, DMA_INT_ERROR);
  // dwRegPB = tw68_reg_readl(dev, DMA_PB_STATUS);
  // dwRegVP = tw68_reg_readl(dev, VIDEO_PARSER_STATUS);
  spin_unlock_irqrestore(&dev->slock, flags);

  video_en = dma_en & 0xff;
  fifo_signal = ~(fifo_status & 0xff) & video_en;
  fifo_ov = fifo_status >> 24;
  fifo_bad = fifo_status >> 16;
  fifo_errors = fifo_signal & (fifo_ov | fifo_bad);
  reset_ch = 0;
  pb_status = tw68_reg_readl(dev, DMA_PB_STATUS);

  video_requests = (int_status & video_en) | fifo_errors;
  audio_requests = (int_status & dma_en) >> 8;

  if (video_requests) {
    TW68_video_irq(dev, video_requests, pb_status, fifo_status, &reset_ch);
#if 0
		for (k = 0; k < 8; k++) {
			//(dwRegST & dev->videoDMA_ID) & (1 << k)
			if (video_requests & (1 << k)) {
				/// exclude  inactive dev
				// TODO fixme
				TW68_irq_video_done(dev, k, pb_status);
			}
		}
#endif
  }
#ifdef AUDIO_BUILD
  if (audio_requests) {
    TW68_audio_irq(dev, audio_requests, pb_status);
  }
#endif
reset_channels:
  if (reset_ch) {
    spin_lock_irqsave(&dev->slock, flags);
    tw68_reset_channels(dev, reset_ch);
    spin_unlock_irqrestore(&dev->slock, flags);
    mod_timer(&dev->dma_delay_timer, jiffies + msecs_to_jiffies(100));
  }
  return IRQ_HANDLED;
}
#if 0

			//err_exist, such as cpl error, tlp error, time-out
			int eno;
			dwErrBit = 0;
			dwErrBit |= ((dwRegST >>24) & 0xFF);
			dwErrBit |= (((dwRegVP >>8) |dwRegVP )& 0xFF);
			///dwErrBit |= (((dwRegER >>24) |(dwRegER >>16) |(dwRegER ))& 0xFF);
			dwErrBit |= (((dwRegER >>24) |(dwRegER >>16))& 0xFF);
			spin_lock_irqsave(&dev->slock, flags);
			dwRegE = tw68_reg_readl(dev, DMA_CHANNEL_ENABLE);
			eno =0;
			for (k = 0; k < 8; k++) {
				if (dwErrBit & (1 << k)) {
					eno++;
					// Disable DMA channel
					dev->vc[k].stat.dma_err++;
					dwRegE &= ~((1<< k));
					if (eno >2)
						dwRegE &= ~((0xFF));
				}
			}
			// stop  all VIDEO error channels
			dev->videoDMA_ID = dwRegE & 0xff;
			tw68_reg_writel(dev, DMA_CHANNEL_ENABLE, dwRegE);
			dwRegF = tw68_reg_readl(dev, DMA_CHANNEL_ENABLE);
			dwRegF = (1<<31);
			dwRegF |= dwRegE;
			dwRegF |= 0xff00;
			tw68_reg_writel(dev, DMA_CMD, dwRegF);
			dwRegF = tw68_reg_readl(dev, DMA_CMD);
			spin_unlock_irqrestore(&dev->slock, flags);
			//printk("DeviceInterrupt: errors  ## dma_status  0x%X "
			//"(err) =0x%X   dwRegVP (video parser)=0X%x  "
			//"int_status 0x%x  # dwRegE 0X%x dwRegF 0X%x \n", 
			//dwErrBit, dwRegER, dwRegVP, dwRegST, dwRegE, dwRegF);
			dev->vc[0].errlog = jiffies;
			dev->last_dmaerr = dwErrBit;
		} else {
			// Normal interrupt:
			// printk("  Normal interrupt:  ++++ ## dma_status 0x%X "
			// "FIFO =0x%X   (video parser)=0X%x   int_status 0x%x  "
			// "PB 0x%x  # dwRegE 0X%x dwRegF 0X%x \n", 
			// dwRegST, dwRegER, dwRegVP, dwRegST, dwRegPB, dwRegE, dwRegF);
#ifdef AUDIO_BUILD
			audio_requests = (dwRegST >> 8);
			if (audio_requests) {
				TW68_audio_irq(dev, audio_requests, dwRegPB);
			}
#endif
			if ((dwRegST & (0xFF)) && (!(dwRegER >>16))) {
				for (k = 0; k < 8; k++) {
					if ((dwRegST & dev->videoDMA_ID) & (1 << k)) {
						/// exclude  inactive dev
						TW68_irq_video_done(dev, k, dwRegPB);
					}
				}
			}
			if (dev->videoRS_ID) {
				int curE = tw68_reg_readl(dev, DMA_CHANNEL_ENABLE);

				spin_lock_irqsave(&dev->slock, flags);
				dev->videoDMA_ID |= dev->videoRS_ID;
				dev-> videoRS_ID =0;
				dwRegE = dev->videoDMA_ID | (curE & 0xff00); 
				tw68_reg_writel(dev, DMA_CHANNEL_ENABLE, dwRegE);
				dwRegF = (1<<31);
				dwRegF |= dwRegE;
				dwRegF |= 0xff00;
				tw68_reg_writel(dev, DMA_CMD, dwRegF);
				dwRegF = tw68_reg_readl(dev, DMA_CMD);
				spin_unlock_irqrestore(&dev->slock, flags);
			}
		}
	}
	return IRQ_RETVAL(handled);
}
#endif
// early init (no i2c, no irq)
static int TW68_hwinit1(struct TW68_dev *dev) {
  u32 m_StartIdx, m_EndIdx, m_nVideoFormat, m_dwCHConfig, dwReg;
  u32 m_bHorizontalDecimate, m_bVerticalDecimate, m_nDropChannelNum;
  u32 m_bDropMasterOrSlave, m_bDropField, m_bDropOddOrEven, m_nCurVideoChannelNum;
  u32 regDW, val1, addr, k, ChannelOffset, pgn;

  spin_lock_init(&dev->slock);
  pci_read_config_dword(dev->pci, PCI_VENDOR_ID, &regDW);
  pci_read_config_dword(dev->pci, PCI_COMMAND, &regDW);  // 04 PCI_COMMAND
  printk(KERN_INFO "%s: %s: CFG[0x04] PCI_COMMAND :  0x%x\n", __func__, dev->name, regDW);
  regDW |= 7;
  regDW &= 0xfffffbff;
  pci_write_config_dword(dev->pci, PCI_COMMAND, regDW);
  pci_read_config_dword(dev->pci, 0x4, &regDW);
  //  printk(KERN_INFO "%s: CFG[0x04]   0x%lx\n", dev->name, regDW );
  pci_read_config_dword(dev->pci, 0x3c, &regDW);
  // printk(KERN_INFO "%s: CFG[0x3c]   0x%lx\n", dev->name, regDW );
  // MSI CAP     disable MSI
  pci_read_config_dword(dev->pci, 0x50, &regDW);
  regDW &= 0xfffeffff;
  pci_write_config_dword(dev->pci, 0x50, regDW);
  pci_read_config_dword(dev->pci, 0x50, &regDW);
  //  printk(KERN_INFO "%s: CFG[0x50]   0x%lx\n", dev->name, regDW );
  //  MSIX  CAP    disable
  pci_read_config_dword(dev->pci, 0xac, &regDW);
  regDW &= 0x7fffffff;
  pci_write_config_dword(dev->pci, 0xac, regDW);
  pci_read_config_dword(dev->pci, 0xac, &regDW);
  //  printk(KERN_INFO "%s: CFG[0xac]   0x%lx\n", dev->name, regDW );
  // PCIe Cap registers
  pci_read_config_dword(dev->pci, 0x70, &regDW);
  // printk(KERN_INFO "%s: CFG[0x70]   0x%lx\n", dev->name, regDW );
  pci_read_config_dword(dev->pci, 0x74, &regDW);
  // printk(KERN_INFO "%s: CFG[0x74]   0x%lx\n", dev->name, regDW );
  pci_read_config_dword(dev->pci, 0x78, &regDW);
  regDW &= 0xfffffe1f;
  regDW |= (0x8 << 5);  ///  8 - 128   ||  9 - 256  || A - 512
  pci_write_config_dword(dev->pci, 0x78, regDW);
  pci_read_config_dword(dev->pci, 0x78, &regDW);
  //  printk(KERN_INFO "%s: CFG[0x78]   0x%lx\n", dev->name, regDW );
  pci_read_config_dword(dev->pci, 0x730, &regDW);
  //  printk(KERN_INFO "%s: CFG[0x730]   0x%lx\n", dev->name, regDW );
  pci_read_config_dword(dev->pci, 0x734, &regDW);
  //  printk(KERN_INFO "%s: CFG[0x734]   0x%lx\n", dev->name, regDW );
  pci_read_config_dword(dev->pci, 0x738, &regDW);
  //  printk(KERN_INFO "%s: CFG[0x738]   0x%lx\n", dev->name, regDW );
  mdelay(20);
  tw68_reg_writel(dev, DMA_CHANNEL_ENABLE, 0);
  mdelay(50);
  tw68_reg_writel(dev, DMA_CMD, 0);
  tw68_reg_readl(dev, DMA_CHANNEL_ENABLE);
  tw68_reg_readl(dev, DMA_CMD);
  // Trasmit Posted FC credit Status
  tw68_reg_writel(dev, EP_REG_ADDR, 0x730);  //
  regDW = tw68_reg_readl(dev, EP_REG_DATA);
  // printk(KERN_INFO "%s: PCI_CFG[Posted 0x730]= 0x%lx\n", dev->name, regDW );
  // Trasnmit Non-Posted FC credit Status
  tw68_reg_writel(dev, EP_REG_ADDR, 0x734);  //
  regDW = tw68_reg_readl(dev, EP_REG_DATA);
  // printk(KERN_INFO "%s: PCI_CFG[Non-Posted 0x734]= 0x%lx\n", dev->name, regDW );
  // CPL FC credit Status
  tw68_reg_writel(dev, EP_REG_ADDR, 0x738);  //
  regDW = tw68_reg_readl(dev, EP_REG_DATA);
  // printk(KERN_INFO "%s: PCI_CFG[CPL 0x738]= 0x%lx\n", dev->name, regDW );
  regDW = tw68_reg_readl(dev, (SYS_SOFT_RST));
  /// printk(KERN_INFO "HWinit %s: SYS_SOFT_RST  0x%lx    \n", dev->name, regDW );
  /// regDW = tw_readl( SYS_SOFT_RST );
  /// printk(KERN_INFO "DMA %s: SYS_SOFT_RST  0x%lx    \n", dev->name, regDW );
  tw68_reg_writel(dev, SYS_SOFT_RST, 0x01);  //??? 01   09
  tw68_reg_writel(dev, SYS_SOFT_RST, 0x0F);
  mdelay(1);
  regDW = tw68_reg_readl(dev, SYS_SOFT_RST);
  /// printk(KERN_INFO " After software reset DMA %s: SYS_SOFT_RST  0x%lx    \n", dev->name, regDW );
  regDW = tw68_reg_readl(dev, PHASE_REF_CONFIG);
  /// printk(KERN_INFO "HWinit %s: PHASE_REF_CONFIG  0x%lx    \n", dev->name, regDW );
  regDW = 0x1518;
  tw68_reg_writel(dev, PHASE_REF_CONFIG, regDW & 0xFFFF);
  //  Allocate PB DMA pagetable  total 16K  filled with 0xFF
  videoDMA_pgtable_alloc(dev->pci, &dev->m_Page0);
  ChannelOffset = pgn = 128;
  pgn = 85;  // starting for 720 * 240 * 2
  m_nDropChannelNum = 0;
  m_bDropMasterOrSlave = 1;  // master
  m_bDropField = 0;
  m_bDropOddOrEven = 0;
  m_nVideoFormat = VIDEO_FORMAT_YUYV;
  m_bHorizontalDecimate = 0;
  m_bVerticalDecimate = 0;
  for (k = 0; k < MAX_NUM_SG_DMA; k++) {
    m_StartIdx = ChannelOffset * k;
    m_EndIdx = m_StartIdx + pgn;
    m_nCurVideoChannelNum = 0;                   // real-time video channel  starts 0
    m_nVideoFormat = 0;                          /// 0; ///VIDEO_FORMAT_UYVY;
    m_dwCHConfig = (m_StartIdx & 0x3FF) |        // 10 bits
                   ((m_EndIdx & 0x3FF) << 10) |  // 10 bits
                   ((m_nVideoFormat & 7) << 20) | ((m_bHorizontalDecimate & 1) << 23) |
                   ((m_bVerticalDecimate & 1) << 24) | ((m_nDropChannelNum & 3) << 25) |
                   ((m_bDropMasterOrSlave & 1) << 27) |  // 1 bit
                   ((m_bDropField & 1) << 28) | ((m_bDropOddOrEven & 1) << 29) | ((m_nCurVideoChannelNum & 3) << 30);
    tw68_reg_writel(dev, DMA_CH0_CONFIG + k, m_dwCHConfig);
    dwReg = tw68_reg_readl(dev, DMA_CH0_CONFIG + k);
    /// printk(" ********#### buffer_setup%d::  m_StartIdx 0X%x  0x%X  dwReg: 0x%X  m_dwCHConfig 0x%X  \n", k,
    /// m_StartIdx, pgn, m_dwCHConfig, dwReg );
    tw68_reg_writel(dev, VERTICAL_CTRL, 0x24);     // 0x26 will cause ch0 and ch1 have dma_error.  0x24
    tw68_reg_writel(dev, LOOP_CTRL, 0xA5);         // 0xfd   0xA5     /// 1005
    tw68_reg_writel(dev, DROP_FIELD_REG0 + k, 0);  /// m_nDropFiledReg
  }
  regDW = tw68_reg_readl(dev, (DMA_PAGE_TABLE0_ADDR));
  printk(KERN_INFO "DMA %s: DMA_PAGE_TABLE0_ADDR  0x%x\n", dev->name, regDW);
  regDW = tw68_reg_readl(dev, (DMA_PAGE_TABLE1_ADDR));
  printk(KERN_INFO "DMA %s: DMA_PAGE_TABLE1_ADDR  0x%x\n", dev->name, regDW);
  tw68_reg_writel(dev, DMA_PAGE_TABLE0_ADDR, dev->m_Page0.dma);                     // P DMA page table
  tw68_reg_writel(dev, DMA_PAGE_TABLE1_ADDR, dev->m_Page0.dma + (PAGE_SIZE << 1));  // B DMA page table
  regDW = tw68_reg_readl(dev, (DMA_PAGE_TABLE0_ADDR));
  printk(KERN_INFO "DMA %s: DMA_PAGE_TABLE0_ADDR  0x%x\n", dev->name, regDW);
  regDW = tw68_reg_readl(dev, (DMA_PAGE_TABLE1_ADDR));
  printk(KERN_INFO "DMA %s: DMA_PAGE_TABLE1_ADDR  0x%x\n", dev->name, regDW);
  tw68_reg_writel(dev, AVSRST, 0x3F);  // u32
  regDW = tw68_reg_readl(dev, AVSRST);
  printk(KERN_INFO "DMA %s: tw AVSRST _u8 %x :: 0x%x\n", dev->name, (AVSRST << 2), regDW);
  tw68_reg_writel(dev, DMA_CMD, 0);  // u32
  regDW = tw68_reg_readl(dev, DMA_CMD);
  printk(KERN_INFO "DMA %s: tw DMA_CMD _u8 %x :: 0x%x\n", dev->name, (DMA_CMD << 2), regDW);
  tw68_reg_writel(dev, DMA_CHANNEL_ENABLE, 0);
  regDW = tw68_reg_readl(dev, DMA_CHANNEL_ENABLE);
  printk(KERN_INFO "DMA %s: tw DMA_CHANNEL_ENABLE %x :: 0x%x\n", dev->name, DMA_CHANNEL_ENABLE, regDW);
  regDW = tw68_reg_readl(dev, DMA_CHANNEL_ENABLE);
  printk(KERN_INFO "DMA %s: tw DMA_CHANNEL_ENABLE %x :: 0x%x\n", dev->name, DMA_CHANNEL_ENABLE, regDW);
  // alternate settings: DMA_CHANNEL_TIMEOUT: 0x180c8F88, 0x140c8560,
  //                                          0x1F0c8b08, 0xF00F00, 140c8E08, 0x140c8D08
  tw68_reg_writel(dev, DMA_CHANNEL_TIMEOUT, 0x3EFF0FF0);  // longer timeout setting
  regDW = tw68_reg_readl(dev, DMA_CHANNEL_TIMEOUT);
  printk(KERN_INFO "DMA %s: tw DMA_CHANNEL_TIMEOUT %x :: 0x%x    \n", dev->name, DMA_CHANNEL_TIMEOUT, regDW);
  tw68_reg_writel(dev, DMA_INT_REF, 0x38000);  ///   2a000 2b000 2c000  3932e     0x3032e
  regDW = tw68_reg_readl(dev, DMA_INT_REF);
  printk(KERN_INFO "DMA %s: tw DMA_INT_REF %x :: 0x%x    \n", dev->name, DMA_INT_REF, regDW);
  tw68_reg_writel(dev, DMA_CONFIG, 0x00FF0004);
  regDW = tw68_reg_readl(dev, DMA_CONFIG);
  printk(KERN_INFO "DMA %s: tw DMA_CONFIG %x :: 0x%x    \n", dev->name, DMA_CONFIG, regDW);
  regDW = (0xFF << 16) | (VIDEO_GEN_PATTERNS << 8) | VIDEO_GEN;
  printk(KERN_INFO " set tw68 VIDEO_CTRL2 %x :: 0x%x    \n", VIDEO_CTRL2, regDW);
  tw68_reg_writel(dev, VIDEO_CTRL2, regDW);
  regDW = tw68_reg_readl(dev, VIDEO_CTRL2);
  printk(KERN_INFO "DMA %s: tw DMA_CONFIG %x :: 0x%x    \n", dev->name, VIDEO_CTRL2, regDW);
  // VDelay
  regDW = 0x014;
  tw68_reg_writel(dev, VDELAY0, regDW);
  tw68_reg_writel(dev, VDELAY1, regDW);
  tw68_reg_writel(dev, VDELAY2, regDW);
  tw68_reg_writel(dev, VDELAY3, regDW);
  // TW6869: 4 more decoders (at 0x100)
  tw68_reg_writel(dev, VDELAY0 + 0x100, regDW);
  tw68_reg_writel(dev, VDELAY1 + 0x100, regDW);
  tw68_reg_writel(dev, VDELAY2 + 0x100, regDW);
  tw68_reg_writel(dev, VDELAY3 + 0x100, regDW);
  // Show Blue background if no signal
  regDW = 0xe6;
  tw68_reg_writel(dev, MISC_CONTROL2, regDW);
  // 6869
  tw68_reg_writel(dev, MISC_CONTROL2 + 0x100, regDW);
  regDW = tw68_reg_readl(dev, VDELAY0);
  printk(KERN_INFO " read tw68 VDELAY0 %x :: 0x%x    \n", VDELAY0, regDW);
  regDW = tw68_reg_readl(dev, MISC_CONTROL2);
  printk(KERN_INFO " read tw68 MISC_CONTROL2 %x :: 0x%x    \n", MISC_CONTROL2, regDW);
  // Reset 2864s
  val1 = tw68_reg_readl(dev, CSR_REG);
  val1 &= 0x7FFF;
  tw68_reg_writel(dev, CSR_REG, val1);
  /// printk("2864 init CSR_REG 0x%x]= I2C 2864   val1:0X%x  %x\n", CSR_REG, val1 );
  mdelay(100);
  val1 |= 0x8002;  // Pull out from reset and enable I2S
  tw68_reg_writel(dev, CSR_REG, val1);
  /// printk("2864 init CSR_REG 0x%x]=  I2C 2864   val1:0X%x  %x\n", CSR_REG, val1 );
  addr = CLKOCTRL | 0x100;
  /// val0 = DeviceRead2864(dev, addr);
  val1 = 0x40 | (VIDEO_IN_MODE | (VIDEO_IN_MODE << 2));  // Out enable
  addr = NOVID | 0x100;
  val1 = 0x73;  // CHID with 656 Sync code, 656 output even no video input
  TW68_video_init1(dev);
  // decoder parameter setup
  TW68_video_init2(dev);           // set TV param
  dev->video_DMA_1st_started = 0;  // initial value for skipping startup DMA error
  dev->err_times = 0;              // DMA error counter
  dev->TCN = 16;
  for (k = 0; k < 8; k++) {
    dev->vc[k].videoDMA_run = 0;
  }
  return 0;
}

int vdev_init(struct TW68_dev *dev);

static void TW68_unregister_video(struct TW68_dev *dev) {
  int i;
  for (i = 0; i < 8; i++) {
    video_unregister_device(&dev->vc[i].vdev);
  }
  v4l2_device_unregister(&dev->v4l2_dev);
  return;
}

// probe function
static int TW68_probe(struct pci_dev *pci_dev, const struct pci_device_id *pci_id) {
  struct TW68_dev *dev;
  int err, err0;
  int i;
  struct pci_dev *pci2;
  int ivytown = 0;

#if defined(NTSC_STANDARD_NODETECT)
  printk("812/1012: NTSC video-standard init.\n");
#elif defined(PAL_STANDARD_NODETECT)
  printk("812/1012: PAL video-standard init.\n");
#endif
  printk("812/1012[vbuf2] driver %s %s\n", TW686X_DATE, TW686X_VERSION);
  if (TW68_devcount == TW68_MAXBOARDS) return -ENOMEM;
  dev = kzalloc(sizeof(*dev), GFP_KERNEL);
  if (NULL == dev) return -ENOMEM;
#ifdef AUDIO_BUILD
  dev->audio_channels = kcalloc(MAX_AUDIO_CH, sizeof(*dev->audio_channels), GFP_KERNEL);
  if (dev->audio_channels == NULL) {
    kfree(dev);
    return -ENOMEM;
  }
#endif
  // search for ivytown
  pci2 = pci_get_device(0x8086, 0x0e00, NULL);
  if (pci2 != NULL) {
    printk(KERN_INFO "ivytown detected\n");
    ivytown = 1;
    pci_dev_put(pci2);
  }
  pci2 = pci_get_device(0x8086, 0x2f00, NULL);
  if (pci2 != NULL) {
    printk(KERN_INFO "xeon E5/haswell detected\n");
    ivytown = 1;
    pci_dev_put(pci2);
  }
  mutex_init(&dev->lock);
  for (i = 0; i < 8; i++) {
    dev->vc[i].dev = dev;
    dev->vc[i].nId = i;
  }

  err = v4l2_device_register(&pci_dev->dev, &dev->v4l2_dev);
  if (err) {
    printk("failed v4l2 device\n");
    goto fail0;
  }
  /* pci init */
  dev->ivytown = ivytown;
  dev->pci = pci_dev;

#ifdef JETSON_TK1
  if (pci_assign_resource(pci_dev, 0)) {
    err = -EIO;
    printk("failed assign\n");
    goto fail1;
  }
#endif
  if (pci_enable_device(pci_dev)) {
    err = -EIO;
    printk("failed enable\n");
    goto fail1;
  }
  dev->nr = TW68_devcount;
  sprintf(dev->name, "TW%x[%d]", pci_dev->device, dev->nr);

  printk(KERN_INFO " %s TW68_devcount: %d \n", dev->name, TW68_devcount);

  /* pci quirks */
  if (pci_pci_problems) {
    if (pci_pci_problems & PCIPCI_TRITON) printk(KERN_INFO "%s: quirk: PCIPCI_TRITON\n", dev->name);
    if (pci_pci_problems & PCIPCI_NATOMA) printk(KERN_INFO "%s: quirk: PCIPCI_NATOMA\n", dev->name);
    if (pci_pci_problems & PCIPCI_VIAETBF) printk(KERN_INFO "%s: quirk: PCIPCI_VIAETBF\n", dev->name);
    if (pci_pci_problems & PCIPCI_VSFX) printk(KERN_INFO "%s: quirk: PCIPCI_VSFX\n", dev->name);
#ifdef PCIPCI_ALIMAGIK
    if (pci_pci_problems & PCIPCI_ALIMAGIK) {
      printk(KERN_INFO "%s: quirk: PCIPCI_ALIMAGIK -- latency fixup\n", dev->name);
      latency = 0x0A;
    }
#endif
    if (pci_pci_problems & (PCIPCI_FAIL | PCIAGP_FAIL)) {
      printk(KERN_INFO
             "%s: quirk: this driver and your "
             "chipset may not work together"
             " in overlay mode.\n",
             dev->name);
      if (!TW68_no_overlay) {
        printk(KERN_INFO
               "%s: quirk: overlay "
               "mode will be disabled.\n",
               dev->name);
        TW68_no_overlay = 1;
      } else {
        printk(KERN_INFO
               "%s: quirk: overlay "
               "mode will be forced. Use this"
               " option at your own risk.\n",
               dev->name);
      }
    }
  }

  /* print pci info */
  pci_read_config_byte(pci_dev, PCI_CLASS_REVISION, &dev->pci_rev);
  pci_read_config_byte(pci_dev, PCI_LATENCY_TIMER, &dev->pci_lat);

  printk(KERN_INFO
         "%s: found at %s, rev: %d, irq: %d, "
         "latency: %d, mmio: 0x%llx\n",
         dev->name, pci_name(pci_dev), dev->pci_rev, pci_dev->irq, dev->pci_lat,
         (unsigned long long)pci_resource_start(pci_dev, 0));

  pci_set_master(pci_dev);
  pci_set_drvdata(pci_dev, &(dev->v4l2_dev));
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
  if (!pci_dma_supported(pci_dev, DMA_BIT_MASK(32))) {
    printk("%s: Oops: no 32bit PCI DMA ???\n", dev->name);
    err = -EIO;
    goto fail1;
  } else
    printk("%s: Hi: 32bit PCI DMA supported \n", dev->name);
#endif

  dev->board = 1;
  printk(KERN_INFO "%s: subsystem: %04x:%04x, board: %s [card=%d,%s]\n", dev->name, pci_dev->subsystem_vendor,
         pci_dev->subsystem_device, TW68_boards[dev->board].name, dev->board,
         dev->autodetected ? "autodetected" : "insmod option");

  /* get mmio */
  if (!request_mem_region(pci_resource_start(pci_dev, 0), pci_resource_len(pci_dev, 0), dev->name)) {
    err = -EBUSY;
    printk(KERN_ERR "%s: can't get MMIO memory @ 0x%llx\n", dev->name,
           (unsigned long long)pci_resource_start(pci_dev, 0));
    goto fail1;
  }
  // no cache
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
  dev->lmmio = ioremap(pci_resource_start(pci_dev, 0), pci_resource_len(pci_dev, 0));
#else
  dev->lmmio = ioremap_nocache(pci_resource_start(pci_dev, 0), pci_resource_len(pci_dev, 0));
#endif
  dev->bmmio = (__u8 __iomem *)dev->lmmio;

  if (NULL == dev->lmmio) {
    err = -EIO;
    printk(KERN_ERR "%s: can't ioremap() MMIO memory\n", dev->name);
    goto fail2;
  }
  tw68_reg_writel(dev, 0xfb, 0xa2);
  tw68_reg_writel(dev, GPIO_REG, 0xffff0000);
  (void)tw68_reg_readl(dev, 0xfb);
  tw68_reg_writel(dev, GPIO_REG, 0xffff0000);
  msleep(30);
#if 0
	tmp = tw68_reg_readl(dev,GPIO_REG);
	//printk("tmp:  %x\n",tmp);
	tmp = ~tmp&0x07ed; tmp ^= 0x07ed; tmp &= 0x0ff7; 
	if (tmp!=0) {
		tmp|= 0x00ffff03;
		tw68_reg_writel(dev, CLKOPOL,tmp);
		tw68_reg_writel(dev, SYS_SOFT_RST, 0x01);
		tw68_reg_writel(dev, SYS_SOFT_RST, 0x0F);
		printk("s812: device error.\n");
		err = -ENODEV;
		goto fail3;
	}
#endif
  TW68_hwinit1(dev);
  err = request_irq(pci_dev->irq, TW68_irq, IRQF_SHARED, dev->name, dev);
  printk("TW68_initdev   %s: request IRQ %d\n", dev->name, pci_dev->irq);
  if (err < 0) {
    printk(KERN_ERR "%s: can't get IRQ %d\n", dev->name, pci_dev->irq);
    goto fail3;
  }

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
  init_timer(&dev->dma_delay_timer);
  dev->dma_delay_timer.function = tw68_dma_delay;
  dev->dma_delay_timer.data = (unsigned long)dev;
#else
  timer_setup(&dev->dma_delay_timer, tw68_dma_delay, 0);
#endif

  // tasklet_init(&dev->task, tw68_reset_channel, dev);
  v4l2_prio_init(&dev->prio);
  printk(KERN_ERR "Adding  TW686v_devlist %p\n", &TW686v_devlist);
  list_add_tail(&dev->devlist, &TW686v_devlist);
  /* register v4l devices */
  if (TW68_no_overlay > 0)
    printk(KERN_INFO "%s: Overlay support disabled.\n", dev->name);
  else
    printk(KERN_INFO "%s: Overlay supported %d .\n", dev->name, TW68_no_overlay);

  err0 = vdev_init(dev);
  if (err0 < 0) {
    printk(KERN_INFO "%s: can't register video device\n", dev->name);
    err = -ENODEV;
    goto fail4;
  }
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 10, 0)
#ifdef CONFIG_PROC_FS
  sprintf(dev->proc_name, "sray_812_%d", (dev->vc[0].vdev.minor / 8));
  dev->tw68_proc = proc_create_data(dev->proc_name, 0, 0, &tw68_procmem_fops, dev);
#endif
#endif

#ifdef AUDIO_BUILD
  TW68_audio_init(dev);
#endif
  TW68_devcount++;
  printk(KERN_INFO "%s: registered PCI device %d [v4l2]:%d  err: |%d| \n", dev->name, TW68_devcount,
         dev->vc[0].vdev.num, err0);

  return 0;
fail4:
  TW68_unregister_video(dev);
  free_irq(pci_dev->irq, dev);
fail3:
  if (dev->lmmio) {
    iounmap(dev->lmmio);
    dev->lmmio = NULL;
  }
fail2:
  release_mem_region(pci_resource_start(pci_dev, 0), pci_resource_len(pci_dev, 0));
fail1:
  v4l2_device_unregister(&dev->v4l2_dev);
fail0:
  kfree(dev);
  printk("probe failed 0x%x\n", err);
  return err;
}

static void TW68_remove(struct pci_dev *pci_dev) {
  int m, n = 0;

  struct v4l2_device *v4l2_dev = pci_get_drvdata(pci_dev);
  struct TW68_dev *dev = container_of_local(v4l2_dev, struct TW68_dev, v4l2_dev);
  printk(KERN_INFO "%s: Starting unregister video device %d\n", dev->name, dev->vc[0].vdev.num);
  printk(KERN_INFO " /* shutdown hardware */ dev 0x%p \n", dev);
  // tell board to stop sending any data, stop all channels
  tw68_reg_writel(dev, DMA_CMD, 0);
  tw68_reg_writel(dev, DMA_CHANNEL_ENABLE, 0);
  del_timer_sync(&dev->dma_delay_timer);
  msleep(50);
  // remove IRQ handler
  free_irq(pci_dev->irq, dev);
  // remove timer
  del_timer_sync(&dev->delay_resync);
  // release resources
#ifdef AUDIO_BUILD
  TW68_audio_free(dev);
#endif
  /* shutdown subsystems */
  /* unregister */
  mutex_lock(&TW68_devlist_lock);
  list_del(&dev->devlist);
  mutex_unlock(&TW68_devlist_lock);
  TW68_devcount--;
  if (dev->lmmio) {
    iounmap(dev->lmmio);
    dev->lmmio = NULL;
  }
  release_mem_region(pci_resource_start(pci_dev, 0), pci_resource_len(pci_dev, 0));
  pci_disable_device(pci_dev);
  TW68_pgtable_free(dev->pci, &dev->m_Page0);
  for (n = 0; n < 8; n++) {
    for (m = 0; m < 4; m++) {
      if (dev->BDbuf[n][m].cpu == NULL) continue;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
      pci_free_consistent(dev->pci, 800 * 300 * 2, dev->BDbuf[n][m].cpu, dev->BDbuf[n][m].dma_addr);
#else
      dma_free_coherent(&dev->pci->dev, 800 * 300 * 2, dev->BDbuf[n][m].cpu, dev->BDbuf[n][m].dma_addr);
#endif
      dev->BDbuf[n][m].cpu = NULL;
    }
  }
  TW68_unregister_video(dev);
  v4l2_device_put(&dev->v4l2_dev);
#ifdef CONFIG_PROC_FS
  if (dev->tw68_proc) {
    remove_proc_entry(dev->proc_name, NULL);
    dev->tw68_proc = NULL;
  }
#endif
  printk(KERN_INFO " unregistered v4l2_dev device  %d %d %d\n", TW68_VERSION_CODE >> 16,
         (TW68_VERSION_CODE >> 8) & 0xFF, TW68_VERSION_CODE & 0xFF);
#ifdef AUDIO_BUILD
  kfree(dev->audio_channels);
#endif
  kfree(dev);
}

/*
 * This function generates an entry in the file system's /proc directory.
 * The information provided here is useful for conveying status
 * and diagnostics info to the outside world.
 * Returns length of output string.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0)
#ifdef CONFIG_PROC_FS
static int tw68_proc_open(struct inode *inode, struct file *file) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
  return single_open(file, tw68_read_show, PDE_DATA(inode));
#else
  return single_open(file, tw68_read_show, pde_data(inode));
#endif
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 5, 0)
static const struct proc_ops tw68_procmem_fops = {
    .proc_open = tw68_proc_open, .proc_read = seq_read, .proc_lseek = seq_lseek, .proc_release = seq_release};
#else
static const struct file_operations tw68_procmem_fops = {
    .open = tw68_proc_open, .read = seq_read, .llseek = seq_lseek, .release = seq_release};
#endif

static int tw68_read_show(struct seq_file *m, void *v) {
  struct TW68_dev *dev;
  int j;

  if (m == NULL) return 0;
  dev = m->private;
  if (dev == NULL) return 0;

  seq_printf(m, "Astute Systems drvr debug stats[vbuf2]\n");
  seq_printf(m, "==================================\n");
  seq_printf(m, "dma timeout: %d\n", dev->vc[0].stat.dma_timeout);
  seq_printf(m, "(video errors may be caused by load, signal, or chipset issues)\n");
  for (j = 0; j < 8; j++) {
    seq_printf(m, "[%d] dma_err: %d field_off: %d vbuf_to: %d\n", j, dev->vc[j].stat.dma_err,
               dev->vc[j].stat.pb_mismatch, dev->vc[j].stat.v4l_timeout);
  }
  /* Insert a linefeed.*/
  seq_printf(m, "\n");
  /* Return char count of the generated string.*/
  return 0;
}
#endif
#endif

static struct pci_driver TW68_pci_driver = {
    .name = "sray812",
    .id_table = TW68_pci_tbl,
    .probe = TW68_probe,
    .remove = TW68_remove,
};

static int TW68_init(void) {
  INIT_LIST_HEAD(&TW686v_devlist);
  printk(KERN_INFO "TW68_: v4l2 driver version %d.%d.%d loaded\n", TW68_VERSION_CODE >> 16,
         (TW68_VERSION_CODE >> 8) & 0xFF, TW68_VERSION_CODE & 0xFF);
  return pci_register_driver(&TW68_pci_driver);
}

static void TW68_fini(void) {
  pci_unregister_driver(&TW68_pci_driver);
  printk(KERN_INFO "TW68_: v4l2 driver version %d.%d.%d removed\n", TW68_VERSION_CODE >> 16,
         (TW68_VERSION_CODE >> 8) & 0xFF, TW68_VERSION_CODE & 0xFF);
}

module_init(TW68_init);
module_exit(TW68_fini);
