/* SPDX-License-Identifier: Apache-2.0 */
/*
 * es8311.c -- ES8311 ALSA SoC Audio Driver for Zephyr
 *
 * Copyright (C) 2024
 * Adapted from Linux driver by Matteo Martelli <matteomartelli3@gmail.com>
 */

#define DT_DRV_COMPAT everest_es8311

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/audio/codec.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include "es8311_reg.h"
#include "es8311.h"

LOG_MODULE_REGISTER(es8311, CONFIG_AUDIO_CODEC_LOG_LEVEL);

/* Forward declarations */
static void es8311_start_output(const struct device *dev);

/* Device configuration structure */
struct es8311_config {
    struct i2c_dt_spec i2c;
    const struct device *mclk_dev;
    clock_control_subsys_t mclk_subsys;
    uint32_t mclk_freq;  /* Fixed MCLK frequency from DT */
};


#define DEV_CFG(dev) ((const struct es8311_config *)dev->config)
#define DEV_DATA(dev) ((struct es8311_data *)dev->data)

/* BCLK divider values */
static const uint32_t es8311_bclk_divs[] = {
    22, 24, 25, 30, 32, 33, 34, 36, 44, 48, 66, 72
};

/* MCLK coefficients table */
static const struct es8311_mclk_coeff es8311_mclk_coeffs[] = {
    {8000, 2048000, 1, 1, 1},
    {8000, 6144000, 3, 1, 1},
    {8000, 18432000, 3, 1, 3},
    {11025, 2822400, 1, 1, 1},
    {11025, 8467200, 3, 1, 1},
    {16000, 4096000, 1, 1, 1},
    {16000, 12288000, 3, 1, 1},
    {16000, 18432000, 3, 2, 3},
    {22050, 5644800, 1, 1, 1},
    {22050, 16934400, 3, 1, 1},
    {32000, 8192000, 1, 1, 1},
    {32000, 12288000, 3, 2, 1},
    {32000, 18432000, 3, 4, 3},
    {44100, 11289600, 1, 1, 1},
    {44100, 33868800, 3, 1, 1},
    {48000, 12288000, 1, 1, 1},
    {48000, 18432000, 3, 2, 1},
    {64000, 8192000, 1, 2, 1},
    {64000, 12288000, 3, 4, 1},
    {88200, 11289600, 1, 2, 1},
    {88200, 33868800, 3, 2, 1},
    {96000, 12288000, 1, 2, 1},
    {96000, 18432000, 3, 4, 1},
};

/* I2C register access functions */
static int es8311_write_reg(const struct device *dev, uint8_t reg, uint8_t val) {
    const struct es8311_config *config = DEV_CFG(dev);
    uint8_t buf[2] = {reg, val};

    return i2c_write_dt(&config->i2c, buf, sizeof(buf));
}

static int es8311_read_reg(const struct device *dev, uint8_t reg, uint8_t *val) {
    const struct es8311_config *config = DEV_CFG(dev);

    return i2c_write_read_dt(&config->i2c, &reg, 1, val, 1);
}

static int es8311_update_reg(const struct device *dev, uint8_t reg,
                             uint8_t mask, uint8_t val) {
    uint8_t old_val, new_val;
    int ret;

    ret = es8311_read_reg(dev, reg, &old_val);
    if (ret < 0) {
        return ret;
    }

    new_val = (old_val & ~mask) | (val & mask);
    if (new_val != old_val) {
        ret = es8311_write_reg(dev, reg, new_val);
    }

    return ret;
}

/* Reset codec */
static void es8311_reset(const struct device *dev, bool reset) {
    uint8_t mask = (uint8_t) (ES8311_RESET_CSM_ON | ES8311_RESET_RST_MASK);

    if (reset) {
        /* Enter reset mode */
        es8311_update_reg(dev, ES8311_RESET, mask, (uint8_t) ES8311_RESET_RST_MASK);
    } else {
        /* Leave reset mode */
        k_sleep(K_MSEC(5));
        es8311_update_reg(dev, ES8311_RESET, mask, ES8311_RESET_CSM_ON);
    }
}

