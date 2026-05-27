#include "PCM1808Sampler.h"
#include "WiFiStreamer.h"

//global streamer
extern WiFiStreamer streamer;

PCM1808Sampler::PCM1808Sampler(i2s_port_t i2s_port)
{
    port = i2s_port;
}

void PCM1808Sampler::begin()
{
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // Vi plukker self left channel ud senre
        .communication_format = I2S_COMM_FORMAT_I2S,
        .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count = 8,
        .dma_buf_len = 256,
        .use_apll = true,
        .tx_desc_auto_clear = false,
        .fixed_mclk = 24576000
    };

    i2s_pin_config_t pin_config;

    pin_config.bck_io_num   = 26;
    pin_config.ws_io_num    = 25;
    pin_config.data_out_num = I2S_PIN_NO_CHANGE;
    pin_config.data_in_num  = 21;
    pin_config.mck_io_num   = 3;

    i2s_driver_install(port, &i2s_config, 0, NULL);
    i2s_set_pin(port, &pin_config);

    delay(100); 

    i2s_zero_dma_buffer(port);

    Serial.println("PCM1808 ready");
}

void PCM1808Sampler::sample(float durationSec, OutputMode mode)
{
    size_t bytesRead;
    uint32_t totalSamples = durationSec * SAMPLE_RATE;

    //Volt buffer
    static float voltBuffer[BLOCK_SAMPLES * 10];
    int voltIndex = 0;

    // Reset streamer ved MODE_RAW
    if (mode == MODE_RAW)
        streamer.reset();

    //Flush første samples (ADC stabilisering)
    int32_t dummy[BLOCK_SAMPLES * 2];
    for (int i = 0; i < 5; i++)
    {
        i2s_read(port,
                 (void*)dummy,
                 sizeof(dummy),
                 &bytesRead,
                 portMAX_DELAY);
    }

    //MAIN LOOP
    for (uint32_t i = 0; i < totalSamples; i += BLOCK_SAMPLES)
    {
        i2s_read(port,
                 (void*)rxBuffer,
                 sizeof(int32_t) * BLOCK_SAMPLES * 2,
                 &bytesRead,
                 portMAX_DELAY);

        int samplesRead = bytesRead / sizeof(int32_t);

        // KUN LEFT CHANNEL (spring hver anden over)
        for (int j = 0; j < samplesRead; j += 2)
        {
            int32_t raw = rxBuffer[j] >> 8;

            if (mode == MODE_RAW)
            {
                streamer.addSample(raw);
            }
            else
            {
                float normalized = (float)raw / 8388608.0f;
                float voltage = normalized * ADC_FULL_SCALE_VOLT;

                if (voltIndex < (BLOCK_SAMPLES * 10))
                {
                    voltBuffer[voltIndex++] = voltage;
                }
            }
        }
    }

    //Hvis MODE_VOLT bare send til local serial monitor
    if (mode == MODE_VOLT)
    {
        for (int i = 0; i < voltIndex; i++)
        {
            Serial.println(voltBuffer[i], 6);
        }
    }

    Serial.println("Sampling done");
}