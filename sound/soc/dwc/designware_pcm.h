/*
 * ALSA Platform Synopsys Audio Layer
 *
 * sound/soc/dwc/designware_pcm.h
 *
 * Copyright (C) 2016 Synopsys
 * Jose Abreu <joabreu@synopsys.com>, Tiago Duarte
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#ifndef __DW_PCM_H
#define __DW_PCM_H

int dw_pcm_platform_get(u32 *lsample, u32 *rsample, int bytes, int buf_size);
int dw_pcm_platform_register(struct platform_device *pdev);
int dw_pcm_platform_unregister(struct platform_device *pdev);

#endif /* __DW_PCM_H */