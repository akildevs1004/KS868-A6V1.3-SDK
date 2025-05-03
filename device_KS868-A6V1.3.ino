#include <Wire.h>
#include <DHT.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <PCF8574.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>
#include <Adafruit_INA219.h>

// PCF8574 Pins
#define DOOR_SENSOR_PIN 0
#define WATER_LEAK_PIN 1
#define POWER_FAIL_PIN 2
#define FIRE_SENSOR_PIN 3
#define RESET_BUTTON_PIN 4

#define MAIN_RELAY_PIN 0
#define STATUS_LED_PIN 1
#define ALARM_BUZZER_PIN 2

// System Variables
bool isSystemGood = true;
bool countdownStarted = false;
bool resetTriggered = false;
unsigned long countdownStartTime = 0;
unsigned long resetStartTime = 0;
unsigned long lastDataSendTime = 0;
const unsigned long countdownDuration = 180000;  // 3-min door open countdown
const unsigned long resetDuration = 300000;      // 5-minute reset
const unsigned long heartbeatInterval = 1000*15;    // 15 minutes (900,000 ms)
const float TEMP_CHANGE_THRESHOLD = 0.2;         // 0.2°C minimum change
double TEMPERATURE_THRESHOLD = 28.0;

// Timing Constants
const unsigned long DOOR_TIMEOUT_MS = 180000;
const unsigned long RESET_DURATION_MS = 300000;
 long TELEMETRY_INTERVAL = 100;
const unsigned long SELF_TEST_INTERVAL = 86400000;
const float TEMP_HYSTERESIS = 0.5f;
const float MAX_CURRENT = 500.0f;

// Global Objects
PCF8574 ioOut(PCF_OUT_ADDR);
PCF8574 ioIn(PCF_IN_ADDR);
DHT dht(DHT_PIN, DHT_TYPE);
Adafruit_INA219 ina219;
WiFiClientSecure secureClient;

double TEMPERATURE_THRESHOLD = 27.0;

struct SystemState {
  struct {
    // float tempThreshold = 28.0f;
    float tempThreshold = TEMPERATURE_THRESHOLD;

    // String deviceID  = device_serial_number;  //"24000003";
    uint8_t checksum = 0;
  } config;

  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  if(TEMPERATURE_THRESHOLD==0)
  {
    TEMPERATURE_THRESHOLD = 28.0;
  }
}

  struct {
    unsigned long doorTimer = 0;
    unsigned long resetTimer = 0;
    unsigned long lastTelemetry = 0;
    unsigned long lastSelfTest = 0;
  } timers;

  // Serial.println("Divice Loop..........");
  // Read Switch 5 (Reset Button)
  int switch5 = digitalRead(SWITCH5_PIN);
  // Serial.println("deviceReadSensorsLoop------------------START-------");
  // Serial.println(switch5);
  // Serial.println(!resetTriggered);
  // Serial.print("Temperature Threshold ");
    Serial.println(TEMPERATURE_THRESHOLD);
  
  // Serial.println("deviceReadSensorsLoop-----------END-------");

SystemState alarmSystem;

void deviceSetupMethod() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Serial.println("Setup starting...");

  ioOut.begin();
  ioIn.begin();

  ioOut.pinMode(MAIN_RELAY_PIN, OUTPUT);
  ioOut.pinMode(STATUS_LED_PIN, OUTPUT);
  ioOut.pinMode(ALARM_BUZZER_PIN, OUTPUT);

  for (uint8_t i = 0; i < 8; i++) {
    ioIn.pinMode(i, INPUT);
  }

  dht.begin();
  if (!ina219.begin()) {
    Serial.println("INA219 not found at 0x40!");
  } else {
    Serial.println("INA219 initialized.");
  }

  EEPROM.begin(sizeof(alarmSystem.config));
  EEPROM.get(0, alarmSystem.config);

  WiFiManager wifiManager;
  wifiManager.autoConnect("AlarmSystem");

  ArduinoOTA.begin();

    alarmSystem.config.tempThreshold = TEMPERATURE_THRESHOLD;
}

