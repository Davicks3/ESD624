#include "DigitalPot.h"

DigitalPot::DigitalPot(int inc, int ud, int cs)
: pinINC(inc), pinUD(ud), pinCS(cs), currentStep(0)
{}

void DigitalPot::begin() {
    pinMode(pinINC, OUTPUT);
    pinMode(pinUD, OUTPUT);
    pinMode(pinCS, OUTPUT);

    digitalWrite(pinINC, HIGH);
    digitalWrite(pinCS, HIGH);
    digitalWrite(pinUD, LOW);

    delay(50);
}

void DigitalPot::pulseINC() {
    digitalWrite(pinINC, HIGH);
    delayMicroseconds(10);
    digitalWrite(pinINC, LOW);
    delayMicroseconds(10);
}

void DigitalPot::beginDS() {
    digitalWrite(pinCS, LOW);
    delayMicroseconds(5);
}

void DigitalPot::endDS() {
    digitalWrite(pinCS, HIGH);
    delay(5);
}

void DigitalPot::calibrate() {

    beginDS();
    digitalWrite(pinUD, LOW);
    delayMicroseconds(10);

    for (int i = 0; i < 120; i++)
        pulseINC();

    endDS();
    currentStep = 0;
}

void DigitalPot::setStep(int logicalStep) {

    logicalStep = constrain(logicalStep, 0, MAX_STEP);
    int targetStep = MAX_STEP - logicalStep;
    int diff = targetStep - currentStep;

    if (diff == 0) return;

    beginDS();

    digitalWrite(pinUD, diff > 0 ? HIGH : LOW);
    delayMicroseconds(10);

    for (int i = 0; i < abs(diff); i++)
        pulseINC();

    endDS();
    currentStep = targetStep;
}

void DigitalPot::setGain(float gain)
{
    const float Rg = 3600.0f;
    const float Rpot_total = 100000.0f;

    // minimum gain er 2
    if (gain < 2.0f) gain = 2.0f;

    // beregn pot-modstand
    float Rpot = Rg * (gain - 2.0f);

    if (Rpot > Rpot_total) Rpot = Rpot_total;

    // konverter til step (0..99 fysisk)
    int physicalStep = (Rpot / Rpot_total) * 99.0f;

    physicalStep = constrain(physicalStep, 0, 99);

    // her kalder vi setStep DIREKTE med physicalStep
    // fordi setStep allerede inverterer internt
    setStep(physicalStep);
}




