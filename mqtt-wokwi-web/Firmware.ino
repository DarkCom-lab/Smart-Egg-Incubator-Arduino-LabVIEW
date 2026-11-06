#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// WiFi credentials for Wokwi
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// MQTT Broker settings
const char* mqtt_server = "broker.emqx.io";
const int mqtt_port = 1883;
const char* client_id = "esp32_incubator";

// MQTT Topics
const char* topic_heartbeat = "incubator/heartbeat";
const char* topic_sensor_readings = "incubator/sensor_readings";
const char* topic_control_setpoints = "incubator/control_setpoints";
const char* topic_manual_controls = "incubator/manual_controls";
const char* topic_system_status = "incubator/system_status";
const char* topic_commands = "incubator/commands";

// Pin definitions
#define DHTPIN 4
#define DHTTYPE DHT22
#define POT_PIN 36
#define LED_PIN 2
#define SERVO_PIN 5
#define FAN_PIN 18
#define BUZZER_PIN 19

// Components
DHT dht(DHTPIN, DHTTYPE);
WiFiClient espClient;
PubSubClient client(espClient);
Servo windowServo;

// System variables
float temperature = 25.0;
float humidity = 60.0;
int maxTemperature = 30;
int maxHumidity = 80;
bool autoMode = true;
bool fanState = false;
bool windowState = false;
bool buzzerState = false;
bool sensorError = false;
int servoPosition = 0;
unsigned long lastMsg = 0;
unsigned long lastHeartbeat = 0;
int lastPotValue = -1;
int sendCounter = 0;