/* Compare and adjust MCLK coefficient */
static int es8311_cmp_adj_mclk_coeff(uint32_t mclk_freq,
                                     const struct es8311_mclk_coeff *coeff,
                                     struct es8311_mclk_coeff *out_coeff) {
    uint32_t div = coeff->div;
    uint32_t mult = coeff->mult;
    bool match = false;

    if (coeff->mclk == mclk_freq) {
        match = true;
    } else if (mclk_freq % coeff->mclk == 0) {
        div = mclk_freq / coeff->mclk;
        div *= coeff->div;
        if (div <= 8) {
            match = true;
        }
    } else if (coeff->mclk % mclk_freq == 0) {
        mult = coeff->mclk / mclk_freq;
        if (mult == 2 || mult == 4 || mult == 8) {
            mult *= coeff->mult;
            if (mult <= 8) {
                match = true;
            }
        }
    }

    if (!match) {
        return -EINVAL;
    }

    if (out_coeff) {
        *out_coeff = *coeff;
        out_coeff->div = div;
        out_coeff->mult = mult;
    }

    return 0;
}

/* Get MCLK coefficient */
static int es8311_get_mclk_coeff(uint32_t mclk_freq, uint32_t rate,
                                 struct es8311_mclk_coeff *out_coeff) {
    for (size_t i = 0; i < ARRAY_SIZE(es8311_mclk_coeffs); i++) {
        const struct es8311_mclk_coeff *coeff = &es8311_mclk_coeffs[i];

        if (coeff->rate != rate) {
            continue;
        }

        int ret = es8311_cmp_adj_mclk_coeff(mclk_freq, coeff, out_coeff);
        if (ret == 0) {
            return 0;
        }
    }

    return -EINVAL;
}

