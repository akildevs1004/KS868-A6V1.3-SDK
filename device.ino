
//-----------------------------------------------DEVICE SETTINGS -----------------------------------

// Pin Definitions
const int RED_LED_PIN = 2;   // Red LED
const int BUZZER_PIN = 18;   // Buzzer (active HIGH)
const int SWITCH1_PIN = 32;  // Unused (Door 2)
const int SWITCH3_PIN = 27;  // Water Leak Sensor
const int SWITCH4_PIN = 14;  // Power Failure Sensor
const int SWITCH5_PIN = 39;  // Reset Button (Active LOW)
const int DOOR_PIN = 36;     // Door Sensor
const int DHT_PIN = 13;      // DHT22 Sensor
DHT22 dht22(DHT_PIN);        // DHT22 Object

// System Variables
bool isSystemGood = true;
bool countdownStarted = false;
bool resetTriggered = false;
unsigned long countdownStartTime = 0;
unsigned long resetStartTime = 0;
unsigned long lastDataSendTime = 0;
long doorCountdownDuration = 180000;      //  3 min- door countdown
long sirenResetDuration = 300000;         // 5-minute reset
long heartbeatInterval = 5000;            // 15 minutes (900,000 ms)
const float TEMP_CHANGE_THRESHOLD = 0.2;  // 0.2°C minimum change
double TEMPERATURE_THRESHOLD = 29.0;
double HUMIDIY_THRESHOLD = 29.0;


bool temp_checkbox = false;
bool humidity_checkbox = false;
bool water_checkbox = false;
bool fire_checkbox = false;
bool power_checkbox = false;
bool door_checkbox = false;
bool siren_checkbox = false;




// Current Status Variables
int doorOpen = 0;
int waterLeakage = 0;
int acPowerFailure = 0;
float temperature = 0;
float humidity = 0;
float fire = 0;
bool alarmTeperature = false;
bool alarmHumidity = false;
bool fireDetected = false;




// Previous Status Variables
int prevDoorOpen = -1;
int prevWaterLeakage = -1;
int prevAcPowerFailure = -1;
float prevTemperature = -100;

void devicePinDefination() {
  // Initialize GPIO
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SWITCH1_PIN, INPUT_PULLUP);
  pinMode(SWITCH3_PIN, INPUT_PULLUP);
  pinMode(SWITCH4_PIN, INPUT_PULLUP);
  pinMode(SWITCH5_PIN, INPUT_PULLUP);
  pinMode(DOOR_PIN, INPUT_PULLUP);

  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  // Serial.begin(9600);

  // // WiFiManager setup
  // WiFiManagerParameter customThreshold("threshold", "Temperature Threshold", String(TEMPERATURE_THRESHOLD).c_str(), 5);
  // wifiManager.addParameter(&customThreshold);
  // wifiManager.autoConnect("XtremeGuard");
  //TEMPERATURE_THRESHOLD = atof(customThreshold.getValue());
}

void deviceReadSensorsLoop()  //loop
{


  // Read Switch 5 (Reset Button)
  int switch5 = digitalRead(SWITCH5_PIN);
  Serial.println("deviceReadSensorsLoop------------------START-------");

  Serial.println(switch5);
  Serial.println(!resetTriggered);

  Serial.println("deviceReadSensorsLoop-----------END-------");

  // Priority 1: Reset button handling
  if (switch5 == LOW && !resetTriggered) {


    Serial.println("deviceReadSensorsLoop--------------------------------------");

    resetTriggered = true;
    resetStartTime = millis();
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    sendDataToServer(true);  // Force update on reset
    delay(20);
    while (digitalRead(SWITCH5_PIN) == LOW)
      ;
  }

  // Check if reset period is active
  if (resetTriggered) {
    if (millis() - resetStartTime >= sirenResetDuration) {
      resetTriggered = false;
    } else {
      digitalWrite(RED_LED_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);
      return;
    }
  }

  // Read all sensors
  readAllSensors();

  // Check if any relevant value changed or 15-minute interval reached
  bool dataChanged = hasDataChanged();
  bool heartbeatDue = (millis() - lastDataSendTime >= heartbeatInterval);

  // Send data if changed or 15 minutes elapsed
  if (dataChanged || heartbeatDue) {
    sendDataToServer(dataChanged);
    lastDataSendTime = millis();
  }

  // Process alerts and update outputs
  processAlerts();
}


