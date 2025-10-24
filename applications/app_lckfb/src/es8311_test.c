#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <stdio.h>
#include <zephyr/logging/log.h>
#include <zephyr/audio/codec.h>
#include <zephyr/drivers/i2s.h>
#include <zephyr/drivers/gpio.h>
#include <string.h>
#include <math.h>

LOG_MODULE_REGISTER(app_main, CONFIG_APP_LOG_LEVEL);

/* Audio configuration parameters */
#define SAMPLE_FREQUENCY    16000  /* 16kHz sample rate */
#define SAMPLE_BIT_WIDTH    16     /* 16-bit samples */
#define BYTES_PER_SAMPLE    sizeof(int16_t)
#define NUMBER_OF_CHANNELS  2      /* Stereo output */
#define SAMPLES_PER_BLOCK   ((SAMPLE_FREQUENCY / 10) * NUMBER_OF_CHANNELS)  /* 100ms blocks */
#define BLOCK_SIZE          (BYTES_PER_SAMPLE * SAMPLES_PER_BLOCK)
#define BLOCK_COUNT         4
#define TIMEOUT_MS          1000

/* Memory slab for audio buffers */
K_MEM_SLAB_DEFINE_STATIC(tx_mem_slab, BLOCK_SIZE, BLOCK_COUNT, 4);

/* Generate a sine wave tone for testing audio output */
static void generate_tone(void *mem_block, uint32_t num_samples, uint32_t *phase, uint32_t frequency) {
    int16_t *samples = (int16_t *) mem_block;
    const int16_t amplitude = 8000; /* Amplitude (about 25% of max to avoid clipping) */

    for (uint32_t i = 0; i < num_samples; i += NUMBER_OF_CHANNELS) {
        /* Calculate sine wave sample */
        uint32_t angle = (*phase * frequency * 360) / SAMPLE_FREQUENCY;
        angle = angle % 360;

        /* Simple sine approximation (for demonstration) */
        double radians = (double) angle * 3.14159265359 / 180.0;
        int16_t sample = (int16_t) (amplitude * sin(radians));

        /* Write to both channels (stereo) */
        samples[i] = sample; /* Left channel */
        samples[i + 1] = sample; /* Right channel */

        (*phase)++;
        if (*phase >= SAMPLE_FREQUENCY) {
            *phase = 0;
        }
    }
}

