#include "WiFiStreamer.h"

//Wifistreamer, holder på WiFi relaterede variabler
WiFiStreamer::WiFiStreamer(const char* ssid, const char* password, IPAddress ip, int port)
{
    this->ssid = ssid;          //SSID for ESP32
    this->password = password;  //Adgangskode tol WIFI
    this->targetIP = ip;        //IP fra RPI4B  
    this->port = port;          //UDP port

    index = 0;                  //Nuværende index i sample buffer
    sequence = 0;               //Pakke sekvens
}

//Start wifi AP
void WiFiStreamer::begin()
{
    //Start Wifi hotspot(AP)
    WiFi.softAP(ssid, password);

    //brug auto-assign port
    udp.begin(0);  

    Serial.println("WiFi AP started");
}

//reset AP
void WiFiStreamer::reset()
{
    index = 0;      //Reset sample buffer
    sequence = 0;   //Reset package sekvens
}

//Tilføj et PCM sample til dens interne buffer, når buffer er fuld send en pakke
void WiFiStreamer::addSample(int32_t sample)
{
    //Gem samples i en buffer
    buffer[index++] = sample;

    //Hvis buffer er fuld, send buffer
    if (index >= UDP_SAMPLES)
    {
        //Start UDP pakke
        udp.beginPacket(targetIP, port);

        //send sequence først
        udp.write((uint8_t*)&sequence, sizeof(sequence));

        //send samples bagefter
        udp.write((uint8_t*)buffer, UDP_SAMPLES * sizeof(int32_t));

        //Slut UDP pakke
        udp.endPacket();

        //vigtigt for at undgå packet loss
        delayMicroseconds(300);

        //Nulstil buffer for index til næste pakke
        index = 0;

        //Forøg sequence number
        sequence++;
    }
}