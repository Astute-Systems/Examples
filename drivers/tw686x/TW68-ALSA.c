/*
 * Based on the audio support from the tw6869 driver:
 * Copyright 2015 www.starterkit.ru <info@starterkit.ru>
 *
 * Based on:
 * Driver for Intersil|Techwell TW6869 based DVR cards
 * (c) 2011-12 liran <jli11@intersil.com> [Intersil|Techwell China]
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/kmod.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <linux/delay.h>

#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/control.h>
#include "TW68.h"
#include "TW68_defines.h"

#define AUDIO_CHANNEL_OFFSET 8

#ifdef AUDIO_BUILD

#define TW68_RECORD_VOLUME_MIN 0
#define TW68_RECORD_VOLUME_MAX 15
#define TW68_RECORD_VOLUME 0

static int tw68_snd_control_info(struct snd_kcontrol *kcontrol,
								 struct snd_ctl_elem_info *uinfo)
{
	switch (kcontrol->private_value)
	{
	default:
		return -EINVAL;
	case TW68_RECORD_VOLUME:
		uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
		uinfo->count = 1;
		uinfo->value.integer.min = TW68_RECORD_VOLUME_MIN;
		uinfo->value.integer.max = TW68_RECORD_VOLUME_MAX;
		uinfo->value.integer.step = 1;
		break;
	}
	return 0;
}

static int tw68_snd_control_get(struct snd_kcontrol *kcontrol,
								struct snd_ctl_elem_value *ucontrol)
{
	struct TW68_audio_channel *snd = snd_kcontrol_chip(kcontrol);
	int idx;
	int again;
	struct TW68_dev *chip = snd->dev;

	idx = snd->ch;
	switch (kcontrol->private_value)
	{
	case TW68_RECORD_VOLUME:
		if (idx < 4)
			again = tw_read(TW6864_R_AINGAIN1 + 4 * idx);
		else
			again = tw_read(TW6864_R_AINGAIN5 + 4 * (idx - 4));
		snd->current_record_volume = again;
		ucontrol->value.integer.value[0] = snd->current_record_volume & 0xff;
		// printk("%s.[%d] VOLUME: %d\n", __func__, idx, again);
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int tw68_snd_control_put(struct snd_kcontrol *kcontrol,
								struct snd_ctl_elem_value *ucontrol)
{
	struct TW68_audio_channel *snd = snd_kcontrol_chip(kcontrol);
	unsigned int newval = 0;
	int idx = snd->ch;
	int addr;
	struct TW68_dev *chip = snd->dev;

	switch (kcontrol->private_value)
	{
	case TW68_RECORD_VOLUME:
		if (ucontrol->value.integer.value[0] >= TW68_RECORD_VOLUME_MAX)
			newval |= snd->current_record_volume & 0xff;
		else if (ucontrol->value.integer.value[0] > 0)
			newval |= ucontrol->value.integer.value[0];
		// printk("%s:[%d] volume=%d\n", __func__, idx, newval);
		if (idx < 4)
			addr = TW6864_R_AINGAIN1 + 4 * idx;
		else
			addr = TW6864_R_AINGAIN5 + 4 * (idx - 4);
		tw_write(addr, newval);
		// printk("[%x]: %x\n", addr, newval);
		if (snd->current_record_volume == newval)
			return 0;
		snd->current_record_volume = newval;
		break;
	}
	return 1;
}

static struct snd_kcontrol_new tw68_snd_record_volume = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Capture Volume",
	.index = 0,
	.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
	.private_value = TW68_RECORD_VOLUME,
	.info = tw68_snd_control_info,
	.get = tw68_snd_control_get,
	.put = tw68_snd_control_put,
};

void TW68_audio_irq(struct TW68_dev *dev, u32 r,
					u32 pb_status)
{
	unsigned long flags;
	unsigned int ch, pb;
	unsigned long requests = (unsigned long)r;
	for_each_set_bit(ch, &requests, MAX_AUDIO_CH)
	{
		struct TW68_audio_channel *ac = &dev->audio_channels[ch];
		struct TW68_audio_buf *done = NULL;
		struct TW68_audio_buf *next = NULL;
		struct TW68_dma_desc *desc;

		pb = !!(pb_status & BIT(AUDIO_CHANNEL_OFFSET + ch));

		spin_lock_irqsave(&ac->lock, flags);

		/* Sanity check */
		if (!ac->ss || !ac->curr_bufs[0] || !ac->curr_bufs[1])
		{
			spin_unlock_irqrestore(&ac->lock, flags);
			continue;
		}

		if (!list_empty(&ac->buf_list))
		{
			next = list_first_entry(&ac->buf_list,
									struct TW68_audio_buf, list);
			list_move_tail(&next->list, &ac->buf_list);
			done = ac->curr_bufs[!pb];
			ac->curr_bufs[pb] = next;
		}
		spin_unlock_irqrestore(&ac->lock, flags);

		if (!done || !next)
			continue;
		/*
		 * Checking for a non-nil dma_desc[pb]->virt buffer is
		 * the same as checking for memcpy DMA mode.
		 */
		desc = &ac->dma_descs[pb];
		if (desc->virt)
		{
			memcpy(done->virt, desc->virt,
				   dev->period_size);
		}
		else
		{
			u32 reg = pb ? ADMA_B_ADDR[ch] : ADMA_P_ADDR[ch];
			tw68_reg_writel(dev, reg, next->dma);
		}
		ac->ptr = done->dma - ac->buf[0].dma;
		snd_pcm_period_elapsed(ac->ss);
	}
}

