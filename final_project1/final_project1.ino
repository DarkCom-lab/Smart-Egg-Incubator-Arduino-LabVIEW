/*
==================================================
  EGG INCUBATOR - LABVIEW COMMAND INTERFACE
  Commands from LabVIEW:
    T 30    → Set max temperature to 30°C
    H 80    → Set max humidity to 80%
    F       → Toggle fan ON/OFF
    AUTO    → Return to automatic mode (use T value if received, otherwise potentiometer)
  Auto-timeout: Returns to automatic mode after 10 seconds of no commands
==================================================
*/

#include <DHT.h>
#include <Servo.h>

// ========== PIN DEFINITIONS ==========
#define DHTPIN 7        // DHT11 data pin
#define DHTTYPE DHT11   // DHT11 sensor type
#define POT_PIN A0      // Potentiometer for max temp setting
#define LED_PIN 3       // Overheat warning LED
#define SERVO_PIN 4     // Window servo motor
#define FAN_PIN 5       // Fan control pin
#define BUZZER_PIN 6    // Buzzer for humidity pass max alert

// ========== SENSOR OBJECTS ==========
DHT dht(DHTPIN, DHTTYPE);
Servo windowServo;

// ========== SYSTEM CONSTANTS ==========
const int CASE_INCREMENT = 10;           // Increment case number by 10
const unsigned long UPDATE_INTERVAL = 2000;  // Send data every 2 seconds
const int TEMP_HYSTERESIS = 3;           // 3°C hysteresis for temperature
const int HUM_HYSTERESIS = 5;            // 5% hysteresis for humidity
const int VENT_CLOSED = 0;               // Window closed position
const int VENT_OPEN = 90;                // Window open position
const unsigned long AUTO_TIMEOUT = 10000; // 10 seconds timeout for auto mode

// ========== SYSTEM VARIABLES ==========
// Current readings
float currentTemp = 25.0;                // Current temperature (°C)
float currentHumidity = 60.0;            // Current humidity (%)

// SETPOINTS (can be changed by LabVIEW or potentiometer)
int maxTemperature = 30;                 // Maximum temperature setpoint (°C)
int maxHumidity = 80;                    // Maximum humidity setpoint (%)

// Status flags
bool isTempHigh = false;                 // Temperature ≥ max_temp
bool isHumidityHigh = false;             // Humidity ≥ max_humidity
bool isFanRunning = false;               // Fan is currently active
bool isWindowOpen = false;               // Window is open
bool manualFanControl = false;           // Fan is in manual mode (by LabVIEW)
bool usePotentiometer = true;            // Use potentiometer or LabVIEW commands

// Timing variables
unsigned long lastUpdateTime = 0;        // Last data send time
unsigned long lastCommandTime = 0;       // Last command received time
int caseNumber = 0;                      // Current case number

// Error handling
int sensorErrorCount = 0;
const int MAX_SENSOR_ERRORS = 5;

// Track if we should use command value for next case
bool useCommandForNextCase = false;
int commandTempValue = 30;  // Store the T value received with AUTO command

// ========== SETUP FUNCTION ==========
void setup() {
  // Initialize serial communication
  Serial.begin(9600);
  while (!Serial) {
    ; // Wait for serial port to connect
  }
  
  // Initialize DHT sensor
  dht.begin();
  
  // Initialize pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  
  // Initialize servo motor
  windowServo.attach(SERVO_PIN);
  
  // Set initial states
  windowServo.write(VENT_CLOSED);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Wait for sensor stabilization
  delay(2000);
  
  // Startup sequence
  startupSequence();
  
  // Initialize last command time
  lastCommandTime = millis();
}

// ========== MAIN LOOP FUNCTION ==========
void loop() {
  unsigned long currentTime = millis();
  
  // 1. Check for auto-timeout
  if (currentTime - lastCommandTime >= AUTO_TIMEOUT) {
    if (manualFanControl) {
      returnToAutomaticMode();
    }
  }
  
  // 2. Read sensors
  readSensors();
  
  // 3. Read control inputs
  if (usePotentiometer) {
    // Use potentiometer for temperature control
    int potValue = analogRead(POT_PIN);
    maxTemperature = map(potValue, 0, 1023, 25, 40);
  }
  
  // 4. Run control logic (only if not in manual fan mode)
  if (!manualFanControl) {
    runControlSystem();
  }
  
  // 5. Send data to LabVIEW (every 2 seconds)
  if (currentTime - lastUpdateTime >= UPDATE_INTERVAL) {
    sendToLabVIEW();
    lastUpdateTime = currentTime;
    caseNumber += CASE_INCREMENT;
    
    // After sending one case, if we were using command value, revert to potentiometer
    if (useCommandForNextCase) {
      usePotentiometer = true;  // Back to using potentiometer
      useCommandForNextCase = false;
    }
  }
  
  // 6. Check for serial commands from LabVIEW
  checkSerialCommands();
  
  // Small delay
  delay(100);
}

// ========== FUNCTION DEFINITIONS ==========

