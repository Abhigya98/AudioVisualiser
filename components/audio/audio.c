#include "audio.h"
#include "esp_adc/adc_oneshot.h"

#define MIC_CHANNEL ADC_CHANNEL_7 //gpio 35
#define MAX_SAMPLING 128

static adc_oneshot_unit_handle_t adc_handle;

void audio_init(void)
{
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT_1,
    };

    adc_oneshot_new_unit(&init_config, &adc_handle);

    adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };

    adc_oneshot_config_channel(adc_handle, MIC_CHANNEL, &config);
}

int audio_get_sample(void)
{
    int sample;
    adc_oneshot_read(adc_handle, MIC_CHANNEL, &sample);
    return sample;
}
int audio_get_level(void)
{
    int sum = 0;
    for(int i =0; i< MAX_SAMPLING; i++)
    {
        int sample;
        sample = audio_get_sample();

        if(sample <10)
            sample = 0; // ignore very low readings as silence

        //centering; sample = DC_offset + audio_signal + noise
        int centered = sample - 2048; // 12-bit ADC with mid-point at 2048

        if(centered < 0)
            centered = -centered; // Take absolute value for level calculation

        sum += centered; // Accumulate absolute values for 128 samples
    }

    int level = sum / MAX_SAMPLING;
    // Noise gate
    if (level < 800)
        level = 0;
    else
        level -= 800;

    // // Optional smoothing
    static int filtered = 0;
    filtered = (filtered * 3 + level) / 4;

    return filtered;
    // return level;

}