void TW68_disable_channel(struct TW68_dev *dev, unsigned int channel)
{
	u32 dma_en = reg_readl(DMA_CHANNEL_ENABLE);
	u32 dma_cmd = reg_readl(DMA_CMD);
	dma_en &= ~BIT(channel);
	dma_cmd &= ~BIT(channel);
	/* Stop DMA if no channels are enabled */
	if (!dma_en)
		dma_cmd = 0;
	tw68_reg_writel(dev, DMA_CHANNEL_ENABLE, dma_en);
	tw68_reg_writel(dev, DMA_CMD, dma_cmd);
	tw68_reg_readl(dev, DMA_CHANNEL_ENABLE);
	tw68_reg_readl(dev, DMA_CMD);
}

void TW68_enable_channel(struct TW68_dev *dev, unsigned int channel)
{
	u32 dma_en = reg_readl(DMA_CHANNEL_ENABLE);
	u32 dma_cmd = reg_readl(DMA_CMD);
	dma_en |= (1 << channel);
	tw68_reg_writel(dev, DMA_CHANNEL_ENABLE, dma_en);
	dma_cmd = (1 << 31) | (0xffff & dma_en);
	tw68_reg_writel(dev, DMA_CMD, dma_cmd);
	(void)tw68_reg_readl(dev, DMA_CMD);
	(void)tw68_reg_readl(dev, DMA_CHANNEL_ENABLE);
}

static int TW68_pcm_hw_params(struct snd_pcm_substream *ss,
							  struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(ss, params_buffer_bytes(hw_params));
}

static int TW68_pcm_hw_free(struct snd_pcm_substream *ss)
{
	return snd_pcm_lib_free_pages(ss);
}

/*
 * Audio parameters are global and shared among all
 * capture channels. The driver prevents changes to
 * the parameters if any audio channel is capturing.
 */
static const struct snd_pcm_hardware TW68_capture_hw = {
	.info = (SNDRV_PCM_INFO_MMAP |
			 SNDRV_PCM_INFO_INTERLEAVED |
			 SNDRV_PCM_INFO_BLOCK_TRANSFER |
			 SNDRV_PCM_INFO_MMAP_VALID),
	.formats = SNDRV_PCM_FMTBIT_S16_LE,
	.rates = SNDRV_PCM_RATE_8000_48000,
	.rate_min = 8000,
	.rate_max = 48000,
	.channels_min = 1,
	.channels_max = 1,
	.buffer_bytes_max = TW68_AUDIO_PAGE_MAX * AUDIO_DMA_SIZE_MAX,
	.period_bytes_min = AUDIO_DMA_SIZE_MIN,
	.period_bytes_max = AUDIO_DMA_SIZE_MAX,
	.periods_min = TW68_AUDIO_PERIODS_MIN,
	.periods_max = TW68_AUDIO_PERIODS_MAX,
};

static int TW68_pcm_open(struct snd_pcm_substream *ss)
{
	struct TW68_dev *dev = snd_pcm_substream_chip(ss);
	struct TW68_audio_channel *ac = &dev->audio_channels[ss->number];
	struct snd_pcm_runtime *rt = ss->runtime;
	int err;

	ac->ss = ss;
	rt->hw = TW68_capture_hw;

	err = snd_pcm_hw_constraint_integer(rt, SNDRV_PCM_HW_PARAM_PERIODS);
	if (err < 0)
		return err;

	return 0;
}

