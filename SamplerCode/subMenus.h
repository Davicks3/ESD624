#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
extern Adafruit_SSD1306 display;

void printCentered(const char* text, int y,int textSize) {
  int charWidth = 6 * textSize;  // 5px + 1px spacing
  int16_t x = (128 - (strlen(text) * charWidth)) / 2;

  display.setCursor(x, y);
  display.setTextColor(WHITE);
  display.setTextSize(textSize);
  display.print(text);
}

void printdBBar(){
  display.drawRoundRect(4,36,120,24,3,WHITE);
  
  display.setTextColor(WHITE);
  display.setTextSize(1);
  
  display.setCursor(0, 28);
  display.print("-3");

  display.setCursor(26, 28);
  display.print("0");

  display.setCursor(49, 28);
  display.print("3");

  display.setCursor(72, 28);
  display.print("6");

  display.setCursor(95, 28);
  display.print("9"); 

  display.setCursor(116, 28);
  display.print("12");
}


void BassMenu(){
  display.clearDisplay();
  printCentered("BASS", 2,3);
  printdBBar();
  display.display();
}

void MidMenu(){
  display.clearDisplay();
  printCentered("Mid", 2,3);
  printdBBar();
  display.display();
}

void TrebbleMenu(){
  display.clearDisplay();
  printCentered("Trebble", 2,3);
  printdBBar();
  display.display();
}

void FilterAmount(){
  display.clearDisplay();
  printCentered("FilterAmt", 2,2);
  display.drawRoundRect(4,36,120,24,3,WHITE);
  display.display();
}

void LowFreqDevi(){
  display.clearDisplay();
  printCentered("LowFreqDev", 2,2);
  display.drawRoundRect(4,36,120,24,3,WHITE);
  display.display();
}

void HighFreqDevi(){
  display.clearDisplay();
  printCentered("HighFreqDev", 2,2);
  display.drawRoundRect(4,36,120,24,3,WHITE);
  display.display();
}

void CrossFreqi(){
  display.clearDisplay();
  printCentered("CrossFreq", 2,2);
  display.drawRoundRect(4,36,120,24,3,WHITE);
  display.display();
}
