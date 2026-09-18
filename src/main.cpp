#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RotaryEncoder.h>
#include <Preferences.h>
#include <RTClib.h>
#include "bitmaps.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Preferences preferences;
RTC_DS1307 rtc; // Use DS1307 class for the Wokwi DS1307 component

// ESP32 Rotary Encoder Pins: CLK -> Pin 16, DT -> Pin 17
RotaryEncoder encoder(16, 17, RotaryEncoder::LatchMode::FOUR3);
const int encoderButtonPin = 19;

// State management - STATE_SHOW_TIME is set as the default startup state
enum AppState {
  STATE_SHOW_TIME,
  STATE_ENTER_MILES,
  STATE_ENTER_LITRES,
  STATE_SHOW_MPG
};

AppState currentState = STATE_SHOW_TIME;

float lastOdometer = 0.0;     
float currentOdometer = 0.0;  
float milesDriven = 0.0;      
float fuelLiters = 0.0;
float calculatedMpg = 0.0;
int lastPos = 0;

// Timer variables for the 5-second time mode delay
unsigned long timeStateEntryMillis = 0;
bool timeToDisplayClock = false;

// Forward declarations
void updateDisplay();
void loadStoredOdometer();
void saveStoredOdometer();
void setupRTC();

/**
 * @brief Initialises serial communication, display, input pins, persistent storage, and the RTC module.
 */
void setup() {
  Serial.begin(115200);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  pinMode(encoderButtonPin, INPUT_PULLUP);

  // Initialise the RTC module over I2C
  setupRTC();

  // Load the last saved odometer baseline from flash memory
  loadStoredOdometer();
  currentOdometer = lastOdometer;

  // Initialise the time state timer so it starts counting down immediately on boot
  timeStateEntryMillis = millis();
  timeToDisplayClock = false;
  
  // Load the first operational state screen (shows the van graphic initially)
  updateDisplay();
}

/**
 * @brief Main execution loop handling encoder rotation, button clicks, and screen state logic.
 */
void loop() {
  encoder.tick();
  int newPos = encoder.getPosition();
  
  // Handle encoder rotation based on current state
  if (lastPos != newPos) {
    int change = newPos - lastPos;
    lastPos = newPos;

    if (currentState == STATE_ENTER_MILES) {
      currentOdometer += change * 1.0; 
      if (currentOdometer < 0) currentOdometer = 0;
    } 
    else if (currentState == STATE_ENTER_LITRES) {
      fuelLiters += change * 0.1; 
      if (fuelLiters < 0) fuelLiters = 0;
    }
    updateDisplay();
  }

  // Handle live clock updates if we are viewing the time state
  if (currentState == STATE_SHOW_TIME) {
    if (!timeToDisplayClock && (millis() - timeStateEntryMillis >= 5000)) {
      timeToDisplayClock = true;
      updateDisplay();
    } else if (timeToDisplayClock) {
      static unsigned long lastClockUpdate = 0;
      if (millis() - lastClockUpdate >= 1000) {
        lastClockUpdate = millis();
        updateDisplay();
      }
    }
  }

  // Simplified and reliable button click handler
  static bool lastBtnState = HIGH;
  bool btnState = digitalRead(encoderButtonPin);
  
  if (btnState == LOW && lastBtnState == HIGH) {
    if (currentState == STATE_SHOW_TIME) {
      currentState = STATE_ENTER_MILES;
      encoder.setPosition(0);
      lastPos = 0;
    }
    else if (currentState == STATE_ENTER_MILES) {
      milesDriven = currentOdometer - lastOdometer;
      if (milesDriven < 0) milesDriven = 0; 
      
      currentState = STATE_ENTER_LITRES;
      encoder.setPosition(0);
      lastPos = 0;
    } 
    else if (currentState == STATE_ENTER_LITRES) {
      if (fuelLiters > 0 && milesDriven > 0) {
        float gallonsUK = fuelLiters / 4.54609;
        calculatedMpg = milesDriven / gallonsUK;
      } else {
        calculatedMpg = 0;
      }
      
      lastOdometer = currentOdometer;
      saveStoredOdometer();
      
      currentState = STATE_SHOW_MPG;
    } 
    else if (currentState == STATE_SHOW_MPG) {
      currentState = STATE_SHOW_TIME;
      timeStateEntryMillis = millis();
      timeToDisplayClock = false;
      encoder.setPosition(0);
      lastPos = 0;
    }
    updateDisplay();
    delay(250); // Debounce delay
  }
  lastBtnState = btnState;
}