void deviceLoopMethod() {
  ArduinoOTA.handle();

  // Read Inputs
  // alarmSystem.sensors.doorOpen = !ioIn.digitalRead(DOOR_SENSOR_PIN);
  // alarmSystem.sensors.waterLeak = !ioIn.digitalRead(WATER_LEAK_PIN);
  // alarmSystem.sensors.powerFail = !ioIn.digitalRead(POWER_FAIL_PIN);
  // alarmSystem.sensors.fire = !ioIn.digitalRead(FIRE_SENSOR_PIN);
  // alarmSystem.sensors.resetPressed = !ioIn.digitalRead(RESET_BUTTON_PIN);

  alarmSystem.sensors.doorOpen = ioIn.digitalRead(DOOR_SENSOR_PIN) == HIGH ? 0 : 1;
  alarmSystem.sensors.waterLeak = ioIn.digitalRead(WATER_LEAK_PIN) == HIGH ? 0 : 1;
  alarmSystem.sensors.powerFail = ioIn.digitalRead(POWER_FAIL_PIN) == HIGH ? 0 : 1;
  alarmSystem.sensors.fire = ioIn.digitalRead(FIRE_SENSOR_PIN) == HIGH ? 0 : 1;
  alarmSystem.sensors.resetPressed = ioIn.digitalRead(RESET_BUTTON_PIN) == HIGH ? 0 : 1;

  // Read DHT safely
  float temp = dht.readTemperature();
  if (!isnan(temp)) alarmSystem.sensors.temperature = temp;

  float hum = dht.readHumidity();
  if (!isnan(hum)) alarmSystem.sensors.humidity = hum;

  float curr = ina219.getCurrent_mA();
  if (!isnan(curr)) alarmSystem.sensors.current_mA = curr;

  // Debug print
  Serial.println("------ Sensor Readings ------");
  Serial.print("Temperature: ");
  Serial.print(alarmSystem.sensors.temperature);
  Serial.println(" °C");
  Serial.print("Humidity   : ");
  Serial.print(alarmSystem.sensors.humidity);
  Serial.println(" %");
  Serial.print("Current    : ");
  Serial.print(alarmSystem.sensors.current_mA);
  Serial.println(" mA");
  Serial.print("Door Open  : ");
  Serial.println(alarmSystem.sensors.doorOpen ? "YES" : "NO");
  Serial.print("Water Leak : ");
  Serial.println(alarmSystem.sensors.waterLeak ? "YES" : "NO");
  Serial.print("Power Fail : ");
  Serial.println(alarmSystem.sensors.powerFail ? "YES" : "NO");
  Serial.print("Fire       : ");
  Serial.println(alarmSystem.sensors.fire ? "YES" : "NO");
  Serial.print("Reset Btn  : ");
  Serial.println(alarmSystem.sensors.resetPressed ? "PRESSED" : "RELEASED");
  Serial.println("-----------------------------");

  // Handle reset
  if (alarmSystem.sensors.resetPressed) {
    alarmSystem.currentState = SystemState::State::NORMAL;
    alarmSystem.timers.resetTimer = millis();
  }

  // State logic
  if (alarmSystem.sensors.fire || alarmSystem.sensors.waterLeak || alarmSystem.sensors.powerFail || alarmSystem.sensors.current_mA > MAX_CURRENT) {
    alarmSystem.currentState = SystemState::State::CRITICAL;
  } else if (alarmSystem.sensors.temperature > alarmSystem.config.tempThreshold + TEMP_HYSTERESIS) {
    alarmSystem.currentState = SystemState::State::WARNING;
  } else if (alarmSystem.sensors.doorOpen) {
    if (alarmSystem.timers.doorTimer == 0) {
      alarmSystem.timers.doorTimer = millis();
    } else if (millis() - alarmSystem.timers.doorTimer > DOOR_TIMEOUT_MS) {
      alarmSystem.currentState = SystemState::State::WARNING;
    }
  } else {
    alarmSystem.timers.doorTimer = 0;
    if (alarmSystem.sensors.temperature < alarmSystem.config.tempThreshold - TEMP_HYSTERESIS) {
      alarmSystem.currentState = SystemState::State::NORMAL;
    }
  }

  // Outputs
  switch (alarmSystem.currentState) {
    case SystemState::State::NORMAL:
      ioOut.digitalWrite(MAIN_RELAY_PIN, LOW);
      ioOut.digitalWrite(STATUS_LED_PIN, LOW);
      ioOut.digitalWrite(ALARM_BUZZER_PIN, LOW);
      TELEMETRY_INTERVAL=1000*5;
      break;

    case SystemState::State::WARNING:
      ioOut.digitalWrite(STATUS_LED_PIN, HIGH);
      ioOut.digitalWrite(ALARM_BUZZER_PIN, HIGH);
      delay(200);
      TELEMETRY_INTERVAL=100;
      ioOut.digitalWrite(ALARM_BUZZER_PIN, LOW);
      break;

    case SystemState::State::CRITICAL:
      ioOut.digitalWrite(MAIN_RELAY_PIN, HIGH);
      ioOut.digitalWrite(STATUS_LED_PIN, HIGH);
      ioOut.digitalWrite(ALARM_BUZZER_PIN, HIGH);
      TELEMETRY_INTERVAL=100;

      break;

    case SystemState::State::MAINTENANCE:
      // Optional: Add maintenance logic
      break;
  }

  if (millis() - alarmSystem.timers.lastTelemetry > TELEMETRY_INTERVAL) {
    sendTelemetry();
    alarmSystem.timers.lastTelemetry = millis();
  }

  if (millis() - alarmSystem.timers.lastSelfTest > SELF_TEST_INTERVAL) {
    runSelfTest();
    alarmSystem.timers.lastSelfTest = millis();
  }

  delay(2000);  // Adjust this as needed
}

