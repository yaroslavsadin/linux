/*
 * ALSA Platform Synopsys Audio Layer
 *
 * sound/soc/dwc/designware_pcm.c
 *
 * Copyright (C) 2016 Synopsys
 * Jose Abreu <joabreu@synopsys.com>, Tiago Duarte
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/dmaengine_pcm.h>

struct dw_pcm_binfo {
	struct snd_pcm_substream *stream;
	spinlock_t lock;
	unsigned char *dma_base;
	unsigned char *dma_pointer;
	unsigned int period_size_frames;
	unsigned int size;
	snd_pcm_uframes_t period_pointer;
	unsigned int total_periods;
	unsigned int current_period;
};

static const struct snd_pcm_hardware dw_pcm_playback_hw = {
	.info       = SNDRV_PCM_INFO_INTERLEAVED |
			SNDRV_PCM_INFO_MMAP |
			SNDRV_PCM_INFO_MMAP_VALID |
			SNDRV_PCM_INFO_BLOCK_TRANSFER,
	.rates      = SNDRV_PCM_RATE_32000 |
			SNDRV_PCM_RATE_44100 |
			SNDRV_PCM_RATE_48000,
	.rate_min   = 32000,
	.rate_max   = 48000,
	.formats    = SNDRV_PCM_FMTBIT_S16_LE,
	.channels_min = 2,
	.channels_max = 2,
	.buffer_bytes_max = 1152000,
	.period_bytes_min = 64000,
	.period_bytes_max = 576000,
	.periods_min      = 8,
	.periods_max      = 18,
};

static struct dw_pcm_binfo *dw_pcm_bi;

static int dw_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *rt = substream->runtime;
	struct dw_pcm_binfo *bi;

	snd_soc_set_runtime_hwparams(substream, &dw_pcm_playback_hw);
	snd_pcm_hw_constraint_integer(rt, SNDRV_PCM_HW_PARAM_PERIODS);

	bi = kzalloc(sizeof(*bi), GFP_KERNEL);
	if (!bi)
		return -ENOMEM;

	dw_pcm_bi = bi;
	spin_lock_init(&bi->lock);

	rt->hw.rate_min = 32000;
	rt->hw.rate_max = 48000;

	spin_lock(&bi->lock);
	bi->stream = substream;
	rt->private_data = bi;
	spin_unlock(&bi->lock);

	return 0;
}

static int dw_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *rt = substream->runtime;
	struct dw_pcm_binfo *bi = rt->private_data;

	kfree(bi);
	dw_pcm_bi = NULL;
	return 0;
}

static int dw_pcm_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *hw_params)
{
	struct snd_pcm_runtime *rt = substream->runtime;
	struct dw_pcm_binfo *bi = rt->private_data;
	int ret;

	ret = snd_pcm_lib_alloc_vmalloc_buffer(substream,
			params_buffer_bytes(hw_params));
	if (ret < 0)
		return ret;

	memset(rt->dma_area, 0, params_buffer_bytes(hw_params));

	spin_lock(&bi->lock);
	bi->dma_base = rt->dma_area;
	bi->dma_pointer = bi->dma_base;
	spin_unlock(&bi->lock);

	return 0;
}

static int dw_pcm_hw_free(struct snd_pcm_substream *substream)
{
	int ret;

	ret = snd_pcm_lib_free_vmalloc_buffer(substream);
	if (ret < 0)
		return ret;

	return 0;
}

static int dw_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *rt = substream->runtime;
	struct dw_pcm_binfo *bi = rt->private_data;
	u32 buffer_size_frames = 0;

	spin_lock(&bi->lock);
	bi->period_size_frames = bytes_to_frames(rt,
			snd_pcm_lib_period_bytes(substream));
	bi->size = snd_pcm_lib_buffer_bytes(substream);
	buffer_size_frames = bytes_to_frames(rt, bi->size);
	bi->total_periods = buffer_size_frames / bi->period_size_frames;
	bi->current_period = 1;
	spin_unlock(&bi->lock);

	if ((buffer_size_frames % bi->period_size_frames) != 0)
		return -EINVAL;

	return 0;
}

static int dw_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static snd_pcm_uframes_t dw_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *rt = substream->runtime;
	struct dw_pcm_binfo *bi = rt->private_data;

	return bi->period_pointer;
}

static struct snd_pcm_ops dw_pcm_capture_ops = {
	.open      = dw_pcm_open,
	.close     = dw_pcm_close,
	.ioctl     = snd_pcm_lib_ioctl,
	.hw_params = dw_pcm_hw_params,
	.hw_free   = dw_pcm_hw_free,
	.prepare   = dw_pcm_prepare,
	.trigger   = dw_pcm_trigger,
	.pointer   = dw_pcm_pointer,
	.page      = snd_pcm_lib_get_vmalloc_page,
	.mmap      = snd_pcm_lib_mmap_vmalloc,
};

static int dw_pcm_new(struct snd_soc_pcm_runtime *runtime)
{
	struct snd_pcm *pcm = runtime->pcm;
	int ret;

	ret =  snd_pcm_lib_preallocate_pages_for_all(pcm,
			SNDRV_DMA_TYPE_DEV,
			snd_dma_continuous_data(GFP_KERNEL),
			dw_pcm_playback_hw.buffer_bytes_max,
			dw_pcm_playback_hw.buffer_bytes_max);
	if (ret < 0)
		return ret;

	return 0;
}

static void dw_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static struct snd_soc_platform_driver dw_pcm_soc_platform = {
	.pcm_new  = dw_pcm_new,
	.pcm_free = dw_pcm_free,
	.ops = &dw_pcm_capture_ops,
};

int dw_pcm_platform_get(u32 *lsample, u32 *rsample, int bytes, int buf_size)
{
	struct snd_pcm_runtime *rt = NULL;
	struct dw_pcm_binfo *bi = dw_pcm_bi;
	int i;

	if (!bi)
		return -1;

	rt = bi->stream->runtime;

	spin_lock(&bi->lock);
	for (i = 0; i < buf_size; i++) {
		memcpy(&lsample[i], bi->dma_pointer, bytes);
		memset(bi->dma_pointer, 0, bytes);
		bi->dma_pointer += bytes;

		memcpy(&rsample[i], bi->dma_pointer, bytes);
		memset(bi->dma_pointer, 0, bytes);
		bi->dma_pointer += bytes;
	}
	bi->period_pointer += bytes_to_frames(rt, bytes * 2 * buf_size);

	if (bi->period_pointer >=
			(bi->period_size_frames * bi->current_period)) {
		bi->current_period++;
		if (bi->current_period > bi->total_periods) {
			bi->dma_pointer = bi->dma_base;
			bi->period_pointer = 0;
			bi->current_period = 1;
		}

		spin_unlock(&bi->lock);
		snd_pcm_period_elapsed(bi->stream);
		spin_lock(&bi->lock);
	}

	spin_unlock(&bi->lock);
	return 0;
}

int dw_pcm_platform_register(struct platform_device *pdev)
{
	return snd_soc_register_platform(&pdev->dev, &dw_pcm_soc_platform);
}

int dw_pcm_platform_unregister(struct platform_device *pdev)
{
	snd_soc_unregister_platform(&pdev->dev);
	return 0;
}

MODULE_AUTHOR("Jose Abreu <joabreu@synopsys.com>, Tiago Duarte");
MODULE_DESCRIPTION("Synopsys PCM module");
MODULE_LICENSE("GPL");