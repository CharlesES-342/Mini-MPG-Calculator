#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RotaryEncoder.h>
#include <Preferences.h>
#include "bitmaps.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

Preferences preferences;

// ESP32 Rotary Encoder Pins: CLK -> Pin 16, DT -> Pin 17
RotaryEncoder encoder(16, 17, RotaryEncoder::LatchMode::FOUR3);
const int encoderButtonPin = 19;

// State management
enum AppState {
  STATE_ENTER_MILES,
  STATE_ENTER_LITRES,
  STATE_SHOW_MPG
};

AppState currentState = STATE_ENTER_MILES;

float lastOdometer = 0.0;     // Stored persistently (baseline from previous fill-up)
float currentOdometer = 0.0;  // Currently edited odometer reading
float milesDriven = 0.0;      // Calculated trip distance (current - last)
float fuelLiters = 0.0;
float calculatedMpg = 0.0;
int lastPos = 0;

// Forward declarations
void updateDisplay();
void bootScreen();
void loadStoredOdometer();
void saveStoredOdometer();

void setup() {
  Serial.begin(115200);
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  pinMode(encoderButtonPin, INPUT_PULLUP);

  // Load the last saved odometer baseline from flash memory
  loadStoredOdometer();
  currentOdometer = lastOdometer;

  // Run the boot screen once on startup
  bootScreen();
  
  // Load the first operational state screen
  updateDisplay();
}

void loop() {
  encoder.tick();
  int newPos = encoder.getPosition();
  
  // Handle encoder rotation based on current state (supports going up and down)
  if (lastPos != newPos) {
    int change = newPos - lastPos;
    lastPos = newPos;

    if (currentState == STATE_ENTER_MILES) {
      currentOdometer += change * 1.0; // Adjust total odometer value up or down
      if (currentOdometer < 0) currentOdometer = 0;
    } 
    else if (currentState == STATE_ENTER_LITRES) {
      fuelLiters += change * 0.1; // Steps of 0.1 Litres
      if (fuelLiters < 0) fuelLiters = 0;
    }
    updateDisplay();
  }

  // Simplified and reliable button click handler for simulation
  static bool lastBtnState = HIGH;
  bool btnState = digitalRead(encoderButtonPin);
  
  if (btnState == LOW && lastBtnState == HIGH) {
    // Button pressed transition logic
    if (currentState == STATE_ENTER_MILES) {
      // Calculate trip miles driven from the baseline odometer reading
      milesDriven = currentOdometer - lastOdometer;
      if (milesDriven < 0) milesDriven = 0; // Prevent negative trip distance if misinputted below baseline
      
      currentState = STATE_ENTER_LITRES;
      encoder.setPosition(0);
      lastPos = 0;
    } 
    else if (currentState == STATE_ENTER_LITRES) {
      // Calculate UK MPG (1 UK Gallon = 4.54609 Litres)
      if (fuelLiters > 0 && milesDriven > 0) {
        float gallonsUK = fuelLiters / 4.54609;
        calculatedMpg = milesDriven / gallonsUK;
      } else {
        calculatedMpg = 0;
      }
      
      // Save the new odometer reading permanently as the new baseline
      lastOdometer = currentOdometer;
      saveStoredOdometer();
      
      currentState = STATE_SHOW_MPG;
    } 
    else if (currentState == STATE_SHOW_MPG) {
      // Reset for a new calculation cycle, keeping the updated odometer baseline
      currentState = STATE_ENTER_MILES;
      fuelLiters = 0.0;
      currentOdometer = lastOdometer; 
      encoder.setPosition(0);
      lastPos = 0;
    }
    updateDisplay();
    delay(250); // Debounce delay
  }
  lastBtnState = btnState;
}

void saveStoredOdometer() {
  preferences.begin("mpg-calc", false);
  preferences.putFloat("lastOdom", lastOdometer);
  preferences.end();
}

void loadStoredOdometer() {
  preferences.begin("mpg-calc", true); // Read-only mode
  lastOdometer = preferences.getFloat("lastOdom", 0.0);
  preferences.end();
}

void bootScreen() {
  display.clearDisplay();
  
  // Draw the Kei-truck bitmap centered (32 pixels wide, positioned at X=48, Y=12)
  display.drawBitmap(48, 12, kei_truck_bmp, 32, 16, SSD1306_WHITE);
  
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(14, 38);
  display.println(F("Kei-Truck MPG Calc"));
  
  display.display();
  delay(2000); // Show for 2 seconds before clearing into setup
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  
  if (currentState == STATE_ENTER_MILES) {
    display.setCursor(0, 0);
    display.println(F("Step 1: Odometer"));
    display.setTextSize(2);
    display.setCursor(0, 25);
    display.print(currentOdometer, 0);
    display.print(" mi");
  } 
  else if (currentState == STATE_ENTER_LITRES) {
    display.setCursor(0, 0);
    display.println(F("Step 2: Set Litres"));
    display.setTextSize(2);
    display.setCursor(0, 25);
    display.print(fuelLiters, 1);
    display.print(" L");
  } 
  else if (currentState == STATE_SHOW_MPG) {
    display.setCursor(0, 0);
    display.println(F("Result (UK Gallon):"));
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print(calculatedMpg, 1);
    display.print(" MPG");
    
    display.setTextSize(1);
    display.setCursor(0, 50);
    display.print(F("Click to restart"));
  }
  display.display();
}