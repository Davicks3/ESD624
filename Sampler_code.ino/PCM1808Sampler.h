#ifndef PCM1808SAMPLER_H
#define PCM1808SAMPLER_H

#include <Arduino.h>
#include "driver/i2s.h"

//Set ADC samplerate 96 kHz
#define SAMPLE_RATE 96000

// ADC_FULL_SCALE_VOLT
#define ADC_FULL_SCALE_VOLT 1.41421356f

//BLOCK_SAMPLES
#define BLOCK_SAMPLES 256

//Enum indeholder de 2 modes
enum OutputMode {
    MODE_VOLT,
    MODE_RAW
};

//PCM1808Sampler class
class PCM1808Sampler {
private:
    //Hvilken I2S port bliver brugt
    i2s_port_t port;

    //PCM holder både left og right kannal derfor 2 samples pr. frame
    int32_t rxBuffer[BLOCK_SAMPLES * 2];
    int32_t txSilence[BLOCK_SAMPLES * 2];

public:
    //Gem I2S port nummber
    PCM1808Sampler(i2s_port_t i2s_port);

    //init I2S + PCM1808 pins + clocks
    void begin();

    void sample(float durationSec, OutputMode mode);
};

#endif