static int TW68_pcm_close(struct snd_pcm_substream *ss)
{
	struct TW68_dev *dev = snd_pcm_substream_chip(ss);
	struct TW68_audio_channel *ac = &dev->audio_channels[ss->number];

	ac->ss = NULL;
	return 0;
}

static int TW68_pcm_prepare(struct snd_pcm_substream *ss)
{
	struct TW68_dev *dev = snd_pcm_substream_chip(ss);
	struct TW68_audio_channel *ac = &dev->audio_channels[ss->number];
	struct snd_pcm_runtime *rt = ss->runtime;
	unsigned int period_size = snd_pcm_lib_period_bytes(ss);
	struct TW68_audio_buf *p_buf, *b_buf;
	unsigned long flags;
	int i;

	spin_lock_irqsave(&dev->slock, flags);
	/*
	 * Given the audio parameters are global (i.e. shared across
	 * DMA channels), we need to check new params are allowed.
	 */
	if (((dev->audio_rate != rt->rate) ||
		 (dev->period_size != period_size)) &&
		dev->audio_enabled)
		goto err_audio_busy;

	TW68_disable_channel(dev, AUDIO_CHANNEL_OFFSET + ac->ch);
	spin_unlock_irqrestore(&dev->slock, flags);

	if (dev->audio_rate != rt->rate)
	{
		u32 reg;
		dev->audio_rate = rt->rate;
		reg = ((125000000 / rt->rate) << 16) +
			  ((125000000 % rt->rate) << 16) / rt->rate;
		tw68_reg_writel(dev, AUDIO_CONTROL2, reg);
	}

	if (dev->period_size != period_size)
	{
		u32 reg;

		dev->period_size = period_size;
		reg = reg_readl(AUDIO_CTRL1);
		reg &= ~(AUDIO_DMA_SIZE_MASK << AUDIO_DMA_SIZE_SHIFT);
		reg |= period_size << AUDIO_DMA_SIZE_SHIFT;
		tw68_reg_writel(dev, AUDIO_CONTROL1, reg);
	}

	if (rt->periods < TW68_AUDIO_PERIODS_MIN ||
		rt->periods > TW68_AUDIO_PERIODS_MAX)
		return -EINVAL;

	spin_lock_irqsave(&ac->lock, flags);
	INIT_LIST_HEAD(&ac->buf_list);

	for (i = 0; i < rt->periods; i++)
	{
		ac->buf[i].dma = rt->dma_addr + period_size * i;
		ac->buf[i].virt = rt->dma_area + period_size * i;
		INIT_LIST_HEAD(&ac->buf[i].list);
		list_add_tail(&ac->buf[i].list, &ac->buf_list);
	}

	p_buf = list_first_entry(&ac->buf_list, struct TW68_audio_buf, list);
	list_move_tail(&p_buf->list, &ac->buf_list);

	b_buf = list_first_entry(&ac->buf_list, struct TW68_audio_buf, list);
	list_move_tail(&b_buf->list, &ac->buf_list);

	ac->curr_bufs[0] = p_buf;
	ac->curr_bufs[1] = b_buf;
	ac->ptr = 0;
#if 0
	if (dev->dma_mode != TW68_DMA_MODE_MEMCPY) {
	  printk("error, not supported\n");

		reg_write(dev, ADMA_P_ADDR[ac->ch], p_buf->dma);
		reg_write(dev, ADMA_B_ADDR[ac->ch], b_buf->dma);

	}
#endif
	spin_unlock_irqrestore(&ac->lock, flags);

	return 0;

err_audio_busy:
	spin_unlock_irqrestore(&dev->slock, flags);
	return -EBUSY;
}

static int TW68_pcm_trigger(struct snd_pcm_substream *ss, int cmd)
{
	struct TW68_dev *dev = snd_pcm_substream_chip(ss);
	struct TW68_audio_channel *ac = &dev->audio_channels[ss->number];
	unsigned long flags;
	int err = 0;
	switch (cmd)
	{
	case SNDRV_PCM_TRIGGER_START:
		if (ac->curr_bufs[0] && ac->curr_bufs[1])
		{
			spin_lock_irqsave(&dev->slock, flags);
			dev->audio_enabled = 1;
			TW68_enable_channel(dev,
								AUDIO_CHANNEL_OFFSET + ac->ch);
			spin_unlock_irqrestore(&dev->slock, flags);
			mod_timer(&dev->delay_resync, // dma_delay_timer
					  jiffies + msecs_to_jiffies(100));
		}
		else
		{
			err = -EIO;
		}
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		spin_lock_irqsave(&dev->slock, flags);
		dev->audio_enabled = 0;
		TW68_disable_channel(dev, AUDIO_CHANNEL_OFFSET + ac->ch);
		spin_unlock_irqrestore(&dev->slock, flags);

		spin_lock_irqsave(&ac->lock, flags);
		ac->curr_bufs[0] = NULL;
		ac->curr_bufs[1] = NULL;
		spin_unlock_irqrestore(&ac->lock, flags);
		break;
	default:
		err = -EINVAL;
	}
	return err;
}

