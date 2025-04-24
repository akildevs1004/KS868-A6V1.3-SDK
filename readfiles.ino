
String readConfig(String filename) {
  String path = "/" + filename;
  if (!LittleFS.exists(path)) {
    Serial.println("Config file not found: " + path);
    return "Config file not found:";
  }

  File file = LittleFS.open(path, "r");
  deviceConfigContent = file.readString();
  Serial.println("Config file found Success: ");

  file.close();

  deserializeJson(config, deviceConfigContent);
  //update Values from Device Config file to Program variables// for device.ino

 
  if (config.containsKey("max_temperature")) {
    TEMPERATURE_THRESHOLD = config["max_temperature"].as<double>();
  }
  // if (config.containsKey("max_humidity")) {
  //   HUMIDIY_THRESHOLD = config["max_humidity"].as<double>();
  // }
  if (config.containsKey("server_url")) {
    serverURL = config["server_url"].as<String>();
  }

  // if (config.containsKey("temp_checkbox")) {
  //   temp_checkbox = config["temp_checkbox"].as<bool>();
  // }
  // if (config.containsKey("humidity_checkbox")) {
  //   humidity_checkbox = config["humidity_checkbox"].as<bool>();
  // }
  // if (config.containsKey("water_checkbox")) {
  //   water_checkbox = config["water_checkbox"].as<bool>();
  // }
  // if (config.containsKey("fire_checkbox")) {
  //   fire_checkbox = config["fire_checkbox"].as<bool>();
  // }
  // if (config.containsKey("power_checkbox")) {
  //   power_checkbox = config["power_checkbox"].as<bool>();
  // }
  // if (config.containsKey("door_checkbox")) {
  //   door_checkbox = config["door_checkbox"].as<bool>();
  // }
  // if (config.containsKey("siren_checkbox")) {
  //   siren_checkbox = config["siren_checkbox"].as<bool>();
  // }


  // if (door_checkbox == true) {
  //   doorCountdownDuration = config["max_doorcontact"].as<long>();
  // } 
  return deviceConfigContent;
}

// Serve static files from LittleFS
String readFile(String path) {
  File file = LittleFS.open(path);
  if (!file) {
    Serial.println("Failed to open file for reading: " + path);
    return "no content in file : "+path;
  }
  String content = file.readString();
  file.close();
  return content;
}
 
void saveConfig(String filename, String data) {
 
  File file = LittleFS.open("/" + filename, FILE_READ);
  if (!file) {
    Serial.println("Failed to open file for reading: " + filename);
    return;
  }
 
  String fileContent = "";
  while (file.available()) {
    fileContent += (char)file.read();
  }
  file.close();
  DynamicJsonDocument doc(1024);  
  DeserializationError error = deserializeJson(doc, fileContent);
  if (error) {
    Serial.println("Failed to parse JSON from the file.");
    return;
  }

  // Parse the incoming data (new JSON string)
  DynamicJsonDocument newDoc(1024);  // Adjust size based on incoming JSON
  error = deserializeJson(newDoc, data);

  if (error) {
    Serial.println("Failed to parse incoming JSON data.");
    return;
  }

  // Merge or update fields from newDoc into the existing doc
  for (JsonPair p : newDoc.as<JsonObject>()) {
    doc[p.key()] = p.value();  // Update/merge the fields
  }

  // Open the file for writing (this will overwrite the file)
  file = LittleFS.open("/" + filename, FILE_WRITE);
  if (!file) {
    Serial.println("Failed to open file for writing: " + filename);
    return;
  }

  // Serialize the updated JSON back to the file
  serializeJson(doc, file);
  file.close();

  Serial.println("Updated data saved to " + filename);
}
void updateJsonConfig(String filename, String param, String value) {
  File configFile = LittleFS.open("/config.json", "r");
  if (!configFile) {
    Serial.println("Failed to open config file for reading");
    return;
  }
  StaticJsonDocument<512> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, configFile);
  if (error) {
    Serial.print("Failed to parse JSON file: ");
    Serial.println(error.c_str());
    jsonDoc.clear();  
  }
  configFile.close();  
  if (value == "true") {
    jsonDoc[param] = true;
  } else if (value == "false") {
    jsonDoc[param] = false;
  } else {
    jsonDoc[param] = value;  
  }
  configFile = LittleFS.open("/config.json", FILE_WRITE);
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return;
  }
  if (serializeJson(jsonDoc, configFile) == 0) {
    Serial.println("Failed to write to file");
  } 
  configFile.close(); 

  Serial.println(param);
  readConfig("config.json");
}
