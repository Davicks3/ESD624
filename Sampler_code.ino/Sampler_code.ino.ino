#include <WiFi.h>
#include <WiFiUdp.h>

#include "PCM1808Sampler.h"
#include "DigitalPot.h"
#include "WiFiStreamer.h"

//MODE VALG, skift til MODE_VOLT for lokal måling, MODE_RAW stream data til RPI4B
#define RUN_MODE MODE_RAW 

// PCM1808Sampler, håndterer I2S og PCM1808 ADC sampling
PCM1808Sampler sampler(I2S_NUM_0);

// Digital potentiometer controlling analog gain before ADC
DigitalPot pot(18, 17, 13);

// WiFiStreamer, håndterer WIFI AP og sende UDP pakker
WiFiStreamer streamer("ESP32_Sampler", "12345678", IPAddress(192,168,4,2), 1234);

//RPI4B sender start til at starte sampling
WiFiUDP commandUdp;
const int commandPort = 1235;

// Setup
void setup()
{
    Serial.begin(115200);
    delay(2000);

    Serial.println("Booting...");

    if (RUN_MODE == MODE_RAW)
    {
        //Start Wifi og UDP streamer 
        streamer.begin();
        //Lyt efter start fra RPI4B
        commandUdp.begin(commandPort);
        delay(200);
    }

    //init PCM1808 og I2S
    sampler.begin();

    //Init digital pot
    pot.begin();
    pot.calibrate();
    pot.setGain(29.0);
    delay(500);

    //Check mode: if MODE_RAW wait for start command and if MODE_VOLT print to serial monitor
    if (RUN_MODE == MODE_RAW)
    {
        Serial.println("RAW mode - waiting for START...");
    }
    else
    {
        Serial.println("VOLT mode - sampling...");
        sampler.sample(1.0, MODE_VOLT);
        Serial.println("Done");
    }
}

// Loop
void loop()
{
    //In MODE_Volt nothing happens due to printing a sample to serial monitor and then exiting
    if (RUN_MODE != MODE_RAW)
        return;

    //Check if UDP command package has been recived
    int packetSize = commandUdp.parsePacket();

    if (!packetSize)
        return; //No command do nothing

    char incoming[16];

    //Read incomming  string
    int len = commandUdp.read(incoming, sizeof(incoming) - 1);

    //Check if string is empty 
    if (len <= 0)
        return;

    //Null terminate
    incoming[len] = '\0';

    //If incomming is "START", begin streaming audio 
    if (strcmp(incoming, "START") == 0)
    {
        Serial.println("START received");

        streamer.reset();

        delay(50);  // giv Pi tid

        //Sample i 5 sekunder
        sampler.sample(5.0, MODE_RAW);

        Serial.println("Sampling done");
    }
}