void sendTelemetry() {
  HTTPClient http;

  DynamicJsonDocument doc(512);

  doc["serialNumber"] = device_serial_number;  // alarmSystem.config.deviceID;
  doc["temperature"] = alarmSystem.sensors.temperature;
  doc["humidity"] = alarmSystem.sensors.humidity;
  doc["current_mA"] = alarmSystem.sensors.current_mA;
  doc["status"] = static_cast<int>(alarmSystem.currentState);
  doc["doorOpen"] = alarmSystem.sensors.doorOpen ? 1 : 0;
  doc["waterLeakage"] = alarmSystem.sensors.waterLeak ? 1 : 0;
  doc["acPowerFailure"] = alarmSystem.sensors.powerFail ? 1 : 0;
  doc["fire"] = alarmSystem.sensors.fire ? 1 : 0;
  doc["temperature_alarm"] = 0;

  if (alarmSystem.sensors.temperature > alarmSystem.config.tempThreshold) {
    doc["temperature_alarm"] = 1;
  }

  String payload;
  serializeJson(doc, payload);

 

  String jsonData = "{\"serialNumber\":\"" + String(device_serial_number) + "\",\"humidity\":\"" + String(humidity) + "\",\"temperature\":\"" + String(temperature) + "\",\"doorOpen\":\"" + String(doorOpen) + "\",\"waterLeakage\":\"" + String(waterLeakage) + "\",\"acPowerFailure\":\"" + String(acPowerFailure) + "\"}";


if(temperature >= TEMPERATURE_THRESHOLD)
{
  jsonData = "{\"serialNumber\":\"" + String(device_serial_number) + "\",\"humidity\":\"" + String(humidity) + "\",\"temperature\":\"" + String(temperature) + "\",\"doorOpen\":\"" + String(doorOpen) + "\",\"waterLeakage\":\"" + String(waterLeakage) + "\",\"acPowerFailure\":\"" + String(acPowerFailure) + "\",\"temperature_alarm\":\"1\"}";


}
  Serial.println("Sending: " + jsonData);
  http.begin(serverURL + "/alarm_device_status");
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.POST(payload);


  //Serial.println(serverURL);

  if (httpCode > 0) {
    Serial.println("HTTP Response: " + String(httpCode));
  } else {
    Serial.println("HTTP Error: " + String(httpCode));
  }
  http.end();
}

void runSelfTest() {
  ioOut.digitalWrite(STATUS_LED_PIN, HIGH);
  ioOut.digitalWrite(ALARM_BUZZER_PIN, HIGH);
  delay(300);
  ioOut.digitalWrite(STATUS_LED_PIN, LOW);
  ioOut.digitalWrite(ALARM_BUZZER_PIN, LOW);
}