static snd_pcm_uframes_t TW68_pcm_pointer(struct snd_pcm_substream *ss)
{
	struct TW68_dev *dev = snd_pcm_substream_chip(ss);
	struct TW68_audio_channel *ac = &dev->audio_channels[ss->number];

	return bytes_to_frames(ss->runtime, ac->ptr);
}

static struct snd_pcm_ops TW68_pcm_ops = {
	.open = TW68_pcm_open,
	.close = TW68_pcm_close,
	.ioctl = snd_pcm_lib_ioctl,
	.hw_params = TW68_pcm_hw_params,
	.hw_free = TW68_pcm_hw_free,
	.prepare = TW68_pcm_prepare,
	.trigger = TW68_pcm_trigger,
	.pointer = TW68_pcm_pointer,
};

static int TW68_snd_pcm_init(struct TW68_dev *dev, int channel)
{
	struct snd_card *card = dev->snd_card;
	struct snd_pcm *pcm;
	struct snd_pcm_substream *ss;
	unsigned int i;
	int err;
	// rv = snd_pcm_new(card, "TW6869 PCM", 0, 0, 1, &pcm);
	err = snd_pcm_new(card, card->driver, 0, 0, MAX_AUDIO_CH, &pcm);
	if (err < 0)
		return err;

	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &TW68_pcm_ops);
	snd_pcm_chip(pcm) = dev;
	pcm->info_flags = 0;
	strlcpy(pcm->name, "TW68 PCM", sizeof(pcm->name));

	for (i = 0, ss = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;
		 ss; ss = ss->next, i++)
		snprintf(ss->name, sizeof(ss->name), "vch%u audio", i);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
	snd_pcm_lib_preallocate_pages_for_all(pcm,
										  SNDRV_DMA_TYPE_DEV,
										  &dev->pci->dev,
										  TW68_AUDIO_PAGE_MAX * AUDIO_DMA_SIZE_MAX,
										  TW68_AUDIO_PAGE_MAX * AUDIO_DMA_SIZE_MAX);
	return 0;
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)) || (RHEL_MAJOR >= 7 && RHEL_MINOR >= 8)
	snd_pcm_lib_preallocate_pages_for_all(pcm,
										  SNDRV_DMA_TYPE_DEV,
										  snd_dma_pci_data(dev->pci),
										  TW68_AUDIO_PAGE_MAX * AUDIO_DMA_SIZE_MAX,
										  TW68_AUDIO_PAGE_MAX * AUDIO_DMA_SIZE_MAX);
	return 0;
#else
	return snd_pcm_lib_preallocate_pages_for_all(pcm,
												 SNDRV_DMA_TYPE_DEV,
												 snd_dma_pci_data(dev->pci),
												 TW68_AUDIO_PAGE_MAX * AUDIO_DMA_SIZE_MAX,
												 TW68_AUDIO_PAGE_MAX * AUDIO_DMA_SIZE_MAX);
#endif
}

static void TW68_audio_dma_free(struct TW68_dev *dev,
								struct TW68_audio_channel *ac)
{
	int pb;

	for (pb = 0; pb < 2; pb++)
	{
		if (!ac->dma_descs[pb].virt)
			continue;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
		pci_free_consistent(dev->pci, ac->dma_descs[pb].size,
							ac->dma_descs[pb].virt,
							ac->dma_descs[pb].phys);
#else
		dma_free_coherent(&dev->pci->dev, ac->dma_descs[pb].size,
						  ac->dma_descs[pb].virt,
						  ac->dma_descs[pb].phys);
#endif
		ac->dma_descs[pb].virt = NULL;
	}
}

static int TW68_audio_dma_alloc(struct TW68_dev *dev,
								struct TW68_audio_channel *ac)
{
	int pb;