/* Configure audio format */
static int es8311_configure_fmt(const struct device *dev,
                                const struct audio_codec_cfg *cfg) {
    struct es8311_data *data = DEV_DATA(dev);
    uint8_t wl;
    uint32_t rate = cfg->dai_cfg.i2s.frame_clk_freq;
    uint32_t word_size = cfg->dai_cfg.i2s.word_size;
    uint32_t mclk_freq = data->mclk_freq;
    uint8_t clkmgr;
    uint8_t mask;
    int ret;

    /* Configure word length */
    switch (word_size) {
        case 16:
            wl = ES8311_SDP_WL_16;
            break;
        case 18:
            wl = ES8311_SDP_WL_18;
            break;
        case 20:
            wl = ES8311_SDP_WL_20;
            break;
        case 24:
            wl = ES8311_SDP_WL_24;
            break;
        case 32:
            wl = ES8311_SDP_WL_32;
            break;
        default:
            LOG_ERR("Invalid word size: %d", word_size);
            return -EINVAL;
    }

    es8311_update_reg(dev, ES8311_SDP_IN, (uint8_t) ES8311_SDP_WL_MASK,
                      wl << ES8311_SDP_WL_SHIFT);
    es8311_update_reg(dev, ES8311_SDP_OUT, (uint8_t) ES8311_SDP_WL_MASK,
                      wl << ES8311_SDP_WL_SHIFT);

    /* Check MCLK frequency */
    if (mclk_freq > ES8311_MCLK_MAX_FREQ) {
        LOG_ERR("MCLK frequency %u too high", mclk_freq);
        return -EINVAL;
    }

    /* If MCLK not configured, use BCLK as internal MCLK (slave mode only) */
    if (!mclk_freq) {
        if (data->is_provider) {
            LOG_ERR("MCLK not configured, cannot run as provider");
            return -EINVAL;
        }
        LOG_DBG("MCLK not configured, use BCLK as internal MCLK");
        clkmgr = ES8311_CLKMGR1_MCLK_SEL;
        mclk_freq = rate * word_size * 2;
    } else {
        clkmgr = ES8311_CLKMGR1_MCLK_ON;
    }

    /* Get MCLK coefficient */
    struct es8311_mclk_coeff coeff;
    ret = es8311_get_mclk_coeff(mclk_freq, rate, &coeff);
    if (ret) {
        LOG_ERR("Unable to find MCLK coefficient for rate %u", rate);
        return ret;
    }

    /* Configure clock manager */
    mask = (uint8_t) (ES8311_CLKMGR1_MCLK_SEL | ES8311_CLKMGR1_MCLK_ON |
                      ES8311_CLKMGR1_BCLK_ON);
    clkmgr |= ES8311_CLKMGR1_BCLK_ON;
    es8311_update_reg(dev, ES8311_CLKMGR1, mask, clkmgr);

    /* Validate divisors */
    if (coeff.div == 0 || coeff.div > 8 ||
        coeff.div_adc_dac == 0 || coeff.div_adc_dac > 8) {
        LOG_ERR("Invalid coefficient divisors");
        return -EINVAL;
    }

    /* Configure multiplier */
    uint8_t mult;
    switch (coeff.mult) {
        case 1:
            mult = 0;
            break;
        case 2:
            mult = 1;
            break;
        case 4:
            mult = 2;
            break;
        case 8:
            mult = 3;
            break;
        default:
            LOG_ERR("Invalid multiplier: %u", coeff.mult);
            return -EINVAL;
    }

    /* Set pre-divider and multiplier */
    mask = (uint8_t) (ES8311_CLKMGR2_DIV_PRE_MASK | ES8311_CLKMGR2_MULT_PRE_MASK);
    clkmgr = ((coeff.div - 1) << ES8311_CLKMGR2_DIV_PRE_SHIFT) |
             (mult << ES8311_CLKMGR2_MULT_PRE_SHIFT);
    es8311_update_reg(dev, ES8311_CLKMGR2, mask, clkmgr);

    /* Set ADC/DAC divider */
    mask = (uint8_t) (ES8311_CLKMGR5_ADC_DIV_MASK | ES8311_CLKMGR5_DAC_DIV_MASK);
    clkmgr = ((coeff.div_adc_dac - 1) << ES8311_CLKMGR5_ADC_DIV_SHIFT) |
             ((coeff.div_adc_dac - 1) << ES8311_CLKMGR5_DAC_DIV_SHIFT);
    es8311_update_reg(dev, ES8311_CLKMGR5, mask, clkmgr);

    /* Configure LRCLK and BCLK dividers if provider mode */
    if (data->is_provider) {
        uint32_t div_lrclk = mclk_freq / rate;

        if (div_lrclk == 0 || div_lrclk > ES8311_CLKMGR_LRCLK_DIV_MAX + 1) {
            LOG_ERR("Invalid LRCLK divider: %u", div_lrclk);
            return -EINVAL;
        }

        /* Set LRCLK divider */
        mask = (uint8_t) ES8311_CLKMGR7_LRCLK_DIV_H_MASK;
        clkmgr = (uint8_t) ((div_lrclk - 1) >> 8);
        es8311_update_reg(dev, ES8311_CLKMGR7, mask, clkmgr);
        es8311_write_reg(dev, ES8311_CLKMGR8, (uint8_t) ((div_lrclk - 1) & 0xFF));

        /* Calculate BCLK divider */
        if (div_lrclk % (2 * word_size) != 0) {
            LOG_ERR("Unable to divide MCLK to generate BCLK");
            return -EINVAL;
        }

        uint32_t div_bclk = div_lrclk / (2 * word_size);

        mask = (uint8_t) ES8311_CLKMGR6_DIV_BCLK_MASK;
        if (div_bclk <= ES8311_BCLK_DIV_IDX_OFFSET) {
            clkmgr = (uint8_t) (div_bclk - 1);
        } else {
            size_t i;
            for (i = 0; i < ARRAY_SIZE(es8311_bclk_divs); i++) {
                if (es8311_bclk_divs[i] == div_bclk) {
                    break;
                }
            }
            if (i == ARRAY_SIZE(es8311_bclk_divs)) {
                LOG_ERR("BCLK divider %u not supported", div_bclk);
                return -EINVAL;
            }
            clkmgr = (uint8_t) (i + ES8311_BCLK_DIV_IDX_OFFSET);
        }
        es8311_update_reg(dev, ES8311_CLKMGR6, mask, clkmgr);
    }

    return 0;
}

