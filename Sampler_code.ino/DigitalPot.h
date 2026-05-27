#ifndef DIGITALPOT_H
#define DIGITALPOT_H

#include <Arduino.h>

class DigitalPot {
private:
    int pinINC;
    int pinUD;
    int pinCS;

    static const int MAX_STEP = 99;
    int currentStep;

    void pulseINC();
    void beginDS();
    void endDS();

public:
    DigitalPot(int inc, int ud, int cs);

    void begin();
    void calibrate();
    void setStep(int logicalStep);
    void sweep(int maxStep, int delayMs);
    void setGain(float gain);
};

#endif