void startupSequence() {
  // Blink LED
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_PIN, HIGH);
    delay(200);
    digitalWrite(LED_PIN, LOW);
    delay(200);
  }
  
  // Test servo
  windowServo.write(VENT_OPEN);
  delay(500);
  windowServo.write(VENT_CLOSED);
  delay(500);
}

void readSensors() {
  // Try to read DHT11 sensor
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  
  if (!isnan(temp) && !isnan(hum)) {
    // Valid readings
    currentTemp = temp;
    currentHumidity = hum;
    sensorErrorCount = 0;
    
    // Clamp to reasonable ranges
    currentTemp = constrain(currentTemp, 15.0, 45.0);
    currentHumidity = constrain(currentHumidity, 20.0, 95.0);
  } else {
    // DHT read failed, use simulation
    sensorErrorCount++;
    if (sensorErrorCount > MAX_SENSOR_ERRORS) {
      simulateSensorValues();
    }
  }
}

void simulateSensorValues() {
  // Simulate temperature changes
  static float baseTemp = 25.0;
  static bool rising = true;
  
  if (rising) {
    baseTemp += 0.1;
    if (baseTemp >= 35.0) rising = false;
  } else {
    baseTemp -= 0.1;
    if (baseTemp <= 20.0) rising = true;
  }
  
  // Fan cooling effect
  float fanEffect = isFanRunning ? -2.0 : 0.0;
  float ventEffect = isWindowOpen ? -0.5 : 0.0;
  
  currentTemp = baseTemp + fanEffect + ventEffect + (random(-10, 10) / 10.0);
  currentTemp = constrain(currentTemp, 20.0, 35.0);
  
  // Simulate humidity
  static float baseHumidity = 60.0;
  baseHumidity += (random(-20, 20) / 100.0);
  
  float tempEffect = (currentTemp - 25) * 0.3;
  float ventHumEffect = (isWindowOpen || isFanRunning) ? -5.0 : 0.0;
  
  currentHumidity = baseHumidity + tempEffect + ventHumEffect;
  currentHumidity = constrain(currentHumidity, 40.0, 90.0);
}

void runControlSystem() {
  // ===== TEMPERATURE CONTROL =====
  if (currentTemp >= maxTemperature) {
    if (!isTempHigh) {
      isTempHigh = true;
      digitalWrite(LED_PIN, HIGH);
      // No buzzer for temperature - only LED
    }
  } else if (currentTemp <= (maxTemperature - TEMP_HYSTERESIS)) {
    if (isTempHigh) {
      isTempHigh = false;
      digitalWrite(LED_PIN, LOW);
    }
  }
  
  // ===== HUMIDITY CONTROL =====
  if (currentHumidity >= maxHumidity) {
    if (!isHumidityHigh) {
      isHumidityHigh = true;
      // BUZZER for humidity alert - ONLY when humidity exceeds max
      tone(BUZZER_PIN, 1500, 100);
      delay(150);
      tone(BUZZER_PIN, 1500, 100);
    }
  } else if (currentHumidity <= (maxHumidity - HUM_HYSTERESIS)) {
    if (isHumidityHigh) {
      isHumidityHigh = false;
      // Stop any ongoing buzzer for humidity
      noTone(BUZZER_PIN);
    }
  }
  
  // ===== AUTOMATIC FAN CONTROL =====
  // Fan turns on automatically if temperature OR humidity exceeds max
  if ((isTempHigh || isHumidityHigh) && !manualFanControl) {
    if (!isFanRunning) {
      activateCooling();
    }
  } else if (!manualFanControl) {
    // Only turn off fan automatically if not in manual mode
    if (isFanRunning) {
      deactivateCooling();
    }
  }
}

void activateCooling() {
  windowServo.write(VENT_OPEN);
  isWindowOpen = true;
  delay(100);
  
  digitalWrite(FAN_PIN, HIGH);
  isFanRunning = true;
}

void deactivateCooling() {
  digitalWrite(FAN_PIN, LOW);
  isFanRunning = false;
  
  delay(1000);
  windowServo.write(VENT_CLOSED);
  isWindowOpen = false;
  delay(100);
}

void sendToLabVIEW() {
  // Format: "caseX: temp hum temp_status fan_status"
  Serial.print("case");
  Serial.print(caseNumber);
  Serial.print(": ");
  
  // Current temperature (integer)
  Serial.print((int)currentTemp);
  Serial.print(" ");
  
  // Current humidity (integer)
  Serial.print((int)currentHumidity);
  Serial.print(" ");
  
  // Temperature status (0/1) - 1 if current temp >= max temp
  Serial.print(isTempHigh ? 1 : 0);
  Serial.print(" ");
  
  // Fan status (0/1)
  Serial.println(isFanRunning ? 1 : 0);
  
  // Reset case number if it gets too large
  if (caseNumber >= 10000) {
    caseNumber = 0;
  }
}