	/*
	 * In the memcpy DMA mode we allocate a consistent buffer
	 * and use it for the DMA capture. Otherwise, DMA
	 * acts on the ALSA buffers as received in pcm_prepare.
	 */
#if 0
	if (dev->dma_mode != TW68_DMA_MODE_MEMCPY)
		return 0;
#endif
	for (pb = 0; pb < 2; pb++)
	{
		u32 reg = pb ? ADMA_B_ADDR[ac->ch] : ADMA_P_ADDR[ac->ch];
		void *virt;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 17, 0)
		virt = pci_alloc_consistent(dev->pci, AUDIO_DMA_SIZE_MAX,
									&ac->dma_descs[pb].phys);
#else
		virt = dma_alloc_coherent(&dev->pci->dev, AUDIO_DMA_SIZE_MAX,
								  &ac->dma_descs[pb].phys, GFP_KERNEL);
#endif
		if (!virt)
		{
			dev_err(&dev->pci->dev,
					"dma%d: unable to allocate audio DMA %s-buffer\n",
					ac->ch, pb ? "B" : "P");
			return -ENOMEM;
		}
		ac->dma_descs[pb].virt = virt;
		ac->dma_descs[pb].size = AUDIO_DMA_SIZE_MAX;
		tw68_reg_writel(dev, reg, ac->dma_descs[pb].phys);
	}
	return 0;
}

void TW68_audio_free(struct TW68_dev *dev)
{
	unsigned long flags;
	u32 dma_ch_mask;
	u32 dma_cmd;

	if (dev == NULL)
		return;
	spin_lock_irqsave(&dev->slock, flags);
	dma_cmd = reg_readl(DMA_CMD);
	dma_ch_mask = reg_readl(DMA_CHANNEL_ENABLE);
	tw68_reg_writel(dev, DMA_CMD, dma_cmd & ~0xff00);
	tw68_reg_writel(dev, DMA_CHANNEL_ENABLE, dma_ch_mask & ~0xff00);
	spin_unlock_irqrestore(&dev->slock, flags);

	if (!dev->snd_card)
		return;
	snd_card_free(dev->snd_card);
	dev->snd_card = NULL;
}

int TW68_audio_init(struct TW68_dev *dev)
{
	struct pci_dev *pci_dev = dev->pci;
	struct snd_card *card;
	int err, ch;
	struct TW68_audio_channel *ac;

	if (pci_dev == NULL)
		return 0;
	/* Enable external audio */
	tw68_reg_writel(dev, AUDIO_CONTROL1, BIT(0));

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0)) || (RHEL_MAJOR >= 7 && RHEL_MINOR >= 8)
	err = snd_card_new(&pci_dev->dev, SNDRV_DEFAULT_IDX1,
					   SNDRV_DEFAULT_STR1,
					   THIS_MODULE, 0, &card);
#else
	err = snd_card_create(SNDRV_DEFAULT_IDX1,
						  SNDRV_DEFAULT_STR1,
						  THIS_MODULE, 0, &card);
#endif
	if (err < 0)
		return err;

	dev->snd_card = card;
	strlcpy(card->driver, "TW68 PCM", sizeof(card->driver));
	strlcpy(card->shortname, "TW6869", sizeof(card->shortname));
	// strlcpy(card->longname, pci_name(pci_dev), sizeof(card->longname));
	sprintf(card->longname, "%s at 0x%p irq %d",
			dev->name, dev->bmmio, dev->pci->irq);
	snd_card_set_dev(card, &pci_dev->dev);
	for (ch = 0; ch < MAX_AUDIO_CH; ch++)
	{
		ac = &dev->audio_channels[ch];
		spin_lock_init(&ac->lock);
		ac->dev = dev;
		ac->ch = ch;
		err = TW68_audio_dma_alloc(dev, ac);
		if (err < 0)
			goto err_cleanup;
		err = snd_ctl_add(card, snd_ctl_new1(&tw68_snd_record_volume, ac));
		if (err < 0)
		{
			printk("failed to add record volume control\n");
		}
	}
	err = TW68_snd_pcm_init(dev, ch);
	if (err < 0)
		goto err_cleanup;
	err = snd_card_register(card);
	if (err)
		goto err_cleanup;

	return 0;
err_cleanup:
	for (ch = 0; ch < MAX_AUDIO_CH; ch++)
	{
		if (!dev->audio_channels[ch].dev)
			continue;
		TW68_audio_dma_free(dev, &dev->audio_channels[ch]);
	}
	if (dev->snd_card)
	{
		snd_card_free(dev->snd_card);
		dev->snd_card = NULL;
	}
	return err;
}
#endif
