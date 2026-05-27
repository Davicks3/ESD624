#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Menu.h>
#include <subMenus.h>

// ----------------------
// Pin Definitions
// ----------------------
#define ENC_A      2     // Encoder A pin
#define ENC_B      3     // Encoder B pin
#define ENC_BTN    1     // Encoder push button
#define SWITCH_PIN 5     // Toggle switch
#define RGB_red 8        // RGB red pin
#define RGB_blue 9       // RGB blue pin
#define RGB_green 20     // RGB green pin

// ----------------------
// OLED SPI Pins
// ----------------------
#define OLED_CLK    4
#define OLED_MOSI   6     //(SDA)
#define OLED_DC     7
#define OLED_RESET  10
#define OLED_CS     21

// ----------------------
// OLED Setup (SPI)
// ----------------------
Adafruit_SSD1306 display(128, 64, &SPI, OLED_DC, OLED_RESET, OLED_CS);

// ----------------------
// VarToSend
// ----------------------
bool SwitchState = false;
int BassGain = 0;
int MidGain = 0;
int TebbleGain = 0;
int FilterAmt = 50;
float LowFreqDev = 1.5;
float HighFreqDev = 1.5;
int CrossFreq = 300;

// ----------------------
// Struct for all var for all states
// ----------------------
struct LastState {
  int BassGain;
  int MidGain;
  int TebbleGain;
  int FilterAmt;
  float LowFreqDev;
  float HighFreqDev;
  int CrossFreq;
  bool SwitchState;
};

//Set all states to 0 ensure values
LastState lastSent = {0,0,0,0,0,0,0,false};

// ----------------------
// enum for all menus
// ----------------------
enum MenuItem {
    MENU_BASS,
    MENU_MID,
    MENU_TREBBLE,
    MENU_FILTERAMOUNT,
    MENU_LOWFREQDEV,
    MENU_HIGHFREQDEV,
    MENU_CROSSFREQ,
    MENU_STATS,
    MENU_PLACEHOLDER,
    MENU_MAIN
};

//Set the current menu to first menu en enum
MenuItem currentMenuItem = MENU_BASS;


// Array of all bitmaps (only one now easy to add more later)
const int myBitmapallArray_LEN = 1;
const unsigned char* myBitmapallArray[1] = {
  Menu_Bitmap
};


// ----------------------
// Encoder Variables
// ----------------------
volatile long rawSteps = 0;   // raw encoder movement
volatile int encoderPos = 0;  // encoder position
int encoderAccum = 0;   // counts raw encoder steps
int lastEncoded = 0;

// ----------------------
// Encoder Interrupt Handler
// ----------------------
void IRAM_ATTR readEncoder() {  
  //read encoder pins state
  int MSB = digitalRead(ENC_A); // Most Significant Bit (A channel)
  int LSB = digitalRead(ENC_B); // Most Significant Bit (B channel)

  //Combine the MSB and LSB to a singe variable (2 bit number)
  int encoded = (MSB << 1) | LSB;

  //Combine previus state and current state into a 4 bit number (Allowing detecting of quadrature transitions)
  int sum = (lastEncoded << 2) | encoded;

  // These patterns represent a forward rotation step
  if (sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011)
    rawSteps++;

  // These patterns represent a backward rotation step
  if (sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000)
    rawSteps--;

  // Store current state for next interrupt
  lastEncoded = encoded;
}

void IRAM_ATTR Switch() {
  if(digitalRead(SWITCH_PIN)==HIGH)
    SwitchState = true;
  else
    SwitchState = false;
}

// ----------------------
// Setup, defining pinModes, itterups and initialising display.
// ----------------------
void setup() {
  //Start serial moniter (This is used to send updates for state variables)
  Serial.begin(9600);

  // Start SPI with your custom pins
  SPI.begin(OLED_CLK, -1, OLED_MOSI, OLED_CS);

  // Encoder pins
  pinMode(ENC_A, INPUT_PULLUP);
  pinMode(ENC_B, INPUT_PULLUP);
  pinMode(ENC_BTN, INPUT_PULLUP);

  //RGB led
  pinMode(RGB_red, OUTPUT);
  pinMode(RGB_green, OUTPUT);
  pinMode(RGB_blue, OUTPUT);

  // Switch pin
  pinMode(SWITCH_PIN, INPUT);

  // Attach interrupts
  attachInterrupt(digitalPinToInterrupt(ENC_A), readEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_B), readEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(SWITCH_PIN), Switch, CHANGE);

  // OLED init (SPI)
  if (!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println("OLED not found");
    while (true);
  }

  // Draw first frame
  display.clearDisplay();
  display.clearDisplay();
  display.drawBitmap(0, 11+(22*encoderPos), Menu_Bitmap, 128, 220, SSD1306_WHITE);
  display.fillRect(24, 42, 80, 2, SSD1306_WHITE);
  display.fillRect(22, 22, 80, 2, SSD1306_WHITE);
  display.display();
}