int main(void) {
    int ret = 0;

    // Enable speaker en pin
    const struct gpio_dt_spec speaker_en = GPIO_DT_SPEC_GET_OR(DT_NODELABEL(speaker_en), gpios, {0});
    if (!gpio_is_ready_dt(&speaker_en)) {
        LOG_ERR("GPIO speaker_en not ready");
        return 0;
    }
    ret = gpio_pin_configure_dt(&speaker_en,GPIO_OUTPUT_ACTIVE);
    if (ret < 0) {
        LOG_ERR("GPIO speaker_en not configured");
        return 0;
    }

    printk("\n=== ES8311 Audio Output Test ===\n");

    /* Get the codec device */
    const struct device *codec = DEVICE_DT_GET_ONE(everest_es8311);
    if (!device_is_ready(codec)) {
        printk("ERROR: Codec device not ready\n");
        return -ENODEV;
    }
    printk("ES8311 codec ready\n");

    /* Get the I2S device */
    const struct device *i2s_dev = DEVICE_DT_GET(DT_ALIAS(i2s_txrx));
    if (!device_is_ready(i2s_dev)) {
        printk("ERROR: I2S device not ready\n");
        return -ENODEV;
    }
    printk("I2S device ready\n");

    /* Configure codec for audio playback (TX mode) */
    struct audio_codec_cfg codec_cfg = {
        .mclk_freq = 12288000, /* Typical MCLK for 16kHz (256 * 48kHz) */
        .dai_type = AUDIO_DAI_TYPE_I2S,
        .dai_route = AUDIO_ROUTE_PLAYBACK, /* Audio output mode */
        .dai_cfg = {
            .i2s = {
                .word_size = SAMPLE_BIT_WIDTH,
                .channels = NUMBER_OF_CHANNELS,
                .format = I2S_FMT_DATA_FORMAT_I2S,
                .options = I2S_OPT_FRAME_CLK_MASTER | I2S_OPT_BIT_CLK_MASTER,
                .frame_clk_freq = SAMPLE_FREQUENCY,
                .mem_slab = &tx_mem_slab,
                .block_size = BLOCK_SIZE,
                .timeout = TIMEOUT_MS,
            },
        },
    };

    ret = audio_codec_configure(codec, &codec_cfg);
    if (ret < 0) {
        printk("ERROR: Failed to configure codec: %d\n", ret);
        return ret;
    }
    printk("Codec configured: %dHz, %d-bit, %d channel(s)\n",
           SAMPLE_FREQUENCY, SAMPLE_BIT_WIDTH, NUMBER_OF_CHANNELS);

    /* Set output volume (adjust as needed, codec-specific values) */
    audio_property_value_t vol_val;
    vol_val.vol = 240; /* Set volume level (0-255, where 192=0dB, 240=+24dB) */
    ret = audio_codec_set_property(codec, AUDIO_PROPERTY_OUTPUT_VOLUME,
                                   AUDIO_CHANNEL_ALL, vol_val);
    if (ret == 0) {
        printk("Output volume set to %d\n", vol_val.vol);
    }

    /* Unmute output */
    audio_property_value_t mute_val;
    mute_val.mute = false;
    ret = audio_codec_set_property(codec, AUDIO_PROPERTY_OUTPUT_MUTE,
                                   AUDIO_CHANNEL_ALL, mute_val);
    if (ret == 0) {
        printk("Output unmuted\n");
    }

    /* Apply codec properties */
    audio_codec_apply_properties(codec);

    /* Configure I2S for TX (transmit/playback) */
    struct i2s_config i2s_cfg = {
        .word_size = SAMPLE_BIT_WIDTH,
        .channels = NUMBER_OF_CHANNELS,
        .format = I2S_FMT_DATA_FORMAT_I2S,
        .options = I2S_OPT_BIT_CLK_MASTER | I2S_OPT_FRAME_CLK_MASTER,
        .frame_clk_freq = SAMPLE_FREQUENCY,
        .mem_slab = &tx_mem_slab,
        .block_size = BLOCK_SIZE,
        .timeout = TIMEOUT_MS,
    };

    ret = i2s_configure(i2s_dev, I2S_DIR_TX, &i2s_cfg);
    if (ret < 0) {
        printk("ERROR: Failed to configure I2S TX: %d\n", ret);
        return ret;
    }
    printk("I2S TX configured\n");

    /* Tone generation parameters */
    uint32_t phase = 0;
    uint32_t tone_frequencies[] = {440, 880, 1320}; /* A4, A5, E6 notes */
    uint32_t current_freq_idx = 0;
    uint32_t blocks_per_tone = 30; /* 3 seconds per tone at 100ms blocks */
    uint32_t block_count = 0;

    /* Pre-fill the I2S TX queue with initial buffers before starting */
    printk("Pre-filling I2S TX buffers...\n");
    for (int i = 0; i < 2; i++) {
        void *mem_block;
        ret = k_mem_slab_alloc(&tx_mem_slab, &mem_block, K_MSEC(TIMEOUT_MS));
        if (ret < 0) {
            printk("ERROR: Failed to allocate initial buffer: %d\n", ret);
            return ret;
        }

        /* Generate initial tone data */
        generate_tone(mem_block, SAMPLES_PER_BLOCK, &phase, tone_frequencies[0]);

        /* Queue the buffer */
        ret = i2s_write(i2s_dev, mem_block, BLOCK_SIZE);
        if (ret < 0) {
            printk("ERROR: Failed to queue initial buffer: %d\n", ret);
            k_mem_slab_free(&tx_mem_slab, mem_block);
            return ret;
        }
    }
    printk("Initial buffers queued\n");

    /* Start I2S transmission */
    ret = i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_START);
    if (ret < 0) {
        printk("ERROR: Failed to start I2S TX: %d\n", ret);
        return ret;
    }
    printk("\n>>> Starting audio playback <<<\n");
    printk("Playing sine wave tones...\n\n");


    /* Main loop: Generate and send audio data */
    while (1) {
        void *mem_block;

        /* Allocate a memory block for audio data */
        ret = k_mem_slab_alloc(&tx_mem_slab, &mem_block, K_MSEC(TIMEOUT_MS));
        if (ret < 0) {
            printk("ERROR: Failed to allocate memory block: %d\n", ret);
            k_sleep(K_MSEC(100));
            continue;
        }

        /* Generate tone data */
        uint32_t current_freq = tone_frequencies[current_freq_idx];
        generate_tone(mem_block, SAMPLES_PER_BLOCK, &phase, current_freq);

        /* Write audio data to I2S */
        ret = i2s_write(i2s_dev, mem_block, BLOCK_SIZE);
        if (ret < 0) {
            printk("ERROR: Failed to write I2S data: %d\n", ret);
            k_mem_slab_free(&tx_mem_slab, mem_block);
            k_sleep(K_MSEC(100));
            continue;
        }

        block_count++;

        /* Change tone every N blocks */
        if (block_count % blocks_per_tone == 0) {
            current_freq_idx = (current_freq_idx + 1) % 3;
            phase = 0; /* Reset phase for clean transition */
            printk("Switching to %u Hz tone\n", tone_frequencies[current_freq_idx]);
        }

        /* Print status every 10 blocks (~1 second) */
        if (block_count % 10 == 0) {
            printk("Playing: %u Hz | Blocks sent: %u (%u.%u seconds)\n",
                   current_freq, block_count, block_count / 10, block_count % 10);
        }

        /* Optional: Add stop condition after some time or button press */
        // Can add stop condition here if needed
    }

    /* Stop I2S (unreachable in current loop, but shown for completeness) */
    i2s_trigger(i2s_dev, I2S_DIR_TX, I2S_TRIGGER_STOP);
    printk("Audio playback stopped\n");

    return 0;
}