void readAllSensors() {
  temperature = dht22.getTemperature();
  humidity = dht22.getHumidity();
  doorOpen = digitalRead(DOOR_PIN) == HIGH ? 1 : 0;
  waterLeakage = digitalRead(SWITCH3_PIN) == HIGH ? 1 : 0;
  acPowerFailure = digitalRead(SWITCH4_PIN) == HIGH ? 1 : 0;
}

bool hasDataChanged() {
  // Check binary sensors first (immediate changes)
  bool binaryChanged = (doorOpen != prevDoorOpen) || (waterLeakage != prevWaterLeakage) || (acPowerFailure != prevAcPowerFailure);

  // Check temperature only if change > 0.2°C
  bool tempChanged = fabs(temperature - prevTemperature) > TEMP_CHANGE_THRESHOLD;




  // Update previous values if changed
  if (binaryChanged || tempChanged) {
    prevDoorOpen = doorOpen;
    prevWaterLeakage = waterLeakage;
    prevAcPowerFailure = acPowerFailure;
    if (tempChanged) {
      prevTemperature = temperature;
    }
    return true;
  }
  return false;
}

void processAlerts() {
  // Priority 2: System alerts
  if (digitalRead(SWITCH3_PIN) == HIGH || digitalRead(SWITCH4_PIN) == HIGH) {
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    isSystemGood = false;
  }
  // Priority 3: Temperature threshold
  else if (temperature >= TEMPERATURE_THRESHOLD) {
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    alarmTeperature = true;
  } else if (humidity >= HUMIDIY_THRESHOLD) {
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    alarmHumidity = true;
  }


  // Priority 4: Door countdown
  else if (countdownStarted && (millis() - countdownStartTime >= doorCountdownDuration)) {
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
  }
  // Default: System good
  else {
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    isSystemGood = true;
    alarmHumidity = false;
    alarmTeperature = false;
  }

  // Update door countdown state
  if (doorOpen && !countdownStarted) {
    countdownStarted = true;
    countdownStartTime = millis();
  } else if (!doorOpen) {
    countdownStarted = false;
  }
}


void sendDataToServer(bool forceSend) {

  if (serverURL != "") {
    if (String(temperature) == "-273.00" || String(temperature) == "0.00")  //ignore sensor is not installed
    {
    } else {




      // String jsonData = "{\"serialNumber\":\"" + config["device_serial_number"].as<String>() + "\",\"humidity\":\"" + String(humidity) + "\",\"temperature\":\"" + String(temperature) + "\",\"doorOpen\":\"" + String(doorOpen) + "\",\"waterLeakage\":\"" + String(waterLeakage) + "\",\"wifiipaddress\":\"" + WiFi.localIP().toString() + "\",\"wifissid\":\"" + String(WiFi.SSID()) +


      //                   "\",\"acPowerFailure\":\"" + String(acPowerFailure) + "\"}";

      StaticJsonDocument<512> doc;

      doc["serialNumber"] = device_serial_number;//config["device_serial_number"].as<String>();
      doc["wifiipaddress"] = WiFi.localIP().toString();
      doc["wifissid"] = WiFi.SSID();


      if (temp_checkbox) {
        doc["temperature"] = String(temperature);
        doc["alarm_temperature"] = String(alarmTeperature);
      }

      if (humidity_checkbox) {
        doc["humidity"] = String(humidity);
        doc["alarm_humidity"] = String(alarmHumidity);
      }


      if (water_checkbox) {
        doc["waterLeakage"] = String(waterLeakage);
      }

      if (fire_checkbox) {
        doc["fireDetected"] = String(fireDetected);
        ;
      }

      if (power_checkbox) {
        doc["acPowerFailure"] = String(acPowerFailure);
      }

      if (door_checkbox) {
        doc["doorOpen"] = String(doorOpen);
      }

      String jsonDataPost;
      serializeJson(doc, jsonDataPost);
      sensorData = jsonDataPost;

      Serial.println("Sending: " + jsonDataPost);
      http.begin(serverURL + "/alarm_device_status");
      http.addHeader("Content-Type", "application/json");
      int httpCode = http.POST(jsonDataPost);

      if (httpCode > 0) {
        Serial.println("HTTP Response: " + String(httpCode));
      } else {
        Serial.println("HTTP Error: " + String(httpCode));
      }

      http.end();
    }
  } else {
    Serial.println("Servr URL is empty");
  }
}