// ----------------------
// mainMenu, this handles encoder, and bitmap position for main menu
// ----------------------
void mainMenu(){
  static long lastRaw = 0;          // Stores previous raw encoder count for comparison
  long diff = rawSteps - lastRaw;   // How much the encoder has moved since last check

  // If encoder moved forward enough (4 raw steps = 1 detent)
  if (diff >= 4) { 
      lastRaw = rawSteps;

      // Clamp for current menuitem, cannot move below last element. 
      if (currentMenuItem < 8)
          currentMenuItem = (MenuItem)(currentMenuItem + 1);
  }
  // If encoder moved backward enough (4 raw steps = 1 detent)
  else if (diff <= -4) {
      lastRaw = rawSteps;

      // Clamp for current menuitem, cannot move above first element. 
      if (currentMenuItem > 0)
          currentMenuItem = (MenuItem)(currentMenuItem - 1);
  }

  // Convert menu index to bitmap offset (negative because menu scrolls opposite direction)
  int encoderPos = -(int)currentMenuItem;

  // Redraw
  display.clearDisplay();
  display.drawBitmap(0, 11 + (22 * encoderPos), Menu_Bitmap, 128, 220, SSD1306_WHITE);
  display.fillRect(24, 42, 80, 2, SSD1306_WHITE);
  display.fillRect(22, 22, 80, 2, SSD1306_WHITE);
  display.display();    //send updated frame to display
}


// ----------------------
// handleEncoder, handles the encoder for all submenus 
// ----------------------
float handleEncoder(float current, float minVal, float maxVal, float step) {
    static long lastRaw = 0;          // Stores previous raw encoder count for comparison
    long diff = rawSteps - lastRaw;   // How much the encoder has moved since last check

    // If encoder moved forward enough (4 raw steps = 1 detent)
    if (diff >= 4) {
        lastRaw = rawSteps;
        current += step;
        // Clamp to maximum allowed value
        if (current > maxVal)
            current = maxVal;
    }
    // If encoder moved backward enough (4 raw steps = 1 detent)
    else if (diff <= -4) {
        lastRaw = rawSteps;
        current -= step;
        // Clamp to minimum allowed value
        if (current < minVal)
            current = minVal;
    }

    return current;
}

// ----------------------
// randomBool, for random true or false to randomly turn on and off RGB led
// ----------------------
bool randomBool() {
    return esp_random() & 1;   // returns true or false randomly
}

// ----------------------
// stateChanged, tracks if a vaiable is changed, this is to check if an update should be sendt.
// ----------------------
bool stateChanged() {
  return (
    lastSent.BassGain    != BassGain    ||
    lastSent.MidGain     != MidGain     ||
    lastSent.TebbleGain  != TebbleGain  ||
    lastSent.FilterAmt   != FilterAmt   ||
    lastSent.LowFreqDev  != LowFreqDev  ||
    lastSent.HighFreqDev != HighFreqDev ||
    lastSent.CrossFreq   != CrossFreq   ||
    lastSent.SwitchState != SwitchState
  );
}


// ----------------------
// PrintData, sends data by the serial monitor (USB cable)
// ----------------------
void PrintData(){
  // Print all parameter values in one line, space‑separated
  Serial.print(BassGain); Serial.print(" ");
  Serial.print(MidGain); Serial.print(" ");
  Serial.print(TebbleGain); Serial.print(" ");
  Serial.print(FilterAmt); Serial.print(" ");
  Serial.print(LowFreqDev); Serial.print(" ");
  Serial.print(HighFreqDev); Serial.print(" ");
  Serial.print(CrossFreq); Serial.print(" ");
  Serial.println(SwitchState);

  // Update last sent values so stateChanged() can detect future changes
  lastSent.BassGain    = BassGain;
  lastSent.MidGain     = MidGain;
  lastSent.TebbleGain  = TebbleGain;
  lastSent.FilterAmt   = FilterAmt;
  lastSent.LowFreqDev  = LowFreqDev;
  lastSent.HighFreqDev = HighFreqDev;
  lastSent.CrossFreq   = CrossFreq;
  lastSent.SwitchState = SwitchState;
}

