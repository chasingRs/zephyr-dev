# ES8311 Audio Codec Driver for Zephyr RTOS

## Overview

This driver provides support for the Everest Semiconductor ES8311 audio codec in Zephyr RTOS. The ES8311 is a low-power mono audio codec with integrated headphone amplifier, ADC, and DAC.

## Features

- I2C control interface
- Mono ADC and DAC
- Supports sample rates: 8kHz to 96kHz
- Word lengths: 16, 18, 20, 24, 32 bits
- DAI formats: I2S, Left-Justified, PCM/DSP modes A and B
- Provider (Master) and Consumer (Slave) modes
- MCLK frequencies up to 49.2 MHz
- Volume control for ADC and DAC
- Mute/unmute functionality
- Dynamic clock configuration

## Hardware Requirements

- I2C bus connection
- Optional MCLK input (can use BCLK in consumer mode)
- Power supply: typically 3.3V or 1.8V

## Device Tree Configuration

### Basic Configuration (Consumer Mode with BCLK-derived clock)

```dts
&i2c0 {
    status = "okay";
    
    es8311: es8311@18 {
        compatible = "everest,es8311";
        reg = <0x18>;  /* I2C address: 0x18 or 0x19 depending on hardware */
        status = "okay";
    };
};
```

### Configuration with MCLK Clock Source

```dts
&i2c1 {
    status = "okay";
    
    es8311: es8311@18 {
        compatible = "everest,es8311";
        reg = <0x18>;
        status = "okay";
        clocks = <&audio_clk>;
        clock-names = "mclk";
    };
};
```

### Note on Clock Polarity

Clock polarity (bit clock and frame clock inversion) is determined by the DAI format type:
- **I2S format**: Standard I2S timing
- **Left-Justified**: Left-aligned data
- **PCM/DSP Mode A**: Short frame sync
- **PCM/DSP Mode B**: Long frame sync (uses LRP bit for polarity)

Zephyr's I2S API does not provide separate clock inversion flags. Clock polarity is inherent to the selected DAI format.

## Kconfig Options

Enable the driver in your `prj.conf`:

```ini
CONFIG_I2C=y
CONFIG_AUDIO=y
CONFIG_AUDIO_CODEC=y
CONFIG_ES8311=y
CONFIG_AUDIO_CODEC_LOG_LEVEL_DBG=y  # Optional: for debug logging
```

## Usage Example

### Initialization and Configuration

```c
#include <zephyr/device.h>
#include <zephyr/audio/codec.h>

/* Get the codec device */
const struct device *codec = DEVICE_DT_GET_ONE(everest_es8311);

if (!device_is_ready(codec)) {
    printk("Codec device not ready\n");
    return -ENODEV;
}

/* Configure codec for I2S, 48kHz, 16-bit */
struct audio_codec_cfg codec_cfg = {
    .dai_type = AUDIO_DAI_TYPE_I2S,
    .dai_cfg = {
        .i2s = {
            .word_size = 16,
            .channels = 2,
            .format = I2S_FMT_DATA_FORMAT_I2S,
            .options = I2S_OPT_FRAME_CLK_MASTER |  /* Provider mode */
                       I2S_OPT_BIT_CLK_MASTER,
            .frame_clk_freq = 48000,
            .mem_slab = NULL,
            .block_size = 0,
            .timeout = 0,
        },
    },
};

int ret = audio_codec_configure(codec, &codec_cfg);
if (ret < 0) {
    printk("Failed to configure codec: %d\n", ret);
    return ret;
}
```

### Volume Control

```c
/* Set DAC (playback) volume: 0-255 (0.5dB steps) */
audio_property_value_t vol;
vol.vol = 200;  /* Near maximum volume */
audio_codec_set_property(codec, AUDIO_PROPERTY_OUTPUT_VOLUME, 
                         AUDIO_CHANNEL_ALL, vol);

/* Set ADC (capture) volume: 0-255 (0.5dB steps) */
vol.vol = 180;
audio_codec_set_property(codec, AUDIO_PROPERTY_INPUT_VOLUME,
                         AUDIO_CHANNEL_ALL, vol);

/* Apply properties */
audio_codec_apply_properties(codec);
```

