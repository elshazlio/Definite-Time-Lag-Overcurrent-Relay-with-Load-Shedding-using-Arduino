#include <LiquidCrystal.h>

// Set up the LCD screen
LiquidCrystal lcd(13, 12, 11, 10, 9, 8);

// Pin where the current sensor is connected
const int CURRENT_SENSOR = A0;

// Pins controlling the relays (switches) for each lamp
const int RELAY1 = 7;
const int RELAY2 = 6;
const int RELAY3 = 5;
const int RELAY4 = 4;

// Factor to convert sensor reading to actual current
const float CURRENT_CONVERSION_FACTOR = 0.0196;

// Current level that's considered too high
const float OVERCURRENT_THRESHOLD = 0.61; // Amps

// Wait time before taking action on high current
const unsigned long TIME_LAG = 4000; // 4 seconds

// Time between turning off each lamp
const unsigned long LOAD_SHED_DELAY = 3000; // 3 seconds

// Variables to keep track of system state
bool overcurrentDetected = false;
unsigned long overcurrentStartTime = 0;
bool loadSheddingStarted = false;
int currentShedStage = 0;

// Variables for smoothing out current readings
const int NUM_READINGS = 10;
float readings[NUM_READINGS];
int readIndex = 0;
float total = 0;
float average = 0;

// How often to check the current
const unsigned long SAMPLING_INTERVAL = 500; // 0.5 seconds
unsigned long lastSampleTime = 0;
unsigned long lastShedTime = 0;

void setup() {
  // Initialize the LCD and serial communication
  lcd.begin(20, 4);
  Serial.begin(9600);
  
  // Set up the relay pins to control lamps
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(RELAY4, OUTPUT);
  
  // Initially, all lamps are on (relays are off)
  digitalWrite(RELAY1, LOW);
  digitalWrite(RELAY2, LOW);
  digitalWrite(RELAY3, LOW);
  digitalWrite(RELAY4, LOW);

  // Initialize the array for smoothing current readings
  for (int i = 0; i < NUM_READINGS; i++) {
    readings[i] = 0;
  }
}

// Function to read the current from the sensor
float readCurrent() {
  int sensorValue = analogRead(CURRENT_SENSOR);
  float current = (sensorValue - 512) * CURRENT_CONVERSION_FACTOR;
  return abs(current);
}

// Function to get a smoothed current reading
float getAverageCurrent() {
  // Update the total by removing the oldest reading and adding the newest
  total = total - readings[readIndex];
  readings[readIndex] = readCurrent();
  total = total + readings[readIndex];
  // Move to the next position in the array
  readIndex = (readIndex + 1) % NUM_READINGS;
  
  // Calculate and return the average
  return total / NUM_READINGS;
}

// Function to update the LCD display
void updateLCD(float current, bool isLoadShedding) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Current: ");
  lcd.print(current, 2);
  lcd.print(" A");
  
  lcd.setCursor(0, 1);
  if (isLoadShedding) {
    // Show which lamp is about to be turned off
    lcd.print("Load Shedding: L");
    lcd.print(5 - currentShedStage);
    lcd.setCursor(0, 2);
    // Show countdown to next lamp turn-off
    unsigned long remainingTime = LOAD_SHED_DELAY - (millis() - lastShedTime);
    lcd.print("Next in: ");
    lcd.print(remainingTime / 1000);
    lcd.print("s");
  } else if (overcurrentDetected) {
    // Show that high current was detected and countdown to action
    lcd.print("Overcurrent detected");
    lcd.setCursor(0, 2);
    lcd.print("Waiting: ");
    lcd.print((TIME_LAG - (millis() - overcurrentStartTime)) / 1000);
    lcd.print("s");
  } else {
    // Everything is normal
    lcd.print("Status: Normal");
  }
}

// Function to start the process of turning off lamps
void startLoadShedding() {
  loadSheddingStarted = true;
  currentShedStage = 0;
  lastShedTime = millis();
}

// Function to turn off lamps one by one
void performLoadShedding() {
  if (millis() - lastShedTime >= LOAD_SHED_DELAY) {
    // Turn off one lamp at a time
    switch(currentShedStage) {
      case 0:
        digitalWrite(RELAY4, HIGH);
        break;
      case 1:
        digitalWrite(RELAY3, HIGH);
        break;
      case 2:
        digitalWrite(RELAY2, HIGH);
        break;
      case 3:
        digitalWrite(RELAY1, HIGH);
        break;
    }
    currentShedStage++;
    lastShedTime = millis();
    
    // Stop after all lamps are off
    if (currentShedStage > 3) {
      loadSheddingStarted = false;
    }
  }
}

// Main program loop
void loop() {
  unsigned long currentTime = millis();

  // Check current at regular intervals
  if (currentTime - lastSampleTime >= SAMPLING_INTERVAL) {
    lastSampleTime = currentTime;
    float current = getAverageCurrent();
    
    if (!loadSheddingStarted) {
      if (current > OVERCURRENT_THRESHOLD) {
        // If current is too high, start countdown
        if (!overcurrentDetected) {
          overcurrentDetected = true;
          overcurrentStartTime = currentTime;
        } else if (currentTime - overcurrentStartTime >= TIME_LAG) {
          // If current stays high for too long, start turning off lamps
          startLoadShedding();
        }
      } else {
        // Current is normal again
        overcurrentDetected = false;
      }
    } else {
      // Continue turning off lamps if needed
      performLoadShedding();
    }
    
    // Update the LCD display
    updateLCD(current, loadSheddingStarted);
  }
}