/* Configure DAI format */
static int es8311_configure_dai(const struct device *dev,
                                const struct audio_codec_cfg *cfg) {
    struct es8311_data *data = DEV_DATA(dev);
    uint8_t sdp = 0;
    uint8_t mask;

    /* Configure provider/consumer mode */
    switch (cfg->dai_cfg.i2s.options & I2S_OPT_FRAME_CLK_MASTER) {
        case I2S_OPT_FRAME_CLK_MASTER:
            /* Provider mode (Master) */
            data->is_provider = true;
            es8311_update_reg(dev, ES8311_RESET, ES8311_RESET_MSC,
                              ES8311_RESET_MSC);
            break;
        default:
            /* Consumer mode (Slave) */
            data->is_provider = false;
            es8311_update_reg(dev, ES8311_RESET, ES8311_RESET_MSC, 0);
            break;
    }

    /* Configure DAI format */
    switch (cfg->dai_type) {
        case AUDIO_DAI_TYPE_I2S:
            sdp |= ES8311_SDP_FMT_I2S;
            break;
        case AUDIO_DAI_TYPE_LEFT_JUSTIFIED:
            sdp |= ES8311_SDP_FMT_LEFT_J;
            break;
        case AUDIO_DAI_TYPE_RIGHT_JUSTIFIED:
            LOG_ERR("Right justified mode not supported");
            return -ENOTSUP;
        case AUDIO_DAI_TYPE_PCMB:
            sdp |= ES8311_SDP_LRP;
            __fallthrough;
        case AUDIO_DAI_TYPE_PCMA:
            sdp |= ES8311_SDP_FMT_DSP;
            break;
        default:
            LOG_ERR("Unsupported DAI type");
            return -EINVAL;
    }

    /* Apply DAI format configuration */
    mask = (uint8_t) (ES8311_SDP_FMT_MASK | ES8311_SDP_LRP);
    es8311_update_reg(dev, ES8311_SDP_IN, mask, sdp);
    es8311_update_reg(dev, ES8311_SDP_OUT, mask, sdp);

    return es8311_configure_fmt(dev, cfg);
}

/* Configure codec */
static int es8311_configure(const struct device *dev,
                            struct audio_codec_cfg *cfg) {
    int ret;

    if (cfg == NULL) {
        LOG_ERR("Invalid configuration");
        return -EINVAL;
    }

    ret = es8311_configure_dai(dev, cfg);
    if (ret < 0) {
        LOG_ERR("Failed to configure DAI");
        return ret;
    }

    /* Start output if configured for playback */
    if (cfg->dai_route == AUDIO_ROUTE_PLAYBACK) {
        es8311_start_output(dev);
        LOG_INF("Codec configured for playback and started");
    } else {
        LOG_DBG("Codec configured successfully");
    }

    return 0;
}

/* Start codec operation */
static void es8311_start_output(const struct device *dev) {
    /* Enable DAC digital and analog clocks */
    uint8_t mask = BIT(ES8311_CLKMGR1_CLKDAC_ON_SHIFT) |
                   BIT(ES8311_CLKMGR1_ANACLKDAC_ON_SHIFT);
    es8311_update_reg(dev, ES8311_CLKMGR1, mask, mask);

    /* Power up DAC if not already */
    es8311_update_reg(dev, ES8311_SYS8, BIT(ES8311_SYS8_PDN_DAC_SHIFT), 0);

    /* Unmute DAC */
    es8311_update_reg(dev, ES8311_DAC1,
                      ES8311_DAC1_DAC_DSMMUTE | ES8311_DAC1_DAC_DEMMUTE, 0);

    LOG_INF("Output started - DAC enabled and unmuted");
}