// ----------------------
// loop, main loop.
// ----------------------
void loop() {
  if (stateChanged()) {
    PrintData();
  }
  
  // -------------------------
  // RGB LED behavior section
  // -------------------------
  if(SwitchState == true){
    static int T1 = millis(); // Timer for random LED flashing

    // When BassGain is exactly 12, flash RGB randomly every 100ms
    if(BassGain == 12 && millis()-T1>100){
      digitalWrite(RGB_red, randomBool() ? HIGH : LOW);
      digitalWrite(RGB_green, randomBool() ? HIGH : LOW);
      digitalWrite(RGB_blue, randomBool() ? HIGH : LOW);
      T1 = millis();    

      // if BassGain is not 12, show a fixed green color
    }else if(BassGain != 12){
      digitalWrite(RGB_red, LOW);
      digitalWrite(RGB_green, HIGH);
      digitalWrite(RGB_blue, LOW);
    }

    // if switch is false show red color
  }else{
      digitalWrite(RGB_red, HIGH);
      digitalWrite(RGB_green, LOW);
      digitalWrite(RGB_blue, LOW);
  }
  

  // -------------------------
  // Switchstate for menu selection, when encoder btn is pressed enter the current menu.
  // -------------------------
  if(digitalRead(ENC_BTN)==HIGH){
    switch(currentMenuItem){
      // -------------------------
      // Menu item 0: Bass Gain
      // -------------------------
      case 0:
        delay(250);
        BassMenu();
        while(1){
          BassGain = handleEncoder(BassGain, -3, 12,1);
          display.fillRoundRect(6,38,116,20,3,BLACK);
          float Width = map(BassGain, -3, 12, 0,116);
          if(Width != 0)
            display.fillRoundRect(6,38,Width,20,3,WHITE);
          display.display();
          if(digitalRead(ENC_BTN)==HIGH){
            delay(250);
            break;
          } 
        }
      break;
      

      // -------------------------
      // Menu item 1: Mid Gain
      // -------------------------
      case 1:
        delay(250);
        MidMenu();
        while(1){
          MidGain = handleEncoder(MidGain, -3, 12,1);
          display.fillRoundRect(6,38,116,20,3,BLACK);
          float Width = map(MidGain, -3, 12, 0,116);
          if(Width != 0)
            display.fillRoundRect(6,38,Width,20,3,WHITE);
          display.display();
          if(digitalRead(ENC_BTN)==HIGH){
            delay(250);
            break;
          } 
        }
      break;  

      // -------------------------
      // Menu item 2: Treble Gain
      // -------------------------
      case 2:
        delay(250);
        TrebbleMenu();
        while(1){
          TebbleGain = handleEncoder(TebbleGain, -3, 12,1);
          display.fillRoundRect(6,38,116,20,3,BLACK);
          float Width = map(TebbleGain, -3, 12, 0,116);
          if(Width != 0)
            display.fillRoundRect(6,38,Width,20,3,WHITE);
          display.display();
          if(digitalRead(ENC_BTN)==HIGH){
            delay(250);
            break;
          } 
        }
      break;

      // -------------------------
      // Menu item 3: Filter Amount
      // -------------------------
      case 3:
        delay(250);
        FilterAmount();
        while(1){
          
          FilterAmt = handleEncoder(FilterAmt, 0, 100,1);
          String s = String(FilterAmt);
          display.fillRoundRect(6,38,116,20,3,BLACK);
          printCentered(s.c_str(), 41, 2);
          display.display();
          if(digitalRead(ENC_BTN)==HIGH){
            delay(250);
            break;
          } 
        }
      break;

      // -------------------------
      // Menu item 4: Low Frequency Deviation
      // -------------------------
      case 4:
        delay(250);
        LowFreqDevi();
        while(1){
          LowFreqDev = handleEncoder(LowFreqDev, 0.5, 3,0.1);
          String s = String(LowFreqDev) + " dB";
          display.fillRoundRect(6,38,116,20,3,BLACK);
          printCentered(s.c_str(), 41, 2);
          display.display();
          if(digitalRead(ENC_BTN)==HIGH){
            delay(250);
            break;
          } 
        }
      break;      

      // -------------------------
      // Menu item 5: High Frequency Deviation
      // -------------------------
      case 5:
        delay(250);
        HighFreqDevi();
        while(1){
          HighFreqDev = handleEncoder(HighFreqDev, 0.5, 3,0.1);
          String s = String(HighFreqDev) + " dB";
          display.fillRoundRect(6,38,116,20,3,BLACK);
          printCentered(s.c_str(), 41, 2);
          display.display();
          if(digitalRead(ENC_BTN)==HIGH){
            delay(250);
            break;
          } 
        }
      break;  

      // -------------------------
      // Menu item 6: Crossover Frequency
      // -------------------------
      case 6:
        delay(250);
        CrossFreqi();
        while(1){
          CrossFreq = handleEncoder(CrossFreq, 50, 10000,10);
          String s = String(CrossFreq) + " Hz";
          display.fillRoundRect(6,38,116,20,3,BLACK);
          printCentered(s.c_str(), 41, 2);
          display.display();
          if(digitalRead(ENC_BTN)==HIGH){
            delay(250);
            break;
          } 
        }
      break;  
      
      // -------------------------
      // Menu item 8: Test screen
      // -------------------------
      case 8:
        display.clearDisplay();
        display.setCursor(0,0);
        display.setTextColor(WHITE);
        display.setTextSize(5);
        display.print("TEST");
        display.display();
      break;

      default:
      break;
    }

  // If button not pressed, show main menu
  }else{
    mainMenu(); 
  }

}