void setup() {
  Serial.begin(115200);
  
  // Initialize pins
  pinMode(LED_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(POT_PIN, INPUT);
  
  // Initialize DHT22 sensor
  dht.begin();
  
  // Initialize servo
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  windowServo.setPeriodHertz(50);
  windowServo.attach(SERVO_PIN, 500, 2400);
  windowServo.write(0);
  servoPosition = 0;
  
  // Set initial states
  digitalWrite(LED_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  
  // Connect to WiFi
  setup_wifi();
  
  // Setup MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  
  Serial.println("\n\n=== Egg Incubator MQTT Controller ===");
  Serial.println("Multi-Topic Communication System");
  Serial.println("WiFi: Wokwi-GUEST");
  Serial.println("MQTT Broker: broker.emqx.io");
  Serial.println("Topics:");
  Serial.println("  - incubator/heartbeat");
  Serial.println("  - incubator/sensor_readings");
  Serial.println("  - incubator/control_setpoints");
  Serial.println("  - incubator/manual_controls");
  Serial.println("  - incubator/system_status");
  Serial.println("System Ready!\n");
}

void setup_wifi() {
  delay(10);
  Serial.print("Connecting to WiFi");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("RSSI: ");
    Serial.print(WiFi.RSSI());
    Serial.println(" dBm");
  } else {
    Serial.println("\n❌ WiFi connection failed!");
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("\n📨 MQTT Message received [");
  Serial.print(topic);
  Serial.print("]: ");
  
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  Serial.println(message);
  
  processCommand(message);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    
    if (client.connect(client_id)) {
      Serial.println("✅ MQTT connected");
      client.subscribe(topic_commands);
      Serial.print("Subscribed to: ");
      Serial.println(topic_commands);
      
      // Send initial data
      sendAllData();
    } else {
      Serial.print("❌ MQTT failed, rc=");
      Serial.print(client.state());
      Serial.println(" - retrying in 5 seconds");
      delay(5000);
    }
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  
  unsigned long now = millis();
  
  // Read sensors and send data every 2 seconds
  if (now - lastMsg > 2000) {
    lastMsg = now;
    readSensors();
    readPotentiometer();
    runAutoControl();
    sendAllData();
  }
  
  // Send heartbeat every 5 seconds
  if (now - lastHeartbeat > 5000) {
    lastHeartbeat = now;
    sendHeartbeat();
  }
  
  delay(10);
}

void readPotentiometer() {
  int potValue = analogRead(POT_PIN);
  
  // Only update if value changed significantly
  if (abs(potValue - lastPotValue) > 10) {
    lastPotValue = potValue;
    
    // Map potentiometer value to temperature range (25-40°C)
    if (autoMode) {
      int newTemp = map(potValue, 0, 4095, 25, 40);
      if (newTemp != maxTemperature) {
        maxTemperature = newTemp;
        Serial.print("🎚️ Potentiometer adjusted max temp to: ");
        Serial.print(maxTemperature);
        Serial.print("°C (Pot value: ");
        Serial.print(potValue);
        Serial.println(")");
      }
    }
  }
}

void readSensors() {
  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  
  if (!isnan(temp) && !isnan(hum)) {
    temperature = temp;
    humidity = hum;
    sensorError = false;
    
    // Clamp values to reasonable ranges
    temperature = constrain(temperature, 15.0, 45.0);
    humidity = constrain(humidity, 20.0, 95.0);
    
    Serial.print("🌡️ DHT22 - Temp: ");
    Serial.print(temperature);
    Serial.print("°C, Hum: ");
    Serial.print(humidity);
    Serial.println("%");
  } else {
    sensorError = true;
    // Simulated values for testing
    static float simTemp = 25.0;
    static float simHum = 60.0;
    
    // Add some random variation for testing
    simTemp += random(-5, 5) / 10.0;
    simHum += random(-10, 10) / 10.0;
    
    simTemp = constrain(simTemp, 25.0, 35.0);
    simHum = constrain(simHum, 50.0, 80.0);
    
    temperature = simTemp;
    humidity = simHum;
    
    Serial.println("⚠️ Using simulated sensor values");
  }
}

void runAutoControl() {
  if (autoMode) {
    // Temperature control with hysteresis
    if (temperature >= maxTemperature) {
      if (!fanState) {
        digitalWrite(FAN_PIN, HIGH);
        fanState = true;
        setServoAngle(90);
        windowState = true;
        digitalWrite(LED_PIN, HIGH);
        Serial.println("🔄 AUTO: Temperature high! Cooling activated.");
      }
    } else if (temperature <= (maxTemperature - 3)) {
      if (fanState) {
        digitalWrite(FAN_PIN, LOW);
        fanState = false;
        setServoAngle(0);
        windowState = false;
        digitalWrite(LED_PIN, LOW);
        Serial.println("🔄 AUTO: Temperature normal. Cooling deactivated.");
      }
    }
    
    // Humidity control with hysteresis
    if (humidity >= maxHumidity) {
      if (!buzzerState) {
        digitalWrite(BUZZER_PIN, HIGH);
        buzzerState = true;
        Serial.println("🔄 AUTO: Humidity high! Buzzer activated.");
      }
    } else if (humidity <= (maxHumidity - 5)) {
      if (buzzerState) {
        digitalWrite(BUZZER_PIN, LOW);
        buzzerState = false;
      }
    }
  }
}

void setServoAngle(int angle) {
  angle = constrain(angle, 0, 180);
  windowServo.write(angle);
  servoPosition = angle;
  
  Serial.print("⚙️ Servo set to ");
  Serial.print(angle);
  Serial.println(" degrees");
}

void processCommand(String message) {
  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.print("❌ JSON parse error: ");
    Serial.println(error.c_str());
    return;
  }
  
  String command = doc["command"];
  
  if (command == "GET_ALL_DATA") {
    Serial.println("📤 GET_ALL_DATA command received - Sending all data");
    sendAllData();
    return;
  }
  
  if (command == "SET_MODE") {
    String mode = doc["mode"];
    if (mode == "MANUAL") {
      autoMode = false;
      Serial.println("🔄 Mode changed to MANUAL");
    } else if (mode == "AUTO") {
      autoMode = true;
      Serial.println("🔄 Mode changed to AUTO");
    }
    // Update control setpoints immediately
    sendControlSetpoints();
  }
  else if (command == "SET_TEMP") {
    maxTemperature = doc["value"];
    Serial.print("🌡️ Max temperature set to: ");
    Serial.println(maxTemperature);
    sendControlSetpoints();
  }
  else if (command == "SET_HUM") {
    maxHumidity = doc["value"];
    Serial.print("💧 Max humidity set to: ");
    Serial.println(maxHumidity);
    sendControlSetpoints();
  }
  else if (command == "FAN_ON" && !autoMode) {
    digitalWrite(FAN_PIN, HIGH);
    fanState = true;
    Serial.println("🌀 Fan turned ON (manual)");
    sendManualControls();
  }
  else if (command == "FAN_OFF" && !autoMode) {
    digitalWrite(FAN_PIN, LOW);
    fanState = false;
    Serial.println("🌀 Fan turned OFF (manual)");
    sendManualControls();
  }
  else if (command == "WINDOW_OPEN" && !autoMode) {
    setServoAngle(90);
    windowState = true;
    Serial.println("🪟 Window opened (manual)");
    sendManualControls();
  }
  else if (command == "WINDOW_CLOSE" && !autoMode) {
    setServoAngle(0);
    windowState = false;
    Serial.println("🪟 Window closed (manual)");
    sendManualControls();
  }
  else if (command == "LED_ON" && !autoMode) {
    digitalWrite(LED_PIN, HIGH);
    Serial.println("💡 LED turned ON (manual)");
    sendManualControls();
  }
  else if (command == "LED_OFF" && !autoMode) {
    digitalWrite(LED_PIN, LOW);
    Serial.println("💡 LED turned OFF (manual)");
    sendManualControls();
  }
  else if (command == "BUZZER_ON" && !autoMode) {
    digitalWrite(BUZZER_PIN, HIGH);
    buzzerState = true;
    Serial.println("🔔 Buzzer turned ON (manual)");
    sendManualControls();
  }
  else if (command == "BUZZER_OFF" && !autoMode) {
    digitalWrite(BUZZER_PIN, LOW);
    buzzerState = false;
    Serial.println("🔔 Buzzer turned OFF (manual)");
    sendManualControls();
  }
  else if (command == "REBOOT") {
    Serial.println("🔄 Reboot command received");
    ESP.restart();
  }
}

// Send all data to respective topics
void sendAllData() {
  sendCounter++;
  Serial.print("\n📤 Sending all data #");
  Serial.println(sendCounter);
  
  sendHeartbeat();
  sendSensorReadings();
  sendControlSetpoints();
  sendManualControls();
  sendSystemStatus();
}

void sendHeartbeat() {
  StaticJsonDocument<200> heartbeat;
  
  heartbeat["client_id"] = client_id;
  heartbeat["uptime"] = millis();
  heartbeat["rssi"] = WiFi.RSSI();
  heartbeat["ip_address"] = WiFi.localIP().toString();
  heartbeat["timestamp"] = millis();
  
  String jsonStr;
  serializeJson(heartbeat, jsonStr);
  
  if (client.publish(topic_heartbeat, jsonStr.c_str())) {
    Serial.println("  💓 Heartbeat sent");
  }
}

void sendSensorReadings() {
  StaticJsonDocument<200> sensor;
  
  // Get current values
  int potValue = analogRead(POT_PIN);
  int ledState = digitalRead(LED_PIN);
  
  sensor["temperature"] = round(temperature * 10) / 10.0;
  sensor["humidity"] = round(humidity * 10) / 10.0;
  sensor["potentiometer"] = potValue;
  sensor["servoPosition"] = servoPosition;
  sensor["timestamp"] = millis();
  sensor["send_counter"] = sendCounter;
  
  String jsonStr;
  serializeJson(sensor, jsonStr);
  
  if (client.publish(topic_sensor_readings, jsonStr.c_str())) {
    Serial.print("  📊 Sensor Readings: Temp=");
    Serial.print(temperature);
    Serial.print("°C, Hum=");
    Serial.print(humidity);
    Serial.print("%, Pot=");
    Serial.print(potValue);
    Serial.print(", Servo=");
    Serial.print(servoPosition);
    Serial.println("°");
  }
}

void sendControlSetpoints() {
  StaticJsonDocument<200> control;
  
  control["maxTemperature"] = maxTemperature;
  control["maxHumidity"] = maxHumidity;
  control["mode"] = autoMode ? "AUTO" : "MANUAL";
  control["timestamp"] = millis();
  control["send_counter"] = sendCounter;
  
  String jsonStr;
  serializeJson(control, jsonStr);
  
  if (client.publish(topic_control_setpoints, jsonStr.c_str())) {
    Serial.print("  🎯 Control Setpoints: MaxTemp=");
    Serial.print(maxTemperature);
    Serial.print("°C, MaxHum=");
    Serial.print(maxHumidity);
    Serial.print("%, Mode=");
    Serial.println(autoMode ? "AUTO" : "MANUAL");
  }
}

void sendManualControls() {
  StaticJsonDocument<200> manual;
  
  manual["fanRunning"] = fanState;
  manual["windowOpen"] = windowState;
  manual["ledStatus"] = digitalRead(LED_PIN);
  manual["buzzerStatus"] = buzzerState;
  manual["timestamp"] = millis();
  manual["send_counter"] = sendCounter;
  
  String jsonStr;
  serializeJson(manual, jsonStr);
  
  if (client.publish(topic_manual_controls, jsonStr.c_str())) {
    Serial.print("  🎮 Manual Controls: Fan=");
    Serial.print(fanState ? "ON" : "OFF");
    Serial.print(", Window=");
    Serial.print(windowState ? "OPEN" : "CLOSED");
    Serial.print(", LED=");
    Serial.print(digitalRead(LED_PIN) ? "ON" : "OFF");
    Serial.print(", Buzzer=");
    Serial.println(buzzerState ? "ON" : "OFF");
  }
}

void sendSystemStatus() {
  StaticJsonDocument<200> system;
  
  system["sensorStatus"] = !sensorError;
  system["wifi_rssi"] = WiFi.RSSI();
  system["free_memory"] = ESP.getFreeHeap();
  system["client_id"] = client_id;
  system["timestamp"] = millis();
  system["send_counter"] = sendCounter;
  
  String jsonStr;
  serializeJson(system, jsonStr);
  
  if (client.publish(topic_system_status, jsonStr.c_str())) {
    Serial.print("  📈 System Status: Sensor=");
    Serial.print(sensorError ? "ERROR" : "OK");
    Serial.print(", RSSI=");
    Serial.print(WiFi.RSSI());
    Serial.print("dBm, Memory=");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
  }
}