/* Stop codec operation */
static void es8311_stop_output(const struct device *dev) {
    /* Mute DAC */
    uint8_t mask = ES8311_DAC1_DAC_DSMMUTE | ES8311_DAC1_DAC_DEMMUTE;
    es8311_update_reg(dev, ES8311_DAC1, mask, mask);
    LOG_DBG("Output stopped");
}

/* Mute/unmute output */
static int es8311_mute_output(const struct device *dev, bool mute) {
    uint8_t mask = ES8311_DAC1_DAC_DSMMUTE | ES8311_DAC1_DAC_DEMMUTE;
    uint8_t val = mute ? mask : 0;

    es8311_update_reg(dev, ES8311_DAC1, mask, val);
    LOG_DBG("Output %s", mute ? "muted" : "unmuted");
    return 0;
}

/* Set codec property */
static int es8311_set_property(const struct device *dev,
                               audio_property_t property,
                               audio_channel_t channel,
                               audio_property_value_t val) {
    int ret = 0;

    switch (property) {
        case AUDIO_PROPERTY_OUTPUT_VOLUME:
            /* Set DAC volume (0-255, 0.5dB steps) */
            ret = es8311_write_reg(dev, ES8311_DAC2, val.vol);
            LOG_DBG("Set DAC volume to %u", val.vol);
            break;

        case AUDIO_PROPERTY_INPUT_VOLUME:
            /* Set ADC volume (0-255, 0.5dB steps) */
            ret = es8311_write_reg(dev, ES8311_ADC3, val.vol);
            LOG_DBG("Set ADC volume to %u", val.vol);
            break;

        case AUDIO_PROPERTY_OUTPUT_MUTE:
            ret = es8311_mute_output(dev, val.mute);
            break;

        case AUDIO_PROPERTY_INPUT_MUTE:
            /* Mute ADC */
            ret = es8311_update_reg(dev, ES8311_SDP_OUT,
                                    BIT(ES8311_SDP_MUTE_SHIFT),
                                    val.mute ? BIT(ES8311_SDP_MUTE_SHIFT) : 0);
            LOG_DBG("Input %s", val.mute ? "muted" : "unmuted");
            break;

        default:
            LOG_WRN("Unsupported property: %d", property);
            ret = -ENOTSUP;
            break;
    }

    return ret;
}

/* Apply codec properties */
static int es8311_apply_properties(const struct device *dev) {
    /* Properties are applied immediately in set_property */
    return 0;
}

/* Codec API structure */
static const struct audio_codec_api es8311_api = {
    .configure = es8311_configure,
    .start_output = es8311_start_output,
    .stop_output = es8311_stop_output,
    .set_property = es8311_set_property,
    .apply_properties = es8311_apply_properties,
};

