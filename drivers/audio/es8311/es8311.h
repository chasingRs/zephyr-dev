/* SPDX-License-Identifier: Apache-2.0 */
/*
 * es8311.h -- ES8311 Audio Codec Driver Header
 *
 * Copyright (C) 2024
 */

#ifndef _ES8311_H
#define _ES8311_H

#include <zephyr/device.h>

/* MCLK coefficient structure */
struct es8311_mclk_coeff {
	uint32_t rate;
	uint32_t mclk;
	uint32_t div;
	uint32_t mult;
	uint32_t div_adc_dac;
};

/* BCLK divider configuration */
#define ES8311_BCLK_DIV_IDX_OFFSET 20
#define ES8311_MCLK_MAX_FREQ 49200000

/* Driver data structure */
struct es8311_data {
	uint32_t mclk_freq;
	bool is_provider;
};

#endif /* _ES8311_H */