### Mute/Unmute

```c
audio_property_value_t mute;

/* Mute output */
mute.mute = true;
audio_codec_set_property(codec, AUDIO_PROPERTY_OUTPUT_MUTE,
                         AUDIO_CHANNEL_ALL, mute);

/* Unmute output */
mute.mute = false;
audio_codec_set_property(codec, AUDIO_PROPERTY_OUTPUT_MUTE,
                         AUDIO_CHANNEL_ALL, mute);
```

### Start/Stop Playback

```c
/* Start playback (unmute DAC) */
audio_codec_start_output(codec);

/* Stop playback (mute DAC) */
audio_codec_stop_output(codec);
```

## Supported Sample Rates

The driver supports the following sample rates with appropriate MCLK frequencies:

- 8 kHz
- 11.025 kHz
- 16 kHz
- 22.05 kHz
- 32 kHz
- 44.1 kHz
- 48 kHz
- 64 kHz
- 88.2 kHz
- 96 kHz

## MCLK to Sample Rate Ratios

Supported MCLK/sample rate ratios:
- 32x, 64x, 128x, 256x, 384x, 512x

The driver automatically calculates appropriate dividers and multipliers based on the configured MCLK and desired sample rate.

## Clock Modes

### Provider Mode (Master)
The codec generates BCLK and LRCLK from MCLK. MCLK must be provided either from external source or configured via device tree.

### Consumer Mode (Slave)
The codec receives BCLK and LRCLK from external source. MCLK can be:
- Provided externally (recommended for best performance)
- Derived from BCLK (automatic fallback)

## Register Access

The driver uses I2C for all register access. Register read/write/update operations are provided internally with proper error handling.

## Power Management

The codec initializes in normal operation mode with:
- VMID configured for normal operation
- Minimal power-up time settings
- All necessary bias generators enabled

## Troubleshooting

### Codec Not Detected
- Check I2C address (typically 0x18 or 0x19)
- Verify I2C bus is properly configured and powered
- Check I2C pull-up resistors
- Verify codec power supply

### No Audio Output
- Check if codec is muted
- Verify volume settings (not set to 0)
- Ensure MCLK is present (if required)
- Check DAI format configuration matches I2S controller

### Clock Configuration Issues
- Verify MCLK frequency is within supported range (< 49.2 MHz)
- Check MCLK/sample rate ratio is supported
- In provider mode, ensure MCLK is configured
- In consumer mode without MCLK, verify BCLK frequency

## API Reference

### Configuration Functions
- `audio_codec_configure()` - Configure codec DAI and format
- `audio_codec_set_property()` - Set codec properties (volume, mute, etc.)
- `audio_codec_apply_properties()` - Apply property changes

### Control Functions
- `audio_codec_start_output()` - Start playback (unmute DAC)
- `audio_codec_stop_output()` - Stop playback (mute DAC)

## File Structure

```
drivers/audio/es8311/
├── CMakeLists.txt          # Build configuration
├── Kconfig                 # Kconfig options
├── es8311.c                # Main driver implementation
├── es8311.h                # Driver header
├── es8311_reg.h            # Register definitions
├── es8311.overlay          # Example device tree overlay
└── README.md               # This file

dts/bindings/audio/
└── everest,es8311.yaml     # Device tree binding
```

## License

SPDX-License-Identifier: Apache-2.0

## References

- ES8311 Datasheet
- Zephyr Audio Codec API Documentation
- Original Linux driver by Matteo Martelli

## Notes

This driver is adapted from the Linux ALSA driver for ES8311, modified to work with Zephyr's audio codec API and device model.