/**
 * @brief Initialises the RTC module and checks if it's running correctly.
 */
void setupRTC() {
  if (!rtc.begin()) {
    Serial.println(F("Couldn't find RTC"));
    while (1);
  }
}

/**
 * @brief Saves the current odometer baseline value into permanent flash memory.
 */
void saveStoredOdometer() {
  preferences.begin("mpg-calc", false);
  preferences.putFloat("lastOdom", lastOdometer);
  preferences.end();
}

/**
 * @brief Loads the previously saved odometer baseline value from flash memory.
 */
void loadStoredOdometer() {
  preferences.begin("mpg-calc", true); 
  lastOdometer = preferences.getFloat("lastOdom", 0.0);
  preferences.end();
}

/**
 * @brief Renders the correct UI layout onto the OLED display depending on the active application state.
 */
void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  if (currentState == STATE_SHOW_TIME) {
    if (!timeToDisplayClock) {
      // Draw the detailed 64x24 campervan bitmap centered during the 5-second startup window
      // X = (128 - 64) / 2 = 32, Y = (64 - 24) / 2 = 20
      display.drawBitmap(0, 0, honda_acty_bmp, 128, 64, SSD1306_WHITE);
    } else {
      DateTime now = rtc.now();
      display.setTextSize(3); // Larger text for hours:minutes
      
      char timeBuffer[6];
      sprintf(timeBuffer, "%02d:%02d", now.hour(), now.minute());
      
      // Center the 5-character string (width ~90px): (128 - 90) / 2 = 19
      display.setCursor(19, 22);
      display.print(timeBuffer);
    }
  }
  else if (currentState == STATE_ENTER_MILES) {
    display.setTextSize(2);
    char milesText[16];
    snprintf(milesText, sizeof(milesText), "%d mi", (int)currentOdometer);

    int16_t textX, textY;
    uint16_t textWidth, textHeight;
    display.getTextBounds(milesText, 0, 25, &textX, &textY, &textWidth, &textHeight);
    display.setCursor((SCREEN_WIDTH - textWidth) / 2, 25);
    display.print(milesText);
  } 
  else if (currentState == STATE_ENTER_LITRES) {
    display.setTextSize(2);
    char litresText[16];
    snprintf(litresText, sizeof(litresText), "%.1f L", fuelLiters);

    int16_t textX, textY;
    uint16_t textWidth, textHeight;
    display.getTextBounds(litresText, 0, 25, &textX, &textY, &textWidth, &textHeight);
    display.setCursor((SCREEN_WIDTH - textWidth) / 2, 25);
    display.print(litresText);
  } 
  else if (currentState == STATE_SHOW_MPG) {
    display.setTextSize(2);
    char mpgText[16];
    snprintf(mpgText, sizeof(mpgText), "%.1f MPG", calculatedMpg);

    int16_t textX, textY;
    uint16_t textWidth, textHeight;
    display.getTextBounds(mpgText, 0, 20, &textX, &textY, &textWidth, &textHeight);
    display.setCursor((SCREEN_WIDTH - textWidth) / 2, 20);
    display.print(mpgText);
    
    display.setTextSize(1);
    const char *prompt = "Click for Time";
    int16_t promptX, promptY;
    uint16_t promptWidth, promptHeight;
    display.getTextBounds(prompt, 0, 0, &promptX, &promptY, &promptWidth, &promptHeight);
    display.setCursor((SCREEN_WIDTH - promptWidth) / 2, 50);
    display.print(prompt);
  }
  display.display();
}