/* Initialize codec */
static int es8311_initialize(const struct device *dev) {
    const struct es8311_config *config = DEV_CFG(dev);
    struct es8311_data *data = DEV_DATA(dev);
    uint8_t chip_id1, chip_id2;
    int ret;

    /* Check I2C bus ready */
    if (!device_is_ready(config->i2c.bus)) {
        LOG_ERR("I2C bus not ready");
        return -ENODEV;
    }

    /* Read chip ID */
    ret = es8311_read_reg(dev, ES8311_CHIPID1, &chip_id1);
    if (ret < 0) {
        LOG_ERR("Failed to read chip ID1");
        return ret;
    }

    ret = es8311_read_reg(dev, ES8311_CHIPID2, &chip_id2);
    if (ret < 0) {
        LOG_ERR("Failed to read chip ID2");
        return ret;
    }

    if (chip_id1 != 0x83 || chip_id2 != 0x11) {
        LOG_ERR("Invalid chip ID: 0x%02x 0x%02x", chip_id1, chip_id2);
        return -ENODEV;
    }

    LOG_INF("ES8311 codec detected (ID: 0x%02x%02x)", chip_id1, chip_id2);

    /* Reset codec */
    es8311_reset(dev, true);
    es8311_reset(dev, false);

    /* Set minimal power-up time */
    es8311_write_reg(dev, ES8311_SYS1, 0);
    es8311_write_reg(dev, ES8311_SYS2, 0);

    /* Configure VMID for normal operation and power up analog */
    es8311_write_reg(dev, ES8311_SYS3, ES8311_SYS3_PDN_VMIDSEL_NORMAL_OPERATION);

    /* Power up DAC - Clear power down bit */
    es8311_update_reg(dev, ES8311_SYS8, BIT(ES8311_SYS8_PDN_DAC_SHIFT), 0);

    /* Enable headphone switch for output */
    es8311_update_reg(dev, ES8311_SYS9, BIT(ES8311_SYS9_HPSW_SHIFT),
                      BIT(ES8311_SYS9_HPSW_SHIFT));

    /* Configure GPIO for DAC to output routing */
    es8311_write_reg(dev, ES8311_GPIO, 0x00);

    /* Set default DAC volume (240 = +24dB, range 0-255 where 192=0dB) */
    es8311_write_reg(dev, ES8311_DAC2, 240);

    /* Enable DAC ramp rate for smooth transitions */
    es8311_update_reg(dev, ES8311_DAC6, (uint8_t)(0x0F << ES8311_DAC6_RAMPRATE_SHIFT),
                      (0x04 << ES8311_DAC6_RAMPRATE_SHIFT));

    /* Unmute DAC by default */
    es8311_update_reg(dev, ES8311_DAC1,
                      ES8311_DAC1_DAC_DSMMUTE | ES8311_DAC1_DAC_DEMMUTE, 0);

    /* Get MCLK frequency from clock controller if available */
    if (config->mclk_dev != NULL) {
        uint32_t rate;
        ret = clock_control_get_rate(config->mclk_dev,
                                     config->mclk_subsys, &rate);
        if (ret == 0) {
            data->mclk_freq = rate;
            LOG_INF("MCLK rate from clock controller: %u Hz", rate);
        } else {
            LOG_WRN("Failed to get MCLK rate from clock controller");
        }
    } else if (config->mclk_freq != 0) {
        /* Use fixed MCLK frequency from device tree */
        data->mclk_freq = config->mclk_freq;
        LOG_INF("MCLK rate from DT: %u Hz", data->mclk_freq);
    } else {
        LOG_INF("No MCLK configured, will use BCLK in consumer mode");
    }

    LOG_INF("ES8311 codec initialized successfully");
    return 0;
}

/* Device instantiation macro */
#define ES8311_DEVICE_INIT(inst)						\
										\
	static struct es8311_data es8311_data_##inst = {			\
		.mclk_freq = 0,							\
		.is_provider = false,						\
	};									\
										\
	static const struct es8311_config es8311_config_##inst = {		\
		.i2c = I2C_DT_SPEC_INST_GET(inst),				\
		.mclk_dev = COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, clocks),	\
			(DEVICE_DT_GET(DT_INST_CLOCKS_CTLR_BY_NAME(inst, mclk))), \
			(NULL)),						\
		.mclk_subsys = COND_CODE_1(DT_INST_NODE_HAS_PROP(inst, clocks), \
			((clock_control_subsys_t)DT_INST_CLOCKS_CELL_BY_NAME(inst, mclk, name)), \
			(NULL)),						\
		.mclk_freq = DT_INST_PROP_OR(inst, mclk_frequency, 0),		\
	};									\
										\
	DEVICE_DT_INST_DEFINE(inst,						\
			      es8311_initialize,				\
			      NULL,						\
			      &es8311_data_##inst,				\
			      &es8311_config_##inst,				\
			      POST_KERNEL,					\
			      CONFIG_AUDIO_CODEC_INIT_PRIORITY,			\
			      &es8311_api);

DT_INST_FOREACH_STATUS_OKAY(ES8311_DEVICE_INIT)