void returnToAutomaticMode() {
  manualFanControl = false;
  
  // Only turn off fan automatically if not needed for cooling
  if (isFanRunning && !(isTempHigh || isHumidityHigh)) {
    digitalWrite(FAN_PIN, LOW);
    isFanRunning = false;
    
    if (isWindowOpen) {
      windowServo.write(VENT_CLOSED);
      isWindowOpen = false;
    }
  }
}

void processCombinedCommand(String command) {
  // Remove "AUTO" prefix and trim
  command = command.substring(4);
  command.trim();
  
  bool hasTCommand = false;
  int tempFromCommand = maxTemperature;  // Default to current value
  
  // Process each command in the combined string
  int index = 0;
  while (index < command.length()) {
    char cmdChar = command.charAt(index);
    
    if (cmdChar == 'T' && index + 1 < command.length()) {
      // Extract temperature value
      String valueStr = "";
      index++; // Skip 'T'
      
      // Collect digits
      while (index < command.length() && (isdigit(command.charAt(index)) || command.charAt(index) == ' ')) {
        if (command.charAt(index) != ' ') {
          valueStr += command.charAt(index);
        }
        index++;
      }
      
      int temp = valueStr.toInt();
      if (temp >= 25 && temp <= 40) {
        maxTemperature = temp;
        hasTCommand = true;
        tempFromCommand = temp;
      }
      
    } else if (cmdChar == 'H' && index + 1 < command.length()) {
      // Extract humidity value
      String valueStr = "";
      index++; // Skip 'H'
      
      // Collect digits
      while (index < command.length() && (isdigit(command.charAt(index)) || command.charAt(index) == ' ')) {
        if (command.charAt(index) != ' ') {
          valueStr += command.charAt(index);
        }
        index++;
      }
      
      int hum = valueStr.toInt();
      if (hum >= 40 && hum <= 95) {
        maxHumidity = hum;
      }
      
    } else if (cmdChar == 'F') {
      // Toggle fan manually
      if (isFanRunning) {
        digitalWrite(FAN_PIN, LOW);
        isFanRunning = false;
        manualFanControl = true;
        
        // Close window if open
        if (isWindowOpen) {
          windowServo.write(VENT_CLOSED);
          isWindowOpen = false;
        }
      } else {
        digitalWrite(FAN_PIN, HIGH);
        isFanRunning = true;
        manualFanControl = true;
        
        // Open window
        windowServo.write(VENT_OPEN);
        isWindowOpen = true;
      }
      index++;
      
    } else {
      // Skip unknown characters
      index++;
    }
  }
  
  // After processing AUTO command, set to use command value for next case
  // if T command was included, otherwise use potentiometer
  if (hasTCommand) {
    usePotentiometer = false;  // Don't use potentiometer for next case
    useCommandForNextCase = true;
    commandTempValue = tempFromCommand;
    maxTemperature = commandTempValue;  // Set the max temperature to command value
  } else {
    usePotentiometer = true;  // Use potentiometer
    useCommandForNextCase = false;
  }
  
  // Finally set fan to automatic mode
  manualFanControl = false;
}

void checkSerialCommands() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    command.toUpperCase();
    
    // Update last command time
    lastCommandTime = millis();
    
    // Remove any extra spaces
    while (command.indexOf("  ") != -1) {
      command.replace("  ", " ");
    }
    
    // ===== COMBINED COMMANDS STARTING WITH AUTO =====
    if (command.startsWith("AUTO")) {
      if (command.length() > 4) {
        // Combined command like AUTOT 80 or AUTOH 40T 70
        processCombinedCommand(command);
      } else {
        // Simple AUTO command - return to automatic mode with potentiometer
        returnToAutomaticMode();
        usePotentiometer = true;
        useCommandForNextCase = false;
      }
      return;
    }
    
    // ===== MAX TEMPERATURE COMMAND: "T 30" =====
    if (command.startsWith("T ")) {
      String valueStr = command.substring(2);
      valueStr.trim();
      int temp = valueStr.toInt();
      
      if (temp >= 25 && temp <= 40) {
        maxTemperature = temp;  // Change max temperature
        usePotentiometer = false;  // Use this value
        useCommandForNextCase = true;  // Use for next case
        commandTempValue = temp;  // Store the value
      }
    }
    
    // ===== MAX HUMIDITY COMMAND: "H 80" =====
    else if (command.startsWith("H ")) {
      String valueStr = command.substring(2);
      valueStr.trim();
      int hum = valueStr.toInt();
      
      if (hum >= 40 && hum <= 95) {
        maxHumidity = hum;  // Change max humidity
      }
    }
    
    // ===== FAN TOGGLE COMMAND: "F" =====
    else if (command == "F") {
      // Manual fan toggle
      if (isFanRunning) {
        digitalWrite(FAN_PIN, LOW);
        isFanRunning = false;
        manualFanControl = true;
        
        if (isWindowOpen) {
          windowServo.write(VENT_CLOSED);
          isWindowOpen = false;
        }
      } else {
        digitalWrite(FAN_PIN, HIGH);
        isFanRunning = true;
        manualFanControl = true;
        
        windowServo.write(VENT_OPEN);
        isWindowOpen = true;
      }
    }
  }
}