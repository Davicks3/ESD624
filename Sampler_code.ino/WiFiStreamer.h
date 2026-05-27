#ifndef WIFI_STREAMER_H
#define WIFI_STREAMER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

//WiFiStreamer, denne klasse håndtere alt Wifi og UDP for PCM1808
class WiFiStreamer {
private:
    //UDP socket brugt til at sende lyd pakker
    WiFiUDP udp;
    //IP adresse af raspberry pi
    IPAddress targetIP;
    //UDP port på raspberry pi
    int port;

    //UDP pakke størelse på 256 samples
    static const int UDP_SAMPLES = 256;

    //aloker buffer til udgående samples
    int32_t buffer[UDP_SAMPLES];
    //Nuværende index i buffer
    int index;
    //nuværende blok sekvens 
    uint32_t sequence;

    //Wifi AP oplysninger
    const char* ssid;
    const char* password;

public:
    WiFiStreamer(const char* ssid, const char* password, IPAddress ip, int port);

    void begin();
    void addSample(int32_t sample);
    void reset